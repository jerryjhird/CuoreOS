#pragma once

#include "device/char.h"

void E9_init(void);
void E9_putc(char c);

extern kernel_char_dev_t e9_dev;
