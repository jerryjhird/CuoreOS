#pragma once

#include <stddef.h>

void* memcpy(void* restrict dest, const void* restrict src, size_t n);
size_t strlen(const char* s);
void* memset(void* s, int c, size_t n);
void* memmove(void* dest, const void* src, size_t n);