#ifndef STDIO_H
#define STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#define BUF_SIZE 256

struct writeout_t {
    char buf[BUF_SIZE];
    size_t len;
    void (*write)(const char *msg, size_t len, void *ctx);
    void *ctx;
};


void write(struct writeout_t *wo, const char *msg, size_t len);
void bwrite(struct writeout_t *wo, const char *msg); // writeout buffered (use flush to flush)
void lbwrite(struct writeout_t *wo, const char *msg, size_t len); // bwrite but with length argument
void printf(struct writeout_t *wo, const char *fmt, ...); // writeout buffered (use flush to flush)

void flush(struct writeout_t *wo); // flush writeout_t->buf to writeout_t->write

void readline(struct writeout_t *wo, char *buf, size_t size);

uint32_t get_epoch(void);

#ifdef __cplusplus
}
#endif

#endif // STDIO_H