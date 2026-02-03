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

#include <stdint.h>
#include <stddef.h>
#define LOGBUF_SIZE 4096

typedef struct logbuf_t {
    char buf[LOGBUF_SIZE];
    size_t len; // current length
} logbuf_t;

extern logbuf_t klog; // defined in kentry

void logbuf_store(logbuf_t *log, const char *msg);
void logbuf_flush(logbuf_t *log, void *(*write_function)(const char *msg, size_t len));
void logbuf_clear(logbuf_t *log);

// supports %c, %x, %d,%u, %s, %p
void printf(struct logbuf_t *log, const char *fmt, ...);

uint32_t get_epoch(void);

#ifdef __cplusplus
}
#endif

#endif // STDIO_H