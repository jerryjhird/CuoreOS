/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "stdint.h"
#include "time.h"
#include "x86.h"
#include "string.h"
#include "memory.h"

#include "serial.h"
#include "GDT.h"

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

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
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

struct tm gettm(void) {
    struct tm tm;

    while (cmos_read(0x0A) & 0x80);

    int sec   = bcdtbin(cmos_read(0x00));
    int min   = bcdtbin(cmos_read(0x02));
    int hour  = bcdtbin(cmos_read(0x04));
    int mday  = bcdtbin(cmos_read(0x07));
    int mon   = bcdtbin(cmos_read(0x08)); // 1–12
    int year  = 2000 + bcdtbin(cmos_read(0x09));

    tm.tm_sec  = sec;
    tm.tm_min  = min;
    tm.tm_hour = hour;
    tm.tm_mday = mday;
    tm.tm_mon  = mon - 1;          // 0–11
    tm.tm_year = year - 1900;      // years since 1900
    tm.tm_isdst = 0;

    int y = year;
    int m = mon;

    if (m <= 2) {
        m += 12;
        y -= 1;
    }

    int32_t days =
        365*y + y/4 - y/100 + y/400 +
        (153*m + 8)/5 +
        mday - 719469;

    // 1970/01/01 was a thursday prob
    tm.tm_wday = (days + 4) % 7;
    if (tm.tm_wday < 0)
        tm.tm_wday += 7;

    static const int yday_tab[12] = {
        0,   31,  59,  90, 120, 151,
        181, 212, 243, 273, 304, 334
    };

    tm.tm_yday = yday_tab[tm.tm_mon] + (mday - 1);

    // leap year correction
    int leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (leap && tm.tm_mon > 1)
        tm.tm_yday++;

    return tm;
}

uint32_t tm_to_epoch(const struct tm *tm)
{
    int year  = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;
    int day   = tm->tm_mday;
    int hour  = tm->tm_hour;
    int min   = tm->tm_min;
    int sec   = tm->tm_sec;

    if (month <= 2) {
        month += 12;
        year -= 1;
    }

    int32_t days =
        365*year + year/4 - year/100 + year/400 +
        (153*month + 8)/5 +
        day - 719469;

    return (uint32_t)(
        days * 86400 +
        hour * 3600 +
        min * 60 +
        sec
    );
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


#define IDT_ENTRIES 256

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;       // bits 0-2 = IST, rest zero
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

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

// helper for printing registers to serial
__attribute__((unused))
void regtserial(uint64_t *regs) {
    char buf[32];
    const char *names[] = {
        "RAX", "RBX", "RCX", "RDX",
        "RSI", "RDI", "RBP", "R8",
        "R9",  "R10", "R11", "R12",
        "R13", "R14", "R15"
    };

    for (int i = 0; i < 15; i++) {
        serial_write(names[i], 3);
        serial_write(": 0x", 4);
        ptrhex(buf, (void*)regs[i]);
        serial_write(buf, sizeof(void*)*2);
        serial_write("\n", 1);
    }
}

// register dumping for c environment
void dumpreg(void) {
    uint64_t regs[15];
    __asm__ volatile(
        "mov %%rax, %0\n\t"
        "mov %%rbx, %1\n\t"
        "mov %%rcx, %2\n\t"
        "mov %%rdx, %3\n\t"
        "mov %%rsi, %4\n\t"
        "mov %%rdi, %5\n\t"
        "mov %%rbp, %6\n\t"
        "mov %%r8,  %7\n\t"
        "mov %%r9,  %8\n\t"
        "mov %%r10, %9\n\t"
        "mov %%r11, %10\n\t"
        "mov %%r12, %11\n\t"
        "mov %%r13, %12\n\t"
        "mov %%r14, %13\n\t"
        "mov %%r15, %14\n\t"
        : "=m"(regs[0]), "=m"(regs[1]), "=m"(regs[2]), "=m"(regs[3]),
          "=m"(regs[4]), "=m"(regs[5]), "=m"(regs[6]), "=m"(regs[7]),
          "=m"(regs[8]), "=m"(regs[9]), "=m"(regs[10]), "=m"(regs[11]),
          "=m"(regs[12]), "=m"(regs[13]), "=m"(regs[14])
        :
        : "memory"
    );
    regtserial(regs);
}


__attribute__((naked))
void generic_exception_handler(void) {
    __asm__ volatile(
        "push %rax\n\t"
        "push %rbx\n\t"
        "push %rcx\n\t"
        "push %rdx\n\t"
        "push %rsi\n\t"
        "push %rdi\n\t"
        "push %rbp\n\t"
        "push %r8\n\t"
        "push %r9\n\t"
        "push %r10\n\t"
        "push %r11\n\t"
        "push %r12\n\t"
        "push %r13\n\t"
        "push %r14\n\t"
        "push %r15\n\t"

        "lea panic_str(%rip), %rdi\n\t"   // pointer to string
        "mov $18, %rsi\n\t"               // length
        "call serial_write\n\t"

        // dump registers
        "mov %rsp, %rdi\n\t"
        "call regtserial\n\t"

        // Halt
        "jmp haltjmp\n\t"

        "haltjmp:\n\t"
        "hlt\n\t"
        "jmp haltjmp\n\t"

        "panic_str: .ascii \"CPU EXCEPTION!!!!\\n\""
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