#pragma once

#include <stddef.h>
#include <stdint.h>

void uart16550_init(void);
void uart16550_postinit(void);

char uart16550_getc(void);
void uart16550_putc(char c);
void uart16550_puthex(uint64_t val);
void uart16550_write(const char* str);