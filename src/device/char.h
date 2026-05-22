#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

void dev_puts(kernel_char_dev_t* dev, const char* s);
void dev_putint(kernel_char_dev_t* dev, uint64_t n);
void dev_printf(kernel_char_dev_t *dev, const char *fmt, ...);

extern kernel_char_dev_t uart16550_dev;
extern kernel_char_dev_t flanterm_dev;
