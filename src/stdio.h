#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void ptrthex(char *buf, uint64_t val);
void readline(char (*getc_func)(void), void (*putc_func)(char), char* buffer, size_t limit);

typedef void (*putc_t)(char c);

/**
 * flags:
 *  - # : adds 0x prefix for %x, 0b for %b but %p always includes 0x
 *  - 0 : zero padding
 *  - - : left alignment
 *  - + : prefaces positive numbers with +
 *  -   : prefaces positive numbers with a space.
 *
 * length modifiers:
 *  - hh  : argument is (unsigned) char.
 *  - h   : argument is (unsigned) short.
 *  - l   : argument is (unsigned) long.
 *  - ll  : argument is (unsigned) long long.
 *  - z   : argument is size_t.
 *  - j   : argument is intmax_t.
 *  - t   : argument is ptrdiff_t.
 *
 *  - d, i: signed decimal integer.
 *  - u : unsigned decimal integer.
 *  - x : unsigned hex
 *  - p : pointer.
 *  - s : null terminated str
 *  - c : single char
 *  - %
 *
 *  - b : binary representation
 *  - I : ipv4 addr (expects uint8_t[4])
 *  - M : mac addr (expects uint8_t[6])
 */
 void vprintfcb(putc_t putc, const char *fmt, va_list args);
