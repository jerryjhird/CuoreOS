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
void logbuf_puthex64(uint64_t val);
void logbuf_putint(uint64_t n);

void logbuf_flush(kernel_char_dev_t *target);
void logbuf_clear(void);

#endif
