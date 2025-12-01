#ifndef STRING_H
#define STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

void u32dec(char *buf, uint32_t val);

void itoa(int64_t val, char *buf);
void uitoa(uint64_t val, char *buf);

unsigned int hash(const char *s);

#ifdef __cplusplus
}
#endif

#endif // STRING_H