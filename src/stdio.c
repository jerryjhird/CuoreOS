#include "stdio.h"

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include "abs.h"

void ptrthex(char *buf, uint64_t val) {
	const char hex_chars[] = "0123456789ABCDEF";
	for (int i = 15; i >= 0; i--) {
		buf[i] = hex_chars[val & 0xF];
		val >>= 4;
	}
	buf[16] = 0;
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

static const char map100[] = 
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

static inline void vprintf_puts_h(putc_t putc, const char *s) {
    while (*s) putc(*s++);
}

#define FLAG_ALT (1 << 0) // #
#define FLAG_ZERO (1 << 1) // 0
#define FLAG_LEFT (1 << 2) // -
#define FLAG_SPACE (1 << 3) // ' '
#define FLAG_SIGN (1 << 4) // +
#define FLAG_UPPER (1 << 5) // uppercase hex

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

static void vprintf_number_h_cb(putc_t putc, uint64_t val, int base, int width, uint8_t flags, bool uppercase) {
    char buf[70];
    int i = 68;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (val == 0) {
        buf[i--] = '0';
    } else if (base == 10) {
        while (val >= 100) {
            uint32_t res = val % 100;
            val /= 100;
            buf[i--] = map100[(res << 1) + 1];
            buf[i--] = map100[res << 1];
        }
        if (val >= 10) {
            buf[i--] = map100[(val << 1) + 1];
            buf[i--] = map100[val << 1];
        } else {
            buf[i--] = (char)('0' + val);
        }
    } else if (base == 16) {
        while (val) {
            buf[i--] = digits[val & 0xF];
            val >>= 4;
        }
    } else if (base == 2) {
        while (val) {
            buf[i--] = digits[val & 1];
            val >>= 1;
        }
    } else {
        while (val) {
            buf[i--] = digits[val % base];
            val /= base;
        }
    }

    int len = 68 - i;
    int padding = width - len;

    if (!(flags & FLAG_LEFT)) {
        char pad_char = (flags & FLAG_ZERO) ? '0' : ' ';
        while (padding-- > 0) putc(pad_char);
    }

    while (++i <= 68) putc(buf[i]);

    if (flags & FLAG_LEFT) {
        while (padding-- > 0) putc(' ');
    }
}

void vprintfcb(putc_t putc, const char *fmt, va_list args) {
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            putc(*p);
            continue;
        }

        uint8_t flags = 0;
        int width = 0;
        int length = LEN_DEFAULT;

        while (*++p) {
            if (*p == '#')      flags |= FLAG_ALT;
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

        // specifiers
        switch (*p) {
            case 'd':
            case 'i': {
                int64_t val;
                if (length == LEN_LL)      val = va_arg(args, long long);
                else if (length == LEN_L)  val = va_arg(args, long);
                else if (length == LEN_Z)  val = va_arg(args, ssize_t);
                else if (length == LEN_J)  val = va_arg(args, intmax_t);
                else if (length == LEN_T)  val = va_arg(args, ptrdiff_t);
                else if (length == LEN_H)  val = (short)va_arg(args, int);
                else if (length == LEN_HH) val = (signed char)va_arg(args, int);
                else                       val = va_arg(args, int);

                if (val < 0) {
                    putc('-');
                    vprintf_number_h_cb(putc, (uint64_t)-val, 10, width - 1, flags, false);
                } else {
                    if (flags & FLAG_SIGN) { putc('+'); width--; }
                    else if (flags & FLAG_SPACE) { putc(' '); width--; }
                    vprintf_number_h_cb(putc, (uint64_t)val, 10, width, flags, false);
                }
                break;
            }

            case 'u':
            case 'x':
            case 'X':
            case 'b': {
                uint64_t val;
                if (length == LEN_LL)      val = va_arg(args, unsigned long long);
                else if (length == LEN_L)  val = va_arg(args, unsigned long);
                else if (length == LEN_Z)  val = va_arg(args, size_t);
                else if (length == LEN_J)  val = va_arg(args, uintmax_t);
                else if (length == LEN_T)  val = (uintptr_t)va_arg(args, void*);
                else if (length == LEN_H)  val = (unsigned short)va_arg(args, unsigned int);
                else if (length == LEN_HH) val = (unsigned char)va_arg(args, unsigned int);
                else                       val = va_arg(args, unsigned int);

                if (flags & FLAG_ALT) {
                    if (*p == 'x' || *p == 'X') { vprintf_puts_h(putc, "0x"); width -= 2; }
                    else if (*p == 'b') { vprintf_puts_h(putc, "0b"); width -= 2; }
                }

                int base = (*p == 'b') ? 2 : ((*p == 'u') ? 10 : 16);
                vprintf_number_h_cb(putc, val, base, width, flags, (*p == 'X'));
                break;
            }

            case 'p':
                vprintf_puts_h(putc, "0x");
                vprintf_number_h_cb(putc, (uintptr_t)va_arg(args, void*), 16, width - 2, flags, false);
                break;

            case 's': {
                const char *s = va_arg(args, char *);
                vprintf_puts_h(putc, s ? s : "(null)");
                break;
            }

            case 'c':
                putc((char)va_arg(args, int));
                break;

            case 'I': { // IPv4
                uint8_t *ip = va_arg(args, uint8_t *);
                for(int i=0; i<4; i++) {
                    vprintf_number_h_cb(putc, ip[i], 10, 0, 0, false);
                    if(i < 3) putc('.');
                }
                break;
            }

            case 'M': { // MAC
                uint8_t *mac = va_arg(args, uint8_t *);
                for(int i=0; i<6; i++) {
                    vprintf_number_h_cb(putc, mac[i], 16, 2, FLAG_ZERO, true);
                    if(i < 5) putc(':');
                }
                break;
            }

            case '%':
                putc('%');
                break;
        }
    }
}
