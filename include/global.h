#ifndef GLOBAL_H
#define GLOBAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "flanterm.h"
#include "stdint.h"

struct flanterm_context;
extern struct flanterm_context *g_ft_ctx;

// buffered-write(bwrite) buffer for writing text to the console
#define BUF_SIZE 256
extern char g_writebuf[BUF_SIZE];
extern size_t g_writebuf_len;

#ifdef __cplusplus
}
#endif

#endif // GLOBAL_H