/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef STRING_H
#define STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);

void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

void ptrhex(char *buf, void *ptr);

#ifdef __cplusplus
}
#endif

#endif // STRING_H