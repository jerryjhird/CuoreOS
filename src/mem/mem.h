#pragma once

#include <stddef.h>
#include <stdint.h>

#define HTONS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))
#define NTOHS(n) HTONS(n)
#define HTONL(n) (((n & 0xFF000000) >> 24) | ((n & 0x00FF0000) >> 8) | ((n & 0x0000FF00) << 8)  | ((n & 0x000000FF) << 24))
#define NTOHL(n) HTONL(n)

#define PAGE_SIZE 4096
#define PAGE_SIZE_2MB 0x200000
#define PAGE_SIZE_1GB 0x40000000

extern uint64_t hhdm_offset;

char *strncpy(char *dest, const char *src, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
int strcmp(const char *s1, const char *s2);
size_t strlen(const char* s);
void* memset(void* s, int c, size_t n);
void* memcpy(void* restrict dest, const void* restrict src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
