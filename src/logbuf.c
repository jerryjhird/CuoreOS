#include <stdint.h>
#include <stdarg.h>
#include "logbuf.h"
#include <sys/types.h>
#include "stdio.h"
#include "mem/mem.h"

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
	size_t len = strlen(str);
	size_t space_until_end = LOGBUF_SIZE - write_pos;
	size_t first_chunk = (len < space_until_end) ? len : space_until_end;
	size_t second_chunk = len - first_chunk;

	memcpy(&logbuf_buffer[write_pos], str, first_chunk);

	if (second_chunk > 0) {
		memcpy(&logbuf_buffer[0], str + first_chunk, second_chunk);
	}

	write_pos = (write_pos + len) % LOGBUF_SIZE;

	if (len >= LOGBUF_SIZE) {
		read_pos = write_pos;
		wrapped = true;
	} else if (write_pos > read_pos && (write_pos - len) < read_pos) {
		read_pos = write_pos;
		wrapped = true;
	} else if (write_pos < read_pos && (write_pos + LOGBUF_SIZE - len) < read_pos) {
		read_pos = write_pos;
		wrapped = true;
	}
}

void logbuf_printf(const char *fmt, ...) {
	va_list args;

	char temp_buf[512];
	va_start(args, fmt);
	vsnprintf(temp_buf, sizeof(temp_buf), fmt, args);
	va_end(args);

	logbuf_write(temp_buf);
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
