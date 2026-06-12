#include "E9.h"

#include <stdint.h>
#include <stdbool.h>
#include "devices/registry.h"
#include "devices/types.h"
#include "cpu/io.h"

#define BOCHS_E9_PORT 0xE9

void E9_putc(char c) {
	outb(BOCHS_E9_PORT, (uint8_t)c);
}

kernel_char_dev_t e9_dev = {
	CHAR_IS_ERROR_HANDLER,
	E9_putc,
	true // true because the device works regardless of if we run E9_init. we just need E9_init for registration
};

void E9_init(void) {
	device_register(CHAR_DEV, &e9_dev);
}
