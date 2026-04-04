#include "cpu/io.h"
#include "kbd_layouts.h"
#include <stdbool.h>

static bool shift_active = false;
static bool caps_active  = false;

char ps2_getc(void) {
	if (!(inb(0x64) & 1)) return 0;

	uint8_t scancode = inb(0x60);
	if (scancode == 0xE0) return 0;

	bool released = (scancode & 0x80);
	uint8_t key_only = scancode & 0x7F;

	if (released) {
		if (key_only == 0x2A || key_only == 0x36) {
			shift_active = false;
		}
		return 0;
	}

	if (key_only == 0x2A || key_only == 0x36) {
		shift_active = true;
		return 0;
	}

	if (key_only == 0x3A) {
		caps_active = !caps_active;
		return 0;
	}

	if (key_only >= 128) return 0;

	char c;
	if (shift_active ^ caps_active) {
		c = keys_high[key_only];
	} else {
		c = keys_low[key_only];
	}

	return c;
}

void io_wait(void) {
	outb(0x80, 0);
}

// in for a rewrite
char ps2_getc_blocking(void) {
	char c = 0;
	while ((c = ps2_getc()) == 0) {
		for(int i = 0; i < 1000; i++) io_wait();
		__asm__ volatile("pause");
	}
	return c;
}
