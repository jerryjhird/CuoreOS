#pragma once

#include <stdint.h>
#include <stddef.h>
#include "graphics/fb.h"
#include "device/char.h"

void _c_flanterm_init(linear_framebuffer_t *fb);
void _c_flanterm_free(void);
void _c_flanterm_putc(char c);

extern kernel_char_dev_t flanterm_dev;
