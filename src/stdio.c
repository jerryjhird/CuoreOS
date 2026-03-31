#include <stdint.h>

void ptrthex(char *buf, uint64_t val) {
	const char hex_chars[] = "0123456789ABCDEF";
	for (int i = 15; i >= 0; i--) {
		buf[i] = hex_chars[val & 0xF];
		val >>= 4;
	}
	buf[16] = 0;
}