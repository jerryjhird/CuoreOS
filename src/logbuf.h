#ifndef LOGBUF_H
#define LOGBUF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "devices.h"

#define LOGBUF_SIZE 16384

extern char logbuf_buffer[LOGBUF_SIZE];

void logbuf_putc(char c);
void logbuf_write(const char *str);
void logbuf_puthex(uint64_t val);
void logbuf_putrawhex(uint64_t val);
void logbuf_puthex64(uint64_t val);
void logbuf_putint(uint64_t n);

/**
 * flags:
 *  - # : adds 0x prefix for %x/%p, 0b for %b
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
void logbuf_printf(const char *fmt, ...);

void logbuf_flush(kernel_char_dev_t *target);
void logbuf_clear(void);

#endif
