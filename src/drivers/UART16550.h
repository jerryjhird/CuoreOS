#pragma once

#include "device/char.h"

void uart16550_init(void);
void uart16550_init_late(void);
char uart16550_getc(void);
void uart16550_putc(char c);

extern kernel_char_dev_t uart16550_dev;
