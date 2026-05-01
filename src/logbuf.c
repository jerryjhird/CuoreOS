#include <stdint.h>
#include <stdarg.h>
#include "logbuf.h"
#include "devices.h"
#include "bitmask.h"
#include <sys/types.h>

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

#define FLAG_ALT (1 << 0) // #
#define FLAG_ZERO (1 << 1) // 0
#define FLAG_LEFT (1 << 2) // -
#define FLAG_SPACE (1 << 3) // ' '
#define FLAG_SIGN (1 << 4) // +

#define FLAG_ZERO_PAD (1 << 0)
#define FLAG_LEFT_JUSTIFY (1 << 1)
#define FLAG_PLUS (1 << 2)
#define FLAG_SPACE (1 << 3)
#define FLAG_HASH (1 << 4)
#define FLAG_UPPERCASE (1 << 5)

enum {
	LEN_DEFAULT,
	LEN_HH,
	LEN_H,
	LEN_L,
	LEN_LL,
	LEN_Z,
	LEN_J,
	LEN_T
};

static void logbuf_printf_number_h(uint64_t val, int base, int width, uint8_t flags, bool uppercase) {
	char buf[65];
	const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
	int i = 0;

	if (val == 0) {
		buf[i++] = '0';
	} else if (base == 16) {
		while (val > 0) {
			buf[i++] = digits[val & 0xF];
			val >>= 4;
		}
	} else if (base == 10) {
		while (val > 0) {
			uint64_t q = (uint64_t)(((unsigned __int128)val * 0xCCCCCCCCCCCCCCCDULL) >> 67);
			uint32_t r = (uint32_t)(val - (q * 10));
			buf[i++] = digits[r];
			val = q;
		}
	} else if (base == 2) {
		while (val > 0) {
			buf[i++] = digits[val & 1];
			val >>= 1;
		}
	} else if (base == 8) {
		while (val > 0) {
			buf[i++] = digits[val & 7];
			val >>= 3;
		}
	} else {
		while (val > 0) {
			buf[i++] = digits[val % base];
			val /= base;
		}
	}

	int length = i;
	int padding = width - length;

	if (!(flags & FLAG_LEFT_JUSTIFY) && (flags & FLAG_ZERO_PAD)) {
		while (padding > 0) {
			logbuf_putc('0');
			padding--;
			width--;
		}
	} else if (!(flags & FLAG_LEFT_JUSTIFY)) {
		while (padding > 0) {
			logbuf_putc(' ');
			padding--;
			width--;
		}
	}

	while (i > 0) {
		logbuf_putc(buf[--i]);
	}

	if ((flags & FLAG_LEFT_JUSTIFY)) {
		while (width > length) {
			logbuf_putc(' ');
			width--;
		}
	}
}

// full docs in logbuf.h
void logbuf_printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);

	for (const char *p = fmt; *p; p++) {
		if (*p != '%') {
			logbuf_putc(*p);
			continue;
		}

		uint8_t flags = 0;
		int width = 0;
		int length = LEN_DEFAULT;

		while (*++p) {
			if (*p == '#')	  flags |= FLAG_ALT;
			else if (*p == '0') flags |= FLAG_ZERO;
			else if (*p == '-') flags |= FLAG_LEFT;
			else if (*p == ' ') flags |= FLAG_SPACE;
			else if (*p == '+') flags |= FLAG_SIGN;
			else break;
		}

		if (*p == '*') {
			width = va_arg(args, int);
			p++;
		} else {
			while (*p >= '0' && *p <= '9') width = width * 10 + (*p++ - '0');
		}

		if (*p == '.') {
			p++;
			if (*p == '*') { va_arg(args, int); p++; }
			else while (*p >= '0' && *p <= '9') p++;
		}

		if (*p == 'h') {
			length = (*++p == 'h') ? (p++, LEN_HH) : LEN_H;
		} else if (*p == 'l') {
			length = (*++p == 'l') ? (p++, LEN_LL) : LEN_L;
		} else if (*p == 'z') { length = LEN_Z; p++; }
		else if (*p == 'j') { length = LEN_J; p++; }
		else if (*p == 't') { length = LEN_T; p++; }

		switch (*p) {
			case 'd':
			case 'i': {
				int64_t val;
				if (length == LEN_LL)	  val = va_arg(args, long long);
				else if (length == LEN_L)  val = va_arg(args, long);
				else if (length == LEN_Z)  val = va_arg(args, ssize_t);
				else if (length == LEN_J)  val = va_arg(args, intmax_t);
				else if (length == LEN_T)  val = va_arg(args, ptrdiff_t);
				else if (length == LEN_H)  val = (short)va_arg(args, int);
				else if (length == LEN_HH) val = (signed char)va_arg(args, int);
				else					   val = va_arg(args, int);

				if (val < 0) {
					logbuf_putc('-');
					logbuf_printf_number_h((uint64_t)-val, 10, width - 1, flags, false);
				} else {
					if (flags & FLAG_SIGN) {
						logbuf_putc('+');
						width--;
					} else if (flags & FLAG_SPACE) {
						logbuf_putc(' ');
						width--;
					}
					logbuf_printf_number_h((uint64_t)val, 10, width, flags, false);
				}
				break;
			}

			case 'u':
			case 'x':
			case 'X':
			case 'b': {
				uint64_t val;
				if (length == LEN_LL)	  val = va_arg(args, unsigned long long);
				else if (length == LEN_L)  val = va_arg(args, unsigned long);
				else if (length == LEN_Z)  val = va_arg(args, size_t);
				else if (length == LEN_J)  val = va_arg(args, uintmax_t);
				else if (length == LEN_T)  val = (uintptr_t)va_arg(args, void*);
				else					   val = va_arg(args, unsigned int);

				if (flags & FLAG_ALT) {
					if (*p == 'x' || *p == 'X') { logbuf_write("0x"); width -= 2; }
					if (*p == 'b') { logbuf_write("0b"); width -= 2; }
				}

				int base = (*p == 'b') ? 2 : ((*p == 'u') ? 10 : 16);
				logbuf_printf_number_h(val, base, width, flags, (*p == 'X'));
				break;
			}

			case 'p':
				logbuf_write("0x");
				logbuf_printf_number_h(va_arg(args, uintptr_t), 16, width, flags, false);
				break;

			case 's': {
				const char *s = va_arg(args, char *);
				logbuf_write(s ? s : "(null)");
				break;
			}

			case 'c':
				logbuf_putc((char)va_arg(args, int));
				break;

			case 'I': { // IPv4
				uint8_t *ip = va_arg(args, uint8_t *);
				for(int i=0; i<4; i++) {
					logbuf_printf_number_h(ip[i], 10, 0, 0, false);
					if(i < 3) logbuf_putc('.');
				}
				break;
			}

			case 'M': { // MAC
				uint8_t *mac = va_arg(args, uint8_t *);
				for(int i=0; i<6; i++) {
					logbuf_printf_number_h(mac[i], 16, 2, FLAG_ZERO, true);
					if(i < 5) logbuf_putc(':');
				}
				break;
			}

			case '%':
				logbuf_putc('%');
				break;
		}
	}
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
