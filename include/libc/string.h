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

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

void u32dec(char *buf, uint32_t val);
void *ptrhex(char *buf, void *ptr);

void iota(struct writeout_t *wo, int64_t val);
void uiota(struct writeout_t *wo, uint64_t val);

unsigned int hash(const char *s);

#ifdef __cplusplus
}
#endif

#endif // STRING_H