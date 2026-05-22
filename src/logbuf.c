#include <stdint.h>
#include <stdarg.h>
#include "logbuf.h"
#include <sys/types.h>
#include "stdio.h"

char logbuf_buffer[LOGBUF_SIZE];

static uint32_t write_pos = 0;
static uint32_t read_pos = 0;
static bool wrapped = false;

void logbuf_putc(char c) {
	logbuf_buffer[write_pos] = c;
	write_pos = (write_pos + 1) % LOGBUF_SIZE;

	if (write_pos == read_pos) {
		read_pos = (read_pos + 1) % LOGBUF_SIZE;
		wrapped = true;
	}
}

void logbuf_write(const char *str) {
	for (size_t i = 0; str[i] != '\0'; i++) {
		logbuf_putc(str[i]);
	}
}

void logbuf_putrawhex(uint64_t val) {
	const char *hex_chars = "0123456789ABCDEF";
	bool started = false;

	for (int i = 15; i >= 0; i--) {
		uint8_t nibble = (val >> (i * 4)) & 0xF;

		if (nibble > 0) {
			started = true;
		}

		if (started) {
			logbuf_putc(hex_chars[nibble]);
		}
	}

	if (!started) {
		logbuf_putc('0');
	}
}

void logbuf_puthex(uint64_t val) {
	logbuf_putc('0');
	logbuf_putc('x');
	logbuf_putrawhex(val);
}

void logbuf_puthex64(uint64_t val) {
	const char *hex_chars = "0123456789ABCDEF";
	logbuf_putc('0');
	logbuf_putc('x');

	for (int i = 15; i >= 0; i--) {
		logbuf_putc(hex_chars[(val >> (i * 4)) & 0xF]);
	}
}

void logbuf_putint(uint64_t n) {
	if (n == 0) {
		logbuf_write("0");
		return;
	}

	char buf[21];
	int i = 20;
	buf[i--] = '\0';

	while (n > 0) {
		buf[i--] = (n % 10) + '0';
		n /= 10;
	}

	logbuf_write(&buf[i + 1]);
}

void logbuf_printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vprintfcb(logbuf_putc, fmt, args);
	va_end(args);
}

void logbuf_flush(kernel_char_dev_t *target) {
	if (!target || !target->putc) return;

	uint32_t curr = read_pos;

	while (curr != write_pos) {
		char c = logbuf_buffer[curr];
		target->putc(c);
		curr = (curr + 1) % LOGBUF_SIZE;
	}
}

void logbuf_clear(void) {
	read_pos = write_pos;
}
