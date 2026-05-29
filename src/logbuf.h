#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "device/char.h"

#define LOGBUF_SIZE 16384

extern char logbuf_buffer[LOGBUF_SIZE];

void logbuf_putc(char c);
void logbuf_write(const char *str);

// see formatting instructions in stdio.h
void logbuf_vprintf(const char *fmt, va_list args);
void logbuf_printf(const char *fmt, ...);

void logbuf_flush(kernel_char_dev_t *target);
void logbuf_clear(void);

#define LOGBUF_WARN(fmt, ...) logbuf_printf(ANSI_BOLD_YELLOW "[ WARN ] " ANSI_RESET fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGBUF_ERROR(fmt, ...) logbuf_printf(ANSI_BOLD_RED "[ ERR  ] " ANSI_RESET fmt __VA_OPT__(,) __VA_ARGS__)
