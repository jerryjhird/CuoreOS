#ifndef STRING_H
#define STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdio.h"

size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);

void u32dec(char *buf, uint32_t val);
void *ptrhex(char *buf, void *ptr);

uint32_t crc32c_swhash(const char *s); // software based crc32c

#ifdef __cplusplus
}
#endif

#endif // STRING_H