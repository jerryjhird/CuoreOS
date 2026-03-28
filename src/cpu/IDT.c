#include <stdint.h>
#include "UART16550.h"
#include "stdio.h"
#include "apic/lapic.h"
#include "IRQ.h"
#include "devices.h"
#include "kstate.h"

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

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

static void idt_set_gate(int n, uint64_t handler, uint16_t sel, uint8_t flags) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = sel;
    idt[n].ist         = 0;
    idt[n].type_attr   = flags;
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (uint32_t)(handler >> 32);
    idt[n].zero        = 0;
}

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
                uart16550_write("Unknown");
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

#define DEF_EXCEPTION_HANDLER(index, name, has_err) \
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

#define DEF_IRQ_HANDLER(index, target) \
    __attribute__((naked)) void irq_handler_##index(void) { \
        __asm__ volatile ( \
            "pushq %1\n\t" /* push vector index to error_code */ \
            "pushq %%rax\n\t pushq %%rbx\n\t pushq %%rcx\n\t pushq %%rdx\n\t" \
            "pushq %%rsi\n\t pushq %%rdi\n\t pushq %%rbp\n\t pushq %%r8\n\t"  \
            "pushq %%r9\n\t  pushq %%r10\n\t pushq %%r11\n\t pushq %%r12\n\t" \
            "pushq %%r13\n\t pushq %%r14\n\t pushq %%r15\n\t" \
            "movq %%rsp, %%rdi\n\t" /* pass trap_frame* */ \
            "movq %0, %%rax\n\t" /* load target */ \
            "subq $8, %%rsp\n\t" /* align stack */ \
            "call *%%rax\n\t" /* call dispatcher */ \
            "addq $8, %%rsp\n\t" /* restore stack */ \
            "popq %%r15\n\t popq %%r14\n\t popq %%r13\n\t popq %%r12\n\t" \
            "popq %%r11\n\t popq %%r10\n\t popq %%r9\n\t  popq %%r8\n\t"  \
            "popq %%rbp\n\t popq %%rdi\n\t popq %%rsi\n\t popq %%rdx\n\t" \
            "popq %%rcx\n\t popq %%rbx\n\t popq %%rax\n\t" \
            "addq $8, %%rsp\n\t" \
            "iretq\n\t" \
            : : "r"((uintptr_t)target), "i"((uint64_t)index) \
        ); \
    }

// exceptions
DEF_EXCEPTION_HANDLER(0,  "Divide By Zero", 0)
DEF_EXCEPTION_HANDLER(1,  "Debug", 0)
DEF_EXCEPTION_HANDLER(2,  "Non-Maskable Interrupt", 0)
DEF_EXCEPTION_HANDLER(3,  "Breakpoint", 0)
DEF_EXCEPTION_HANDLER(4,  "Overflow", 0)
DEF_EXCEPTION_HANDLER(5,  "Bound Range Exceeded", 0)
DEF_EXCEPTION_HANDLER(6,  "Invalid Opcode", 0)
DEF_EXCEPTION_HANDLER(7,  "Device Not Available", 0)
DEF_EXCEPTION_HANDLER(8,  "Double Fault", 1)
DEF_EXCEPTION_HANDLER(9,  "Coprocessor Segment Overrun", 0)
DEF_EXCEPTION_HANDLER(10, "Invalid TSS", 1)
DEF_EXCEPTION_HANDLER(11, "Segment Not Present", 1)
DEF_EXCEPTION_HANDLER(12, "Stack-Segment Fault", 1)
DEF_EXCEPTION_HANDLER(13, "General Protection Fault", 1)
DEF_EXCEPTION_HANDLER(14, "Page Fault", 1)
DEF_EXCEPTION_HANDLER(15, "Reserved", 0)
DEF_EXCEPTION_HANDLER(16, "x87 Floating-Point Exception", 0)
DEF_EXCEPTION_HANDLER(17, "Alignment Check", 1)
DEF_EXCEPTION_HANDLER(18, "Machine Check", 0)
DEF_EXCEPTION_HANDLER(19, "SIMD Floating-Point Exception", 0)
DEF_EXCEPTION_HANDLER(20, "Virtualization Exception", 0)
DEF_EXCEPTION_HANDLER(21, "Control Protection Exception", 1)
DEF_EXCEPTION_HANDLER(22, "Reserved", 0)
DEF_EXCEPTION_HANDLER(23, "Reserved", 0)
DEF_EXCEPTION_HANDLER(24, "Reserved", 0)
DEF_EXCEPTION_HANDLER(25, "Reserved", 0)
DEF_EXCEPTION_HANDLER(26, "Reserved", 0)
DEF_EXCEPTION_HANDLER(27, "Reserved", 0)
DEF_EXCEPTION_HANDLER(28, "Hypervisor Injection Exception", 0)
DEF_EXCEPTION_HANDLER(29, "VMM Communication Exception", 1)
DEF_EXCEPTION_HANDLER(30, "Security Exception", 1)
DEF_EXCEPTION_HANDLER(31, "Reserved", 0)

// irq's

static irq_handler_t irq_routines[256];

void irq_install_handler(uint8_t vector, irq_handler_t handler) {
    irq_routines[vector] = handler;
}

void irq_dispatch(struct trap_frame *tf) {
    uint64_t vector = tf->error_code;

    if (irq_routines[vector]) {
        irq_routines[vector](tf);
    }

    lapic_eoi();
}

// for interrupt 255
void irq_drunk_handler(struct trap_frame *tf) {
    UNUSED(tf);
}

DEF_IRQ_HANDLER(32, irq_dispatch)
DEF_IRQ_HANDLER(36, irq_dispatch)

DEF_IRQ_HANDLER(255, irq_drunk_handler)

static void (*interrupt_handlers[256])(void) = {
    // exceptions
    [0] = handler_0,   [1] = handler_1,   [2] = handler_2,   [3] = handler_3,
    [4] = handler_4,   [5] = handler_5,   [6] = handler_6,   [7] = handler_7,
    [8] = handler_8,   [9] = handler_9,   [10] = handler_10, [11] = handler_11,
    [12] = handler_12, [13] = handler_13, [14] = handler_14, [15] = handler_15,
    [16] = handler_16, [17] = handler_17, [18] = handler_18, [19] = handler_19,
    [20] = handler_20, [21] = handler_21, [22] = handler_22, [23] = handler_23,
    [24] = handler_24, [25] = handler_25, [26] = handler_26, [27] = handler_27,
    [28] = handler_28, [29] = handler_29, [30] = handler_30, [31] = handler_31,

    // IRQs
    [32] = irq_handler_32,
    [36] = irq_handler_36,
    [255] = irq_handler_255
};

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) {
        if (interrupt_handlers[i] != NULL) {
            idt_set_gate(i, (uint64_t)interrupt_handlers[i], 0x08, 0x8E); // asign interrupt to handler
        } else {
            idt[i] = (struct idt_entry){0}; // mark as not-present
        }
    }

    __asm__ volatile("lidt %0" : : "m"(idtp));
}