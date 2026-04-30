#pragma once

#include <stdint.h>
#include <stddef.h>
#include "graphics/fb.h"

void _c_flanterm_init(linear_framebuffer_t *fb);
void _c_flanterm_free(void);
void _c_flanterm_putc(char c);
