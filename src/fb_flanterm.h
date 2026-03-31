#pragma once

#include <stdint.h>
#include <stddef.h>
#include "limine.h"

void _c_flanterm_init(struct limine_framebuffer *fb);
void _c_flanterm_free();
void _c_flanterm_putc(char c);
