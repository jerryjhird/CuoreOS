#include "stdio.h"

#include <stdint.h>
#include <stddef.h>

void ptrthex(char *buf, uint64_t val) {
	const char hex_chars[] = "0123456789ABCDEF";
	for (int i = 15; i >= 0; i--) {
		buf[i] = hex_chars[val & 0xF];
		val >>= 4;
	}
	buf[16] = 0;
}

void byteswap_str(char* str, size_t len) {
	for (size_t i = 0; i < len; i += 2) {
		char tmp = str[i];
		str[i] = str[i + 1];
		str[i + 1] = tmp;
	}
}

/**
 * @param getc_func  pointer to a blocking getc
 * @param putc_func  pointer to putc (to echo back characters) if NULL echo is disabled.
 * @param buffer	 where to return the string to
 * @param limit		 max length of the string including NULL terminator
 */
void readline(char (*getc_func)(void), void (*putc_func)(char), char* buffer, size_t limit) {
	size_t index = 0;

	while (index < limit - 1) {
		char c = getc_func();

		if (c == '\n' || c == '\r') {
			if (putc_func) putc_func('\n');
			break;
		}

		if (c == '\b' || c == 0x7F) {
			if (index > 0) {
				index--;
				if (putc_func) {
					putc_func('\b');
					putc_func(' ');
					putc_func('\b');
				}
			}
			continue;
		}

		if (c >= 32 && c <= 126) {
			buffer[index++] = c;
			if (putc_func) putc_func(c);
		}
	}

	if (index == limit - 1 && putc_func) {
		putc_func('\n');
	}

	buffer[index] = '\0';
}
