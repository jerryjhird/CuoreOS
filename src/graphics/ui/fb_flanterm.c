#include "flanterm.c"
#include "flanterm_backends/fb.c"
#include "flanterm.h"
#include "flanterm_backends/fb.h"

#include "fb_flanterm.h"

#include "graphics/fb.h"
#include "devices.h"
#include "mem/heap.h"
#include <stdint.h>
#include "abs.h"

struct flanterm_context *ft_ctx = NULL;

kernel_char_dev_t flanterm_dev = {
	CHAR_DEV_CAP_ON_ERROR,
	_c_flanterm_putc,
	false
};

void _c_flanterm_init(linear_framebuffer_t *fb) {
	char_devices[char_devices_c++] = &flanterm_dev;

	ft_ctx = flanterm_fb_init(
		malloc,
		sfree,
		fb->address,
		fb->width,
		fb->height,
		fb->pitch,
		fb->pixelf.red_size,
		fb->pixelf.red_shift,
		fb->pixelf.green_size,
		fb->pixelf.green_shift,
		fb->pixelf.blue_size,
		fb->pixelf.blue_shift,
		NULL,
		NULL, NULL,
		NULL, NULL,
		NULL, NULL,
		NULL,
		0, 0,
		1,
		0, 0,
		0,
		0
	);
	flanterm_dev.initialized = true;
}

void _c_flanterm_free(void) {
	flanterm_deinit(ft_ctx, sfree);
	flanterm_dev.initialized = false;
}

void _c_flanterm_putc(char c) {
	if (UNLIKELY(!ft_ctx)) return;

	flanterm_write(ft_ctx, &c, 1);

	if (c == '\n') {
		char r = '\r';
		flanterm_write(ft_ctx, &r, 1);
	}
}
