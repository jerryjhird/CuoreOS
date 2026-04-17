#include "GDT.h"

#include <stdint.h>
#include "bitmask.h"
#include "drivers/UART16550.h"
#include "stdio.h"
#include "apic/lapic.h"
#include "IRQ.h"
#include "devices.h"
#include "kstate.h"
#include "cpu/smp/init.h"

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
	idt[n].selector	= sel;
	idt[n].ist		 = 0;
	idt[n].type_attr   = flags;
	idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
	idt[n].offset_high = (uint32_t)(handler >> 32);
	idt[n].zero		= 0;
}

void exception_main(struct trap_frame *tf, const char *description);

__attribute__((used))
void exception_main(struct trap_frame *tf, const char *description) {
	__asm__ volatile ("cli");

	char buf[32];
	const char *labels[] = {
		"CR2", "CR3", "CR4", "DS ", "ES ", "FS ", "GS ",
		"R15", "R14", "R13", "R12", "R11", "R10", "R9 ", "R8 ",
		"RBP", "RDI", "RSI", "RDX", "RCX", "RBX", "RAX",
		"ERR", "RIP", "CS ", "FLG", "RSP", "SS "
	};

	for (size_t i = 0; i < char_devices_c; i++) {
		kernel_char_dev_t* dev = char_devices[i];

		if (BIT_CHECK(dev->DevCAP, CHAR_DEV_CAP_ON_ERROR)) {
			dev_puts(dev, "\n*** CPU EXCEPTION: ");

			if (description) {
				dev_puts(dev, description);
			} else {
				dev_puts(dev, "UNKNOWN");
			}

			dev_puts(dev, " ***\n");

			uint64_t *raw_data = (uint64_t*)tf;

			for (int j = 0; j < 28; j++) {
				dev_puts(dev, labels[j]);
				dev_puts(dev, ": 0x");

				ptrthex(buf, raw_data[j]);
				dev_puts(dev, buf);

				dev->putc('\n');
				}
			}
		}

	for(;;) {
		__asm__ volatile ("hlt");
	}
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
			"xorq %%rax, %%rax\n\t" \
			"movw %%gs, %%ax\n\t pushq %%rax\n\t" \
			"movw %%fs, %%ax\n\t pushq %%rax\n\t" \
			"movw %%es, %%ax\n\t pushq %%rax\n\t" \
			"movw %%ds, %%ax\n\t pushq %%rax\n\t" \
			"movq %%cr4, %%rax\n\t pushq %%rax\n\t" \
			"movq %%cr3, %%rax\n\t pushq %%rax\n\t" \
			"movq %%cr2, %%rax\n\t pushq %%rax\n\t" \
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
			"pushq %1\n\t" \
			"pushq %%rax\n\t pushq %%rbx\n\t pushq %%rcx\n\t pushq %%rdx\n\t" \
			"pushq %%rsi\n\t pushq %%rdi\n\t pushq %%rbp\n\t pushq %%r8\n\t"  \
			"pushq %%r9\n\t  pushq %%r10\n\t pushq %%r11\n\t pushq %%r12\n\t" \
			"pushq %%r13\n\t pushq %%r14\n\t pushq %%r15\n\t" \
			"pushq $0\n\t pushq $0\n\t pushq $0\n\t pushq $0\n\t" \
			"pushq $0\n\t pushq $0\n\t pushq $0\n\t" \
			\
			"movq %%rsp, %%rdi\n\t" \
			"movq %%rsp, %%rbp\n\t" \
			"andq $-16, %%rsp\n\t" \
			"subq $8, %%rsp\n\t" \
			\
			"movq %0, %%rax\n\t" \
			"call *%%rax\n\t" \
			\
			"movq %%rax, %%rsp\n\t" \
			"addq $56, %%rsp\n\t" \
			\
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

void handler_0(void);
void handler_1(void);
void handler_2(void);
void handler_3(void);
void handler_4(void);
void handler_5(void);
void handler_6(void);
void handler_7(void);
void handler_8(void);
void handler_9(void);
void handler_10(void);
void handler_11(void);
void handler_12(void);
void handler_13(void);
void handler_14(void);
void handler_15(void);
void handler_16(void);
void handler_17(void);
void handler_18(void);
void handler_19(void);
void handler_20(void);
void handler_21(void);
void handler_22(void);
void handler_23(void);
void handler_24(void);
void handler_25(void);
void handler_26(void);
void handler_27(void);
void handler_28(void);
void handler_29(void);
void handler_30(void);
void handler_31(void);

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

void irq_install_handler(uint8_t logical_id, uint8_t vector, irq_handler_t handler) {
	cpu_control_block_t *target_cpu = logical_indexed_cpu_list[logical_id];
	target_cpu->routines[vector] = handler;
}

static struct trap_frame* irq_dispatch(struct trap_frame *tf) {
	uint64_t vector = tf->error_code;
	cpu_control_block_t *my_cpu; GET_CURRENT_CPU(my_cpu);

	struct trap_frame *ret_tf = tf;

	if (my_cpu->routines[vector]) {
		ret_tf = my_cpu->routines[vector](tf);
	}

	lapic_eoi();

	return ret_tf;
}

// for interrupt 255
static void irq_drunk_handler(struct trap_frame *tf) {
	UNUSED(tf);
}

#include "IRQHL.h"

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
	[32] = irq_handler_32, [33] = irq_handler_33, [34] = irq_handler_34, [35] = irq_handler_35,
	[36] = irq_handler_36, [37] = irq_handler_37, [38] = irq_handler_38, [39] = irq_handler_39,
	[40] = irq_handler_40, [41] = irq_handler_41, [42] = irq_handler_42, [43] = irq_handler_43,
	[44] = irq_handler_44, [45] = irq_handler_45, [46] = irq_handler_46, [47] = irq_handler_47,
	[48] = irq_handler_48, [49] = irq_handler_49, [50] = irq_handler_50, [51] = irq_handler_51,
	[52] = irq_handler_52, [53] = irq_handler_53, [54] = irq_handler_54, [55] = irq_handler_55,
	[56] = irq_handler_56, [57] = irq_handler_57, [58] = irq_handler_58, [59] = irq_handler_59,
	[60] = irq_handler_60, [61] = irq_handler_61, [62] = irq_handler_62, [63] = irq_handler_63,
	[64] = irq_handler_64, [65] = irq_handler_65, [66] = irq_handler_66, [67] = irq_handler_67,
	[68] = irq_handler_68, [69] = irq_handler_69, [70] = irq_handler_70, [71] = irq_handler_71,
	[72] = irq_handler_72, [73] = irq_handler_73, [74] = irq_handler_74, [75] = irq_handler_75,
	[76] = irq_handler_76, [77] = irq_handler_77, [78] = irq_handler_78, [79] = irq_handler_79,
	[80] = irq_handler_80, [81] = irq_handler_81, [82] = irq_handler_82, [83] = irq_handler_83,
	[84] = irq_handler_84, [85] = irq_handler_85, [86] = irq_handler_86, [87] = irq_handler_87,
	[88] = irq_handler_88, [89] = irq_handler_89, [90] = irq_handler_90, [91] = irq_handler_91,
	[92] = irq_handler_92, [93] = irq_handler_93, [94] = irq_handler_94, [95] = irq_handler_95,
	[96] = irq_handler_96, [97] = irq_handler_97, [98] = irq_handler_98, [99] = irq_handler_99,
	[100] = irq_handler_100, [101] = irq_handler_101, [102] = irq_handler_102, [103] = irq_handler_103,
	[104] = irq_handler_104, [105] = irq_handler_105, [106] = irq_handler_106, [107] = irq_handler_107,
	[108] = irq_handler_108, [109] = irq_handler_109, [110] = irq_handler_110, [111] = irq_handler_111,
	[112] = irq_handler_112, [113] = irq_handler_113, [114] = irq_handler_114, [115] = irq_handler_115,
	[116] = irq_handler_116, [117] = irq_handler_117, [118] = irq_handler_118, [119] = irq_handler_119,
	[120] = irq_handler_120, [121] = irq_handler_121, [122] = irq_handler_122, [123] = irq_handler_123,
	[124] = irq_handler_124, [125] = irq_handler_125, [126] = irq_handler_126, [127] = irq_handler_127,
	[128] = irq_handler_128, [129] = irq_handler_129, [130] = irq_handler_130, [131] = irq_handler_131,
	[132] = irq_handler_132, [133] = irq_handler_133, [134] = irq_handler_134, [135] = irq_handler_135,
	[136] = irq_handler_136, [137] = irq_handler_137, [138] = irq_handler_138, [139] = irq_handler_139,
	[140] = irq_handler_140, [141] = irq_handler_141, [142] = irq_handler_142, [143] = irq_handler_143,
	[144] = irq_handler_144, [145] = irq_handler_145, [146] = irq_handler_146, [147] = irq_handler_147,
	[148] = irq_handler_148, [149] = irq_handler_149, [150] = irq_handler_150, [151] = irq_handler_151,
	[152] = irq_handler_152, [153] = irq_handler_153, [154] = irq_handler_154, [155] = irq_handler_155,
	[156] = irq_handler_156, [157] = irq_handler_157, [158] = irq_handler_158, [159] = irq_handler_159,
	[160] = irq_handler_160, [161] = irq_handler_161, [162] = irq_handler_162, [163] = irq_handler_163,
	[164] = irq_handler_164, [165] = irq_handler_165, [166] = irq_handler_166, [167] = irq_handler_167,
	[168] = irq_handler_168, [169] = irq_handler_169, [170] = irq_handler_170, [171] = irq_handler_171,
	[172] = irq_handler_172, [173] = irq_handler_173, [174] = irq_handler_174, [175] = irq_handler_175,
	[176] = irq_handler_176, [177] = irq_handler_177, [178] = irq_handler_178, [179] = irq_handler_179,
	[180] = irq_handler_180, [181] = irq_handler_181, [182] = irq_handler_182, [183] = irq_handler_183,
	[184] = irq_handler_184, [185] = irq_handler_185, [186] = irq_handler_186, [187] = irq_handler_187,
	[188] = irq_handler_188, [189] = irq_handler_189, [190] = irq_handler_190, [191] = irq_handler_191,
	[192] = irq_handler_192, [193] = irq_handler_193, [194] = irq_handler_194, [195] = irq_handler_195,
	[196] = irq_handler_196, [197] = irq_handler_197, [198] = irq_handler_198, [199] = irq_handler_199,
	[200] = irq_handler_200, [201] = irq_handler_201, [202] = irq_handler_202, [203] = irq_handler_203,
	[204] = irq_handler_204, [205] = irq_handler_205, [206] = irq_handler_206, [207] = irq_handler_207,
	[208] = irq_handler_208, [209] = irq_handler_209, [210] = irq_handler_210, [211] = irq_handler_211,
	[212] = irq_handler_212, [213] = irq_handler_213, [214] = irq_handler_214, [215] = irq_handler_215,
	[216] = irq_handler_216, [217] = irq_handler_217, [218] = irq_handler_218, [219] = irq_handler_219,
	[220] = irq_handler_220, [221] = irq_handler_221, [222] = irq_handler_222, [223] = irq_handler_223,
	[224] = irq_handler_224, [225] = irq_handler_225, [226] = irq_handler_226, [227] = irq_handler_227,
	[228] = irq_handler_228, [229] = irq_handler_229, [230] = irq_handler_230, [231] = irq_handler_231,
	[232] = irq_handler_232, [233] = irq_handler_233, [234] = irq_handler_234, [235] = irq_handler_235,
	[236] = irq_handler_236, [237] = irq_handler_237, [238] = irq_handler_238, [239] = irq_handler_239,
	[240] = irq_handler_240, [241] = irq_handler_241, [242] = irq_handler_242, [243] = irq_handler_243,
	[244] = irq_handler_244, [245] = irq_handler_245, [246] = irq_handler_246, [247] = irq_handler_247,
	[248] = irq_handler_248, [249] = irq_handler_249, [250] = irq_handler_250, [251] = irq_handler_251,
	[252] = irq_handler_252, [253] = irq_handler_253, [254] = irq_handler_254, [255] = irq_handler_255
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
