#include "PS2.h"

#include "cpu/io.h"
#include "kbd_layouts.h"
#include <stdbool.h>
#include "_time.h"

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

char ps2_getc_blocking(void) {
	char c;

	if ((c = ps2_getc())) return c;

	while (1) {
		usleep(100);

		if ((c = ps2_getc())) {
			return c;
		}

		__asm__ volatile("pause");
	}
}
