#include <stdint.h>

void ptrthex(char *buf, uint64_t val) {
	const char hex_chars[] = "0123456789ABCDEF";
	for (int i = 15; i >= 0; i--) {
		buf[i] = hex_chars[val & 0xF];
		val >>= 4;
	}
	buf[16] = 0;
}

void u64tstr(char* buf, uint64_t val) {
	char temp[21];
	int i = 0;
	do {
		temp[i++] = (val % 10) + '0';
		val /= 10;
	} while (val > 0);

	int j = 0;
	while (i > 0) buf[j++] = temp[--i];
	buf[j] = '\0';
}
