#include "stdio.h"

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include "mem/mem.h"

void ptrthex(char *buf, uint64_t val) {
	const char hex_chars[] = "0123456789ABCDEF";
	for (int i = 15; i >= 0; i--) {
		buf[i] = hex_chars[val & 0xF];
		val >>= 4;
	}
	buf[16] = 0;
}

uint64_t strtoull(const char* nptr) {
	uint64_t val = 0;
	if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
		nptr += 2;
		while ((*nptr >= '0' && *nptr <= '9') || (*nptr >= 'a' && *nptr <= 'f') || (*nptr >= 'A' && *nptr <= 'F')) {
			val *= 16;
			if (*nptr >= '0' && *nptr <= '9') val += *nptr - '0';
			else if (*nptr >= 'a' && *nptr <= 'f') val += *nptr - 'a' + 10;
			else val += *nptr - 'A' + 10;
			nptr++;
		}
	} else {
		while (*nptr >= '0' && *nptr <= '9') {
			val = val * 10 + (*nptr - '0');
			nptr++;
		}
	}
	return val;
}

/**
 * @param getc_func pointer to a blocking getc
 * @param putc_func pointer to putc (to echo back characters) if NULL echo is disabled.
 * @param buffer where to return the string to
 * @param limit	max length of the string including NULL terminator
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

static const char digits_lower[17] = "0123456789abcdef";
static const char digits_upper[17] = "0123456789ABCDEF";

#define FLAG_ALT   (1 << 0) // #
#define FLAG_ZERO  (1 << 1) // 0
#define FLAG_LEFT  (1 << 2) // -
#define FLAG_SPACE (1 << 3) // ' '
#define FLAG_SIGN  (1 << 4) // +

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

typedef struct {
	char *buf;
	size_t cap;
	size_t pos;
} _internal_printf_ctx_t;

static inline void _internal_printf_ctx_putc(_internal_printf_ctx_t *ctx, char c) {
	if (ctx->pos < ctx->cap) {
		ctx->buf[ctx->pos] = c;
	}
	ctx->pos++;
}

static inline void _internal_printf_ctx_write(_internal_printf_ctx_t *ctx, const char *s, size_t n) {
	if (n == 0) {
		return;
	}
	size_t avail = ctx->cap > ctx->pos ? ctx->cap - ctx->pos : 0;
	size_t copy = n < avail ? n : avail;
	memcpy(ctx->buf + ctx->pos, s, copy);
	ctx->pos += n;
}

static inline void _internal_printf_ctx_pad(_internal_printf_ctx_t *ctx, char ch, int n) {
	if (n <= 0) {
		return;
	}
	size_t avail = ctx->cap > ctx->pos ? ctx->cap - ctx->pos : 0;
	size_t fill = (size_t)n < avail ? (size_t)n : avail;
	memset(ctx->buf + ctx->pos, (unsigned char)ch, fill);
	ctx->pos += (size_t)n;
}

static inline int _internal_printf_count_digits10(uint64_t v) {
	if (v < 10ULL) return 1;
	if (v < 100ULL) return 2;
	if (v < 1000ULL) return 3;
	if (v < 10000ULL) return 4;
	if (v < 100000ULL) return 5;
	if (v < 1000000ULL) return 6;
	if (v < 10000000ULL) return 7;
	if (v < 100000000ULL) return 8;
	if (v < 1000000000ULL) return 9;
	if (v < 10000000000ULL) return 10;
	if (v < 100000000000ULL) return 11;
	if (v < 1000000000000ULL) return 12;
	if (v < 10000000000000ULL) return 13;
	if (v < 100000000000000ULL) return 14;
	if (v < 1000000000000000ULL) return 15;
	if (v < 10000000000000000ULL) return 16;
	if (v < 100000000000000000ULL) return 17;
	if (v < 1000000000000000000ULL) return 18;
	if (v < 10000000000000000000ULL) return 19;
	return 20;
}

static void _internal_printf_fmt_uint(_internal_printf_ctx_t *ctx, uint64_t val, int base, int width, uint8_t flags, bool upper, const char *prefix, int prefixlen) {
	const char *digits = upper ? digits_upper : digits_lower;

	if (base == 10) {
		int numlen = (val == 0) ? 1 : _internal_printf_count_digits10(val);
		int total = numlen + prefixlen;
		int padding = width > total ? width - total : 0;

		if (!(flags & FLAG_LEFT)) {
			if (flags & FLAG_ZERO) {
				_internal_printf_ctx_write(ctx, prefix, (size_t)prefixlen);
				_internal_printf_ctx_pad(ctx, '0', padding);
			} else {
				_internal_printf_ctx_pad(ctx, ' ', padding);
				_internal_printf_ctx_write(ctx, prefix, (size_t)prefixlen);
			}
		} else {
			_internal_printf_ctx_write(ctx, prefix, (size_t)prefixlen);
		}

		size_t avail = ctx->cap > ctx->pos ? ctx->cap - ctx->pos : 0;
		size_t copy = (size_t)numlen < avail ? (size_t)numlen : avail;
		char *out = ctx->buf + ctx->pos;

		if (val == 0) {
			if (copy >= 1) {
				out[0] = '0';
			}
		} else {
			char *p = out + copy;
			char *p_log = out + numlen;
			if (copy == (size_t)numlen) {
				char *wp = out + numlen;
				while (val >= 100) {
					uint32_t rem = (uint32_t)(val % 100);
					val /= 100;
					*--wp = map100[(rem << 1) + 1];
					*--wp = map100[rem << 1];
				}
				if (val >= 10) {
					*--wp = map100[(val << 1) + 1];
					*--wp = map100[val << 1];
				} else {
					*--wp = (char)('0' + val);
				}
			} else {
				char tmp[21];
				char *wp = tmp + numlen;
				while (val >= 100) {
					uint32_t rem = (uint32_t)(val % 100);
					val /= 100;
					*--wp = map100[(rem << 1) + 1];
					*--wp = map100[rem << 1];
				}
				if (val >= 10) {
					*--wp = map100[(val << 1) + 1];
					*--wp = map100[val << 1];
				} else {
					*--wp = (char)('0' + val);
				}
				memcpy(out, tmp, copy);
			}
			(void)p;
			(void)p_log;
		}

		ctx->pos += (size_t)numlen;

		if (flags & FLAG_LEFT) {
			_internal_printf_ctx_pad(ctx, ' ', padding);
		}
	} else {
		char tmp[66];
		int i = 65;

		if (val == 0) {
			tmp[--i] = '0';
		} else if (base == 16) {
			while (val) {
				tmp[--i] = digits[val & 0xF];
				val >>= 4;
			}
		} else if (base == 2) {
			while (val) {
				tmp[--i] = (char)('0' + (val & 1));
				val >>= 1;
			}
		} else {
			// octal or other
			while (val) {
				tmp[--i] = digits[val % (uint32_t)base];
				val /= (uint32_t)base;
			}
		}

		int numlen = 65 - i;
		int total = numlen + prefixlen;
		int padding = width > total ? width - total : 0;

		if (!(flags & FLAG_LEFT)) {
			if (flags & FLAG_ZERO) {
				_internal_printf_ctx_write(ctx, prefix, (size_t)prefixlen);
				_internal_printf_ctx_pad(ctx, '0', padding);
			} else {
				_internal_printf_ctx_pad(ctx, ' ', padding);
				_internal_printf_ctx_write(ctx, prefix, (size_t)prefixlen);
			}
		} else {
			_internal_printf_ctx_write(ctx, prefix, (size_t)prefixlen);
		}

		_internal_printf_ctx_write(ctx, tmp + i, (size_t)numlen);

		if (flags & FLAG_LEFT) {
			_internal_printf_ctx_pad(ctx, ' ', padding);
		}
	}
}

static void _internal_printf_fmt_signed(_internal_printf_ctx_t *ctx, int64_t val, int width, uint8_t flags) {
	char prefix[2];
	int prefixlen = 0;
	uint64_t uval;

	if (val < 0) {
		prefix[0] = '-';
		prefixlen = 1;
		uval = (uint64_t)(-(val + 1)) + 1; // safe for INT64_MIN
	} else {
		uval = (uint64_t)val;
		if (flags & FLAG_SIGN) {
			prefix[0] = '+';
			prefixlen = 1;
		} else if (flags & FLAG_SPACE) {
			prefix[0] = ' ';
			prefixlen = 1;
		}
	}

	_internal_printf_fmt_uint(ctx, uval, 10, width, flags, false, prefix, prefixlen);
}

static void _internal_printf_fmt_string(_internal_printf_ctx_t *ctx, const char *s, int width, uint8_t flags) {
	if (!s) {
		s = "(null)";
	}
	size_t len = strlen(s);
	int padding = (width > (int)len) ? width - (int)len : 0;

	if (!(flags & FLAG_LEFT)) {
		_internal_printf_ctx_pad(ctx, ' ', padding);
	}
	_internal_printf_ctx_write(ctx, s, len);
	if (flags & FLAG_LEFT) {
		_internal_printf_ctx_pad(ctx, ' ', padding);
	}
}

static inline void _internal_printf_fmt_u8_dec(_internal_printf_ctx_t *ctx, uint8_t v) {
	if (v >= 100) {
		char tmp[3];
		uint32_t rem = v % 100;
		tmp[0] = (char)('0' + v / 100);
		tmp[1] = map100[(rem << 1)];
		tmp[2] = map100[(rem << 1) + 1];
		_internal_printf_ctx_write(ctx, tmp, 3);
	} else if (v >= 10) {
		char tmp[2];
		tmp[0] = map100[(v << 1)];
		tmp[1] = map100[(v << 1) + 1];
		_internal_printf_ctx_write(ctx, tmp, 2);
	} else {
		_internal_printf_ctx_putc(ctx, (char)('0' + v));
	}
}

static inline void _internal_printf_fmt_u8_hex2(_internal_printf_ctx_t *ctx, uint8_t v) {
	char tmp[2];
	tmp[0] = digits_upper[v >> 4];
	tmp[1] = digits_upper[v & 0xF];
	_internal_printf_ctx_write(ctx, tmp, 2);
}

int vsnprintf(char *buf, size_t bufsz, const char *fmt, va_list args) {
	_internal_printf_ctx_t ctx;
	ctx.buf = buf;
	ctx.cap = (bufsz > 0) ? bufsz - 1 : 0;
	ctx.pos = 0;

	const char *p = fmt;
	while (*p) {
		const char *lit = p;
		while (*p && *p != '%') {
			p++;
		}
		if (p != lit) {
			_internal_printf_ctx_write(&ctx, lit, (size_t)(p - lit));
		}
		if (!*p) {
			break;
		}

		p++; // skip '%'
		if (!*p) {
			break;
		}

		uint8_t flags = 0;
		int width = 0;
		int length = LEN_DEFAULT;

		for (;;) {
			if (*p == '#')	  { flags |= FLAG_ALT;   p++; }
			else if (*p == '0') { flags |= FLAG_ZERO;  p++; }
			else if (*p == '-') { flags |= FLAG_LEFT;  p++; }
			else if (*p == ' ') { flags |= FLAG_SPACE; p++; }
			else if (*p == '+') { flags |= FLAG_SIGN;  p++; }
			else break;
		}

		if (flags & FLAG_LEFT) {
			flags &= (uint8_t)~FLAG_ZERO;
		}

		if (*p == '*') {
			width = va_arg(args, int);
			if (width < 0) {
				flags |= FLAG_LEFT;
				width = -width;
			}
			p++;
		} else {
			while (*p >= '0' && *p <= '9') {
				width = width * 10 + (*p - '0');
				p++;
			}
		}

		if (*p == '.') {
			p++;
			if (*p == '*') {
				va_arg(args, int);
				p++;
			} else {
				while (*p >= '0' && *p <= '9') {
					p++;
				}
			}
		}

		if (*p == 'h') {
			p++;
			if (*p == 'h') { length = LEN_HH; p++; }
			else { length = LEN_H; }
		} else if (*p == 'l') {
			p++;
			if (*p == 'l') { length = LEN_LL; p++; }
			else { length = LEN_L; }
		} else if (*p == 'z') { length = LEN_Z; p++; }
		else if (*p == 'j')   { length = LEN_J; p++; }
		else if (*p == 't')   { length = LEN_T; p++; }

		switch (*p) {
			case 'd':
			case 'i': {
				int64_t val;
				if (length == LEN_LL)	  val = va_arg(args, long long);
				else if (length == LEN_L)  val = va_arg(args, long);
				else if (length == LEN_Z)  val = (int64_t)va_arg(args, ssize_t);
				else if (length == LEN_J)  val = va_arg(args, intmax_t);
				else if (length == LEN_T)  val = va_arg(args, ptrdiff_t);
				else if (length == LEN_H)  val = (short)va_arg(args, int);
				else if (length == LEN_HH) val = (signed char)va_arg(args, int);
				else val = va_arg(args, int);
				_internal_printf_fmt_signed(&ctx, val, width, flags);
				break;
			}

			case 'u':
			case 'x':
			case 'X':
			case 'o':
			case 'b': {
				uint64_t val;
				if (length == LEN_LL)	  val = va_arg(args, unsigned long long);
				else if (length == LEN_L)  val = va_arg(args, unsigned long);
				else if (length == LEN_Z)  val = va_arg(args, size_t);
				else if (length == LEN_J)  val = va_arg(args, uintmax_t);
				else if (length == LEN_T)  val = (uintptr_t)va_arg(args, void *);
				else if (length == LEN_H)  val = (unsigned short)va_arg(args, unsigned int);
				else if (length == LEN_HH) val = (unsigned char)va_arg(args, unsigned int);
				else val = va_arg(args, unsigned int);

				const char *prefix = "";
				int prefixlen = 0;
				int base;
				bool upper = false;

				switch (*p) {
					case 'x':
						base = 16;
						if (flags & FLAG_ALT) { prefix = "0x"; prefixlen = 2; }
						break;
					case 'X':
						base = 16;
						upper = true;
						if (flags & FLAG_ALT) { prefix = "0X"; prefixlen = 2; }
						break;
					case 'o':
						base = 8;
						if ((flags & FLAG_ALT) && val) { prefix = "0"; prefixlen = 1; }
						break;
					case 'b':
						base = 2;
						if (flags & FLAG_ALT) { prefix = "0b"; prefixlen = 2; }
						break;
					default:
						base = 10;
						break;
				}

				_internal_printf_fmt_uint(&ctx, val, base, width, flags, upper, prefix, prefixlen);
				break;
			}

			case 'p': {
				uintptr_t val = (uintptr_t)va_arg(args, void *);
				int pwidth = (width > 0) ? width : (int)(sizeof(void *) * 2 + 2);
				_internal_printf_fmt_uint(&ctx, (uint64_t)val, 16, pwidth, FLAG_ZERO, false, "0x", 2);
				break;
			}

			case 's': {
				const char *s = va_arg(args, const char *);
				_internal_printf_fmt_string(&ctx, s, width, flags);
				break;
			}

			case 'c':
				_internal_printf_ctx_putc(&ctx, (char)va_arg(args, int));
				break;

			case 'I': {
				// ipv4: expects uint8_t[4]
				uint8_t *ip = va_arg(args, uint8_t *);
				_internal_printf_fmt_u8_dec(&ctx, ip[0]); _internal_printf_ctx_putc(&ctx, '.');
				_internal_printf_fmt_u8_dec(&ctx, ip[1]); _internal_printf_ctx_putc(&ctx, '.');
				_internal_printf_fmt_u8_dec(&ctx, ip[2]); _internal_printf_ctx_putc(&ctx, '.');
				_internal_printf_fmt_u8_dec(&ctx, ip[3]);
				break;
			}

			case 'M': {
				// mac: expects uint8_t[6]
				uint8_t *mac = va_arg(args, uint8_t *);
				_internal_printf_fmt_u8_hex2(&ctx, mac[0]); _internal_printf_ctx_putc(&ctx, ':');
				_internal_printf_fmt_u8_hex2(&ctx, mac[1]); _internal_printf_ctx_putc(&ctx, ':');
				_internal_printf_fmt_u8_hex2(&ctx, mac[2]); _internal_printf_ctx_putc(&ctx, ':');
				_internal_printf_fmt_u8_hex2(&ctx, mac[3]); _internal_printf_ctx_putc(&ctx, ':');
				_internal_printf_fmt_u8_hex2(&ctx, mac[4]); _internal_printf_ctx_putc(&ctx, ':');
				_internal_printf_fmt_u8_hex2(&ctx, mac[5]);
				break;
			}

			case '%':
				_internal_printf_ctx_putc(&ctx, '%');
				break;

			default:
				_internal_printf_ctx_putc(&ctx, '%');
				_internal_printf_ctx_putc(&ctx, *p);
				break;
		}
		p++;
	}

	if (bufsz > 0) {
		buf[ctx.pos < ctx.cap ? ctx.pos : ctx.cap] = '\0';
	}

	return (int)ctx.pos;
}

int snprintf(char *buf, size_t bufsz, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int ret = vsnprintf(buf, bufsz, fmt, args);
	va_end(args);
	return ret;
}
