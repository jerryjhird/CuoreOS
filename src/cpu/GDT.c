#include <stdint.h>
#include "UART16550.h"
#include "stdio.h"

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

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct trap_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gp;
static struct idt_entry idt[256];
static struct idt_ptr   idtp;

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].access      = access;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].base_high   = (uint8_t)((base >> 24) & 0xFF);
}

static void idt_set_gate(int n, uint64_t handler, uint16_t sel, uint8_t flags) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = sel;
    idt[n].ist         = 0;
    idt[n].type_attr   = flags;
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (uint32_t)(handler >> 32);
    idt[n].zero        = 0;
}

#include "devices.h"

void exception_main(struct trap_frame *tf, const char *description) {
    __asm__ volatile ("cli");

    char buf[32];
    const char *labels[] = {
        "R15", "R14", "R13", "R12", "R11", "R10", "R9 ", "R8 ",
        "RBP", "RDI", "RSI", "RDX", "RCX", "RBX", "RAX",
        "ERR", "RIP", "CS ", "FLG", "RSP", "SS "
    };

    for (size_t i = 0; i < output_devices_c; i++) {
        output_dev_t* dev = output_devices[i];

        if (DEV_CAP_HAS(dev, CAP_ON_ERROR)) {
            dev->write("\n*** CPU EXCEPTION: ");
            
            if (description) {
                size_t d_len = 0;
                while (description[d_len] != '\0' && d_len < 64) d_len++;
                dev->write(description);
            } else {
                uart16550_lwrite("Unknown", 7);
            }
            
            dev->write(" ***\n");
            uint64_t *raw_data = (uint64_t*)tf;

            for (int i = 0; i < 21; i++) {
                dev->write(labels[i]);
                dev->write(": 0x");

                ptrthex(buf, raw_data[i]);
                dev->write(buf);
                
                if (i >= 15 || i % 2 == 1) {
                    dev->write("\n");
                } else {
                    dev->write("  ");
                }
            }
        }
    }
    for(;;) {__asm__ volatile ("hlt");}
}

#define PUSH_ERR_0 "pushq $0\n\t"
#define PUSH_ERR_1 ""

#define DEF_HANDLER(index, name, has_err) \
    __attribute__((naked)) void handler_##index(void) { \
        __asm__ volatile ( \
            PUSH_ERR_##has_err \
            "pushq %%rax\n\t pushq %%rbx\n\t pushq %%rcx\n\t pushq %%rdx\n\t" \
            "pushq %%rsi\n\t pushq %%rdi\n\t pushq %%rbp\n\t pushq %%r8\n\t"  \
            "pushq %%r9\n\t  pushq %%r10\n\t pushq %%r11\n\t pushq %%r12\n\t" \
            "pushq %%r13\n\t pushq %%r14\n\t pushq %%r15\n\t" \
            "movq %%rsp, %%rdi\n\t" \
            "movabsq %0, %%rsi\n\t" \
            "andq $-16, %%rsp\n\t" \
            "call exception_main\n\t" \
            : : "i"(name) \
        ); \
    }

DEF_HANDLER(0,  "Divide By Zero", 0)
DEF_HANDLER(1,  "Debug", 0)
DEF_HANDLER(2,  "Non-Maskable Interrupt", 0)
DEF_HANDLER(3,  "Breakpoint", 0)
DEF_HANDLER(4,  "Overflow", 0)
DEF_HANDLER(5,  "Bound Range Exceeded", 0)
DEF_HANDLER(6,  "Invalid Opcode", 0)
DEF_HANDLER(7,  "Device Not Available", 0)
DEF_HANDLER(8,  "Double Fault", 1)
DEF_HANDLER(9,  "Coprocessor Segment Overrun", 0)
DEF_HANDLER(10, "Invalid TSS", 1)
DEF_HANDLER(11, "Segment Not Present", 1)
DEF_HANDLER(12, "Stack-Segment Fault", 1)
DEF_HANDLER(13, "General Protection Fault", 1)
DEF_HANDLER(14, "Page Fault", 1)
DEF_HANDLER(15, "Reserved", 0)
DEF_HANDLER(16, "x87 Floating-Point Exception", 0)
DEF_HANDLER(17, "Alignment Check", 1)
DEF_HANDLER(18, "Machine Check", 0)
DEF_HANDLER(19, "SIMD Floating-Point Exception", 0)
DEF_HANDLER(20, "Virtualization Exception", 0)
DEF_HANDLER(21, "Control Protection Exception", 1)
DEF_HANDLER(22, "Reserved", 0)
DEF_HANDLER(23, "Reserved", 0)
DEF_HANDLER(24, "Reserved", 0)
DEF_HANDLER(25, "Reserved", 0)
DEF_HANDLER(26, "Reserved", 0)
DEF_HANDLER(27, "Reserved", 0)
DEF_HANDLER(28, "Hypervisor Injection Exception", 0)
DEF_HANDLER(29, "VMM Communication Exception", 1)
DEF_HANDLER(30, "Security Exception", 1)
DEF_HANDLER(31, "Reserved", 0)

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint64_t)&gdt;

    gdt_set(0, 0, 0, 0, 0); 
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xA0); // Kernel Code
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xA0); // Kernel Data

    __asm__ volatile ("lgdt %0" : : "m"(gp) : "memory");

    __asm__ volatile (
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        : : : "rax", "memory"
    );
}

static void (*exception_handlers[])(void) = {
    handler_0, handler_1, handler_2, handler_3, handler_4, handler_5, handler_6, handler_7,
    handler_8, handler_9, handler_10, handler_11, handler_12, handler_13, handler_14, handler_15,
    handler_16, handler_17, handler_18, handler_19, handler_20, handler_21, handler_22, handler_23,
    handler_24, handler_25, handler_26, handler_27, handler_28, handler_29, handler_30, handler_31
};

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint64_t)exception_handlers[i], 0x08, 0x8E);
    }

    __asm__ volatile("lidt %0" : : "m"(idtp));
}