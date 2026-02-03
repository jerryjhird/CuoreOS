/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "stdio.h"
#include "stddef.h"
#include <stdarg.h>
#include "string.h"

static inline const char* logbuftptr(logbuf_t *log, size_t *out_len) {
    *out_len = log->len;
    return log->buf;
}

void logbuf_store(logbuf_t *log, const char *msg) {
    while (*msg && log->len < LOGBUF_SIZE - 1) {
        log->buf[log->len++] = *msg++;
    }
    log->buf[log->len] = '\0';
}

void logbuf_flush(logbuf_t *log, void *(*write_function)(const char *msg, size_t len)) {
    size_t len;
    const char *ptr = logbuftptr(log, &len);
    write_function(ptr, len);
}

void logbuf_clear(logbuf_t *log) {
    log->len = 0;
    log->buf[0] = '\0';
}

void logbuf_printf(struct logbuf_t *log, const char *fmt, ...)
{
    if (!log || !fmt) return;

    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            logbuf_store(log, fmt);
            fmt++;
            continue;
        }

        fmt++; /* skip '%' */

        switch (*fmt) {
        case '%':
            logbuf_store(log, "%");
            break;

        case 'c': {
            char c = (char)va_arg(ap, int);
            logbuf_store(log, &c);
            break;
        }

        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            logbuf_store(log, s);
            break;
        }

        case 'd': {
            int v = va_arg(ap, int);
            unsigned uv;
            char buf[12];
            int i = 0;

            if (v < 0) {
                logbuf_store(log, "-");
                uv = (unsigned)(-(v + 1)) + 1; /* avoid INT_MIN UB */
            } else {
                uv = (unsigned)v;
            }

            if (uv == 0u) {
                logbuf_store(log, "0");
                break;
            }

            while (uv) {
                buf[i++] = (char)('0' + (uv % 10u));
                uv /= 10u;
            }
            while (i--) logbuf_store(log, &buf[i]);
            break;
        }

        case 'u': {
            unsigned v = va_arg(ap, unsigned);
            char buf[12];
            int i = 0;

            if (v == 0u) {
                logbuf_store(log, "0");
                break;
            }

            while (v) {
                buf[i++] = (char)('0' + (v % 10u));
                v /= 10u;
            }
            while (i--) logbuf_store(log, &buf[i]);
            break;
        }

        case 'x': {
            unsigned v = va_arg(ap, unsigned);
            char buf[sizeof(unsigned) * 2];
            int i = 0;
            static const char hex[] = "0123456789abcdef";

            if (v == 0u) {
                logbuf_store(log, "0");
                break;
            }

            while (v) {
                buf[i++] = hex[v & 0xfu];
                v >>= 4;
            }
            while (i--) logbuf_store(log, &buf[i]);
            break;
        }

        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            char buf[sizeof(uintptr_t) * 2];
            int i = 0;
            static const char hex[] = "0123456789abcdef";

            logbuf_store(log, "0x");

            if (v == 0u) {
                logbuf_store(log, "0");
                break;
            }

            while (v) {
                buf[i++] = hex[v & 0xfu];
                v >>= 4;
            }
            while (i--) logbuf_store(log, &buf[i]);
            break;
        }

        default:
            logbuf_store(log, "%");
            logbuf_store(log, fmt);
            break;
        }

        fmt++;
    }

    va_end(ap);
}

#include "time.h"
uint32_t get_epoch(void) {
    struct tm now = gettm();
    return tm_to_epoch(&now);
}