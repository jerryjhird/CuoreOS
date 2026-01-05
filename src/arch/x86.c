#include "stdint.h"
#include "stdio.h"
#include "time.h"
#include "arch/x86_64.h"
#include "string.h"
#include "memory.h"

#include "drivers/serial.h"

uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

// reads 64 bit timestamp counter
uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

uint8_t bcdtbin(uint8_t val) { // bcd to binary
    return (uint8_t)((val & 0x0F) + ((val >> 4) * 10));
}

void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx),
                       "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}

void halt(void) {
    for (;;) __asm__ volatile("hlt");
}

datetime_t getdatetime(void) {
    datetime_t dt;

    while (cmos_read(0x0A) & 0x80); // wait until update-in-progress clear

    dt.sec   = bcdtbin(cmos_read(0x00));
    dt.min   = bcdtbin(cmos_read(0x02));
    dt.hour  = bcdtbin(cmos_read(0x04));
    dt.day   = bcdtbin(cmos_read(0x07));
    dt.month = bcdtbin(cmos_read(0x08));
    dt.year  = 2000 + bcdtbin(cmos_read(0x09));

    return dt;
}

uint32_t dttepoch(datetime_t dt) { // time.h 
    uint16_t year = dt.year;
    uint8_t month = dt.month;
    uint8_t day = dt.day;
    uint8_t hour = dt.hour;
    uint8_t min = dt.min;
    uint8_t sec = dt.sec;

    if (month <= 2) {
        month += 12;
        year -= 1;
    }

    int32_t days_signed = 365*year + year/4 - year/100 + year/400 + (153*month + 8)/5 + day - 719469;
    uint32_t days = (uint32_t)days_signed;
    return days*86400 + hour*3600 + min*60 + sec;
}

void sleep_ms(uint64_t ms, uint64_t cpu_hz) { // time.h
    uint64_t start = rdtsc();
    uint64_t cycles_to_wait = (cpu_hz / 1000) * ms;
    while ((rdtsc() - start) < cycles_to_wait) {
        __asm__ volatile("pause"); // prevents busy-wait from hammering the CPU
    }
}

char* cpu_brand(void) {
    uint32_t eax, ebx, ecx, edx;

    // allocate 49 bytes
    char *brand = zalloc(49);
    char *p = brand;

    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
        cpuid(leaf, 0, &eax, &ebx, &ecx, &edx);

        // copy 4 registers (16 bytes) per leaf
        memcpy(p, &eax, 4); p += 4;
        memcpy(p, &ebx, 4); p += 4;
        memcpy(p, &ecx, 4); p += 4;
        memcpy(p, &edx, 4); p += 4;
    }

    brand[48] = '\0';

    // trim
    int end = 47;
    while (end >= 0 && (brand[end] == ' ' || brand[end] == '\0' || brand[end] == '\n' || brand[end] == '\r'))
        brand[end--] = '\0';

    return brand;
}

// GDT

static struct gdt_entry gdt[3];
static struct gdt_ptr   gp;

// helper to fill a descriptor
static void gdt_set(
    int i, uint32_t base, uint32_t limit,
    uint8_t access, uint8_t gran)
{
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].access      = access;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].base_high   = (uint8_t)((base >> 24) & 0xFF);
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint64_t)&gdt;
    gdt_set(0, 0, 0, 0, 0);
    gdt_set(1, 0, 0, 0x9A, 0xA0); // kernel code
    gdt_set(2, 0, 0, 0x92, 0xA0); // kernel data

    // load gdt
    __asm__ volatile (
        "lgdt %0\n\t"
        :
        : "m"(gp)
        : "memory"
    );

    __asm__ volatile (
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"

        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        :
        :
        : "rax", "memory"
    );
}

// IDT

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

// helper to fill a idt entry
static void idt_set_gate(int n, uint64_t handler, uint16_t sel, uint8_t flags, uint8_t ist) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = sel;
    idt[n].ist         = ist & 0x7;
    idt[n].type_attr   = flags;
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[n].zero        = 0;
}

void generic_exception_handler_main(void* regs __attribute__((unused))) {
    serial_write("CPU Exception\n", 14);
}


__attribute__((naked))
void generic_exception_handler(void)
{
    __asm__ volatile(
        "cli\n\t"
        "call generic_exception_handler_main\n\t"
        "hlt\n\t"
    );
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    // use the generic_exception handler for all exceptions
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, (uint64_t)generic_exception_handler, 0x08, 0x8E, 0);
    }

    __asm__ volatile("lidt %0" : : "m"(idtp));
}

// SSE 

int sse_init(void) {
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    if (!(edx & (1U << 25))) {
        return -1; // no SSE support
    }

    // CR0
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);   // clear EM
    cr0 |=  (1ULL << 1);   // set MP
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));

    // CR4
    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10); // OSFXSR | OSXMMEXCPT
    __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));

    return 0;
}

__attribute__((noinline, optimize("O0")))
uint32_t crc32c_hwhash(const char *s) {
    unsigned int h = 0;
    while (*s) {
        unsigned char c = (unsigned char)(*s++);
        __asm__ volatile ("crc32b %1, %0" : "+r"(h) : "rm"(c));
    }
    return h;
}

// switch stack and jmp
__attribute__((noreturn))
void swstack_jmp(void *new_sp, void (*entry)(void)) {
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "jmp *%1\n"
        :
        : "r"(new_sp), "r"(entry)
        : "memory"
    );
    __builtin_unreachable();
}