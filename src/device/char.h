#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include "abs.h"

void dev_puts(kernel_char_dev_t* dev, const char* s);
void dev_printf(kernel_char_dev_t *dev, const char *fmt, ...);

#ifdef DEBUG
	#define dev_log_debug(dev, fmt, ...) dev_printf(dev, "(" ANSI_HI_BLACK " DEBUG " ANSI_RESET ") " fmt, ##__VA_ARGS__)
#else
	#define dev_log_debug(dev, fmt, ...) ((void)0)
#endif

#define dev_log_ok(dev, fmt, ...) dev_printf(dev, "(" ANSI_BOLD_GREEN	"  OK   " ANSI_RESET ") ", ##__VA_ARGS__)
#define dev_log_info(dev, fmt, ...) dev_printf(dev, "(" ANSI_REG_CYAN	" INFO  " ANSI_RESET ") ", ##__VA_ARGS__)
#define dev_log_warn(dev, fmt, ...) dev_printf(dev, "(" ANSI_BOLD_YELLOW " WARN  " ANSI_RESET ") ", ##__VA_ARGS__)
#define dev_log_error(dev, fmt, ...) dev_printf(dev, "(" ANSI_BOLD_RED   " ERROR " ANSI_RESET ") ", ##__VA_ARGS__)
#define dev_log_critical(dev, fmt, ...) dev_printf(dev, "(" ANSI_BG_RED ANSI_BOLD_WHITE "	 CRIT	 " ANSI_RESET ") ", ##__VA_ARGS__)

extern kernel_char_dev_t* debug_dev;
