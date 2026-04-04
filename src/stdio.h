#pragma once

#include <stdint.h>
#include <stddef.h>

void ptrthex(char *buf, uint64_t val);
void readline(char (*getc_func)(void), void (*putc_func)(char), char* buffer, size_t limit);
