#ifndef STDIO_H
#define STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

void write(const char* msg, size_t len);
void flush(void);
void bwrite(const char *msg);

void readline(char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif // STDIO_H