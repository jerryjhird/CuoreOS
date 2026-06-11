#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "device/char.h"
#include "abs.h"

#define LOGBUF_SIZE 16384

extern char logbuf_buffer[LOGBUF_SIZE];

void logbuf_putc(char c);
void logbuf_write(const char *str);

// see formatting instructions in stdio.h
void logbuf_vprintf(const char *fmt, va_list args);
void logbuf_printf(const char *fmt, ...);

void logbuf_flush(kernel_char_dev_t *target);
void logbuf_clear(void);

#ifdef DEBUG
	#define logbuf_debug(fmt, ...) logbuf_printf("(" ANSI_HI_BLACK " DEBUG " ANSI_RESET ") " fmt __VA_OPT__(,) __VA_ARGS__)
#else
	#define logbuf_debug(fmt, ...) ((void)0)
#endif

#define logbuf_info(fmt, ...) logbuf_printf("(" ANSI_REG_CYAN	 " INFO  " ANSI_RESET ") " fmt __VA_OPT__(,) __VA_ARGS__)
#define logbuf_ok(fmt, ...) logbuf_printf("(" ANSI_BOLD_GREEN	"  OK   " ANSI_RESET ") " fmt __VA_OPT__(,) __VA_ARGS__)
#define logbuf_warn(fmt, ...) logbuf_printf("(" ANSI_BOLD_YELLOW " WARN  " ANSI_RESET ") " fmt __VA_OPT__(,) __VA_ARGS__)
#define logbuf_error(fmt, ...) logbuf_printf("(" ANSI_BOLD_RED	 " ERROR " ANSI_RESET ") " fmt __VA_OPT__(,) __VA_ARGS__)
#define logbuf_critical(fmt, ...) logbuf_printf("(" ANSI_BG_RED ANSI_BOLD_WHITE " CRIT  " ANSI_RESET ") " fmt __VA_OPT__(,) __VA_ARGS__)
