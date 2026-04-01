#include <stdint.h>
#include "logbuf.h"

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

void logbuf_vwrite(char level, const char *str) {
	logbuf_putc('\033');
	logbuf_putc(level);
	logbuf_write(str);
	logbuf_putc('\034');
}

void logbuf_vputhex(char level, uint64_t val) {
	logbuf_putc('\033');
	logbuf_putc(level);
	logbuf_puthex(val);
	logbuf_putc('\034');
}

void logbuf_vputhex64(char level, uint64_t val) {
	logbuf_putc('\033');
	logbuf_putc(level);
	logbuf_puthex64(val);
	logbuf_putc('\034');
}

void logbuf_puthex(uint64_t val) {
	const char *hex_chars = "0123456789ABCDEF";
	logbuf_putc('0');
	logbuf_putc('x');

	if (val == 0) {
		logbuf_putc('0');
		return;
	}

	bool started = false;
	for (int i = 15; i >= 0; i--) {
		uint8_t nibble = (val >> (i * 4)) & 0xF;
		if (nibble > 0 || started) {
			logbuf_putc(hex_chars[nibble]);
			started = true;
		}
	}
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

void logbuf_vputint(char level, uint64_t n) {
	logbuf_putc('\033');
	logbuf_putc(level);
	logbuf_putint(n);
	logbuf_putc('\034');
}

void logbuf_flush(kernel_dev_t *target) {
	if (!target || !target->putc) return;

	uint32_t curr = read_pos;
	bool skipping = false;

	while (curr != write_pos) {
		char c = logbuf_buffer[curr];

		if (c == '\033') { // metadata start
			uint32_t next = (curr + 1) % LOGBUF_SIZE;
			if (next == write_pos) break;

			char level = logbuf_buffer[next];
			skipping = (level == '0' && !DEV_CAP_HAS(target, CAP_ON_DEBUG));

			curr = (next + 1) % LOGBUF_SIZE;
			continue;
		}
		else if (c == '\034') {
			skipping = false;
		}
		else if (!skipping) {
			target->putc(c);
		}

		curr = (curr + 1) % LOGBUF_SIZE;
	}
}

void logbuf_clear(void) {
	read_pos = write_pos;
}
