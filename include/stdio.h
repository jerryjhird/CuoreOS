/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef STDIO_H
#define STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#define BUF_SIZE 256

#define INFO_LOG_STR "\x1b[#48B9D7m[ INFO ]\x1b[0m"
#define TIME_LOG_STR "\x1b[#48B9D7m[ TIME ]\x1b[0m"
#define PASS_LOG_STR "\x1b[#96D9C0m[ PASS ]\x1b[0m"
#define FAIL_LOG_STR "\x1b[#D99696m[ FAIL ]\x1b[0m"
#define WARN_LOG_STR "\x1b[#D35A11m[ WARN ]\x1b[0m"

struct writeout_t {
    char buf[BUF_SIZE];
    size_t len;
    void (*write)(const char *msg, size_t len, void *ctx);
    void *ctx;
};

void write(struct writeout_t *wo, const char *msg, size_t len);
void bwrite(struct writeout_t *wo, const char *msg); // writeout buffered
void lbwrite(struct writeout_t *wo, const char *msg, size_t len); // bwrite but with length argument

// supports %c, %x, %d,%u, %s, %p
void printf(struct writeout_t *wo, const char *fmt, ...); // writeout buffered

void flush(struct writeout_t *wo); // flush writeout_t->buf to writeout_t->write

void readline(struct writeout_t *wo, char *buf, size_t size);

uint32_t get_epoch(void);

#ifdef __cplusplus
}
#endif

#endif // STDIO_H