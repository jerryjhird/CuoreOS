#include "char.h"

#include "stdio.h"

void dev_puts(kernel_char_dev_t* dev, const char* s) {
	while (*s) {
		dev->putc(*s++);
	}
}

void dev_printf(kernel_char_dev_t *dev, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vprintfcb(dev->putc, fmt, args);
	va_end(args);
}

void dev_putint(kernel_char_dev_t* dev, uint64_t n) {
	if (n == 0) {
		dev_puts(dev, "0");
		return;
	}

	char buf[21];
	int i = 20;
	buf[i--] = '\0';

	while (n > 0) {
		buf[i--] = (n % 10) + '0';
		n /= 10;
	}

	dev_puts(dev, &buf[i + 1]);
}
