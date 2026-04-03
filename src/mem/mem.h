#pragma once

#include <stddef.h>
#include <stdint.h>

char *strncpy(char *dest, const char *src, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char* s);
void* memset(void* s, int c, size_t n);
void* memcpy(void* restrict dest, const void* restrict src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

#ifndef HHDM_OFFSET_SET
extern uint64_t hhdm_offset;
#define HHDM_OFFSET_SET
#endif

#define P2V(pa) ((uintptr_t)(pa) + hhdm_offset)
#define V2P(va) ((uintptr_t)(va) - hhdm_offset)
