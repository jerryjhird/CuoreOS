#ifndef logbuf_H
#define logbuf_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "device/char.h"

#define LOGBUF_SIZE 16384

extern char logbuf_buffer[LOGBUF_SIZE];

void logbuf_putc(char c);
void logbuf_write(const char *str);
void logbuf_printf(const char *fmt, ...); // see formatting instructions in stdio.h

void logbuf_flush(kernel_char_dev_t *target);
void logbuf_clear(void);

#endif
