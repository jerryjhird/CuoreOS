#include "char.h"

#include "stdio.h"

void dev_puts(kernel_char_dev_t* dev, const char* s) {
	while (*s) {
		dev->putc(*s++);
	}
}

void dev_printf(kernel_char_dev_t *dev, const char *fmt, ...) {
	if (!dev || !dev->putc) return;

	char temp_buf[1028];
	va_list args;

	va_start(args, fmt);
	vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
	va_end(args);

	dev_puts(dev, temp_buf);
}
