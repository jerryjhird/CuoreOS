#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void ptrthex(char *buf, uint64_t val);
uint64_t strtoull(const char* nptr);

void readline(char (*getc_func)(void), void (*putc_func)(char), char* buffer, size_t limit);

/**
 * flags:
 *  - # : adds 0x prefix for %x/%X, 0b for %b; %p always includes 0x
 *  - 0 : zero padding
 *  - - : left align
 *  - + : prefix positive numbers with +
 *  -   : prefix positive numbers with a space
 *
 * length modifiers:
 *  - hh : (unsigned) char
 *  - h  : (unsigned) short
 *  - l  : (unsigned) long
 *  - ll : (unsigned) long long
 *  - z  : size_t / ssize_t
 *  - j  : intmax_t / uintmax_t
 *  - t  : ptrdiff_t
 *
 * specifiers:
 *  - d, i : signed decimal
 *  - u	: unsigned decimal
 *  - x, X : unsigned hex (lower/upper)
 *  - o	: unsigned octal
 *  - p	: pointer (0x-prefixed hex)
 *  - s	: null-terminated string
 *  - c	: single char
 *  - b	: binary
 *  - I	: IPv4 address (uint8_t[4])
 *  - M	: MAC address (uint8_t[6])
 *  - %%   : literal %
 *
 */

int vsnprintf(char *buf, size_t bufsz, const char *fmt, va_list args);
int snprintf(char *buf, size_t bufsz, const char *fmt, ...);
