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
void flush(struct writeout_t *wo);
void bwrite(struct writeout_t *wo, const char *msg);

void readline(struct writeout_t *wo, char *buf, size_t size);

void write_epoch(struct writeout_t *wo);
void panic(struct writeout_t *wo);

#ifdef __cplusplus
}
#endif

#endif // STDIO_H