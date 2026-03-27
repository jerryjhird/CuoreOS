#pragma once

#include <stdint.h>
#include <stddef.h>

void _c_flanterm_init(struct limine_framebuffer *fb);
void _c_flanterm_free();

void _c_flanterm_putc(char c);
void _c_flanterm_lwrite(const char *msg, size_t len);
void _c_flanterm_write(const char *str);
void _c_flanterm_puthex(uint64_t val);
