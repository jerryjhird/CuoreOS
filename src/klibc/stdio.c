/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "string.h"
#include "stdio.h"
#include "stdint.h"
#include "stdarg.h"

#include "ps2.h"
#include "time.h"
#include "memory.h"

void write(struct writeout_t *wo, const char *msg, size_t len) {
    if (wo && wo->write) {
        wo->write(msg, len, wo->ctx);
    }
}

void flush(struct writeout_t *wo) {
    if (wo->len > 0 && wo->write) {
        wo->write(wo->buf, wo->len, wo->ctx);
        wo->len = 0;
        wo->buf[0] = '\0';
    }
}

void bwrite(struct writeout_t *wo, const char *msg) {
    if (!wo || !wo->write || !msg) return;

    size_t msg_len = strlen(msg);
    size_t offset = 0;

    while (offset < msg_len) {
        size_t space = BUF_SIZE - wo->len;
        size_t chunk = (msg_len - offset < space) ? (msg_len - offset) : space;

        memcpy(wo->buf + wo->len, msg + offset, chunk);
        wo->len += chunk;
        offset += chunk;

        if (wo->len == BUF_SIZE) {
            flush(wo);
        }
    }
}

void lbwrite(struct writeout_t *wo, const char *msg, size_t len) {
    if (!wo || !wo->write || !msg || len == 0) return;

    size_t offset = 0;

    while (offset < len) {
        size_t space = BUF_SIZE - wo->len;
        size_t chunk = (len - offset < space) ? (len - offset) : space;

        memcpy(wo->buf + wo->len, msg + offset, chunk);
        wo->len += chunk;
        offset += chunk;

        if (wo->len == BUF_SIZE) {
            flush(wo);
        }
    }
}

void printf(struct writeout_t *wo, const char *fmt, ...)
{
    if (!wo || !fmt) return;

    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            lbwrite(wo, fmt, 1);
            fmt++;
            continue;
        }

        fmt++; /* skip '%' */

        switch (*fmt) {
        case '%':
            lbwrite(wo, "%", 1);
            break;

        case 'c': {
            char c = (char)va_arg(ap, int);
            lbwrite(wo, &c, 1);
            break;
        }

        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            lbwrite(wo, s, strlen(s));
            break;
        }

        case 'd': {
            int v = va_arg(ap, int);
            unsigned uv;
            char buf[12];
            int i = 0;

            if (v < 0) {
                lbwrite(wo, "-", 1);
                uv = (unsigned)(-(v + 1)) + 1; /* avoid INT_MIN UB */
            } else {
                uv = (unsigned)v;
            }

            if (uv == 0u) {
                lbwrite(wo, "0", 1);
                break;
            }

            while (uv) {
                buf[i++] = (char)('0' + (uv % 10u));
                uv /= 10u;
            }
            while (i--) lbwrite(wo, &buf[i], 1);
            break;
        }

        case 'u': {
            unsigned v = va_arg(ap, unsigned);
            char buf[12];
            int i = 0;

            if (v == 0u) {
                lbwrite(wo, "0", 1);
                break;
            }

            while (v) {
                buf[i++] = (char)('0' + (v % 10u));
                v /= 10u;
            }
            while (i--) lbwrite(wo, &buf[i], 1);
            break;
        }

        case 'x': {
            unsigned v = va_arg(ap, unsigned);
            char buf[sizeof(unsigned) * 2];
            int i = 0;
            static const char hex[] = "0123456789abcdef";

            if (v == 0u) {
                lbwrite(wo, "0", 1);
                break;
            }

            while (v) {
                buf[i++] = hex[v & 0xfu];
                v >>= 4;
            }
            while (i--) lbwrite(wo, &buf[i], 1);
            break;
        }

        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            char buf[sizeof(uintptr_t) * 2];
            int i = 0;
            static const char hex[] = "0123456789abcdef";

            lbwrite(wo, "0x", 2);

            if (v == 0u) {
                lbwrite(wo, "0", 1);
                break;
            }

            while (v) {
                buf[i++] = hex[v & 0xfu];
                v >>= 4;
            }
            while (i--) lbwrite(wo, &buf[i], 1);
            break;
        }

        default:
            lbwrite(wo, "%", 1);
            lbwrite(wo, fmt, 1);
            break;
        }

        fmt++;
    }

    va_end(ap);
}

void readline(struct writeout_t *wo, char *buf, size_t size) {
    if (size == 0) return;
    if (wo->len > 0) {
        flush(wo);
    }

    size_t pos = 0;
    while (1) {
        char c = ps2_getc();  // read one character

        // stop reading if enter
        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            write(wo, "\n", 1);
            return;
        }

        if ((c == '\b' || c == 127) && pos > 0) {
            pos--;
            const char bs_seq[] = "\b \b";
            write(wo, bs_seq, sizeof(bs_seq) - 1); // erase char
            continue;
        }

        if (pos < size - 1) {
            buf[pos++] = c;
            write(wo, &c, 1);  // echo typed char
        } else {
            // buffer full
            buf[pos] = '\0';
            return;
        }
    }
}

uint32_t get_epoch(void) {
    struct tm now = gettm();
    return tm_to_epoch(&now);
}