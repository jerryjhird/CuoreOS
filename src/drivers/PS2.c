#include "cpu/io.h"
#include "kbd_layouts.h"
#include <stdbool.h>

static bool shift_active = false;
static bool caps_active  = false;

char ps2_getc(void) {
	if (!(inb(0x64) & 1)) return 0;

	uint8_t scancode = inb(0x60);

	if (scancode == 0x2A || scancode == 0x36) {
		shift_active = true;
		return 0;
	}
	if (scancode == 0xAA || scancode == 0xB6) {
		shift_active = false;
		return 0;
	}

	if (scancode == 0x3A) {
		caps_active = !caps_active;
		return 0;
	}

	if (scancode & 0x80) return 0;

	if (shift_active ^ caps_active) {
		return keys_high[scancode];
	} else {
		return keys_low[scancode];
	}
}
