#ifndef LOGBUF_H
#define LOGBUF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "devicetypes.h"

#define LOGBUF_SIZE 16384

#define LOG_LEVEL_DEBUG '0'
#define LOG_TAG(lvl) "\033" lvl

extern char logbuf_buffer[LOGBUF_SIZE];

void logbuf_putc(char c);

void logbuf_write(const char *str);
void logbuf_puthex(uint64_t val);
void logbuf_puthex64(uint64_t val);

void logbuf_vwrite(char level, const char *str);
void logbuf_vputhex(char level, uint64_t val);
void logbuf_vputhex64(char level, uint64_t val);

void logbuf_putint(uint64_t n);
void logbuf_vputint(char level, uint64_t n);

void logbuf_flush(kernel_dev_t *target);
void logbuf_clear(void);

#endif
