#pragma once

#include <stddef.h>
#include <stdint.h>
#include "abs.h"
#include "panic.h"

#define PAGE_SIZE 4096
#define PAGE_SIZE_2MB 0x200000
#define PAGE_SIZE_1GB 0x40000000

extern uint64_t hhdm_offset;

static inline bool ptr_is_canonical(const void *p) {
	uintptr_t addr = (uintptr_t)p;
	uintptr_t shifted = ((uintptr_t)addr << 7);
	uintptr_t extended = ((intptr_t)shifted >> 7);

	return addr == extended;
}

static inline bool range_is_safe(const void *p, size_t n) {
	if (n == 0) { return true; }
	uintptr_t start = (uintptr_t)p;
	uintptr_t end = start + n - 1;

	if (end < start) { return false; }
	if (!ptr_is_canonical(p) || !ptr_is_canonical((void *)end)) { return false; }
	return true;
}

static inline void *memset(void *s, int c, size_t n) {
	if (UNLIKELY(s == NULL)) { panic("MEMORY", "memset s is null"); }
	if (UNLIKELY(!range_is_safe(s, n))) { panic("MEMORY", "memset s is out of range"); }

	uint8_t *p = (uint8_t *)s;
	for (size_t i = 0; i < n; i++) {
		p[i] = (uint8_t)c;
	}

	return s;
}

static inline void *memcpy(void *dest, const void *src, size_t n) {
	if (UNLIKELY(dest == NULL)) { panic("MEMORY", "memcpy dest is null"); }
	if (UNLIKELY(src == NULL)) { panic("MEMORY", "memcpy src is null"); }
	if (UNLIKELY(!range_is_safe(dest, n))) { panic("MEMORY", "memcpy dest is out of range"); }
	if (UNLIKELY(!range_is_safe(src, n))) { panic("MEMORY", "memcpy src is out of range"); }

	uint8_t *d = (uint8_t *)dest;
	const uint8_t *s = (const uint8_t *)src;

	for (size_t i = 0; i < n; i++) {
		d[i] = s[i];
	}

	return dest;
}

static inline void *memmove(void *dest, const void *src, size_t n) {
	if (UNLIKELY(dest == NULL)) { panic("MEMORY", "memmove dest is null"); }
	if (UNLIKELY(src == NULL)) { panic("MEMORY", "memmove src is null"); }
	if (UNLIKELY(!range_is_safe(dest, n))) { panic("MEMORY", "memmove dest is out of range"); }
	if (UNLIKELY(!range_is_safe(src, n))) { panic("MEMORY", "memmove src is out of range"); }

	uint8_t *d = (uint8_t *)dest;
	const uint8_t *s = (const uint8_t *)src;
	if (d == s) { return dest; }
	if (d < s) {
		for (size_t i = 0; i < n; i++) {
			d[i] = s[i];
		}
	} else {
		// copy backwards when regions overlap
		for (size_t i = n; i > 0; i--) {
			d[i - 1] = s[i - 1];
		}
	}
	return dest;
}

static inline int memcmp(const void *s1, const void *s2, size_t n) {
	if (UNLIKELY(s1 == NULL)) { panic("MEMORY", "memcmp s1 is null"); }
	if (UNLIKELY(s2 == NULL)) { panic("MEMORY", "memcmp s2 is null"); }
	if (UNLIKELY(!range_is_safe(s1, n))) { panic("MEMORY", "memcmp s1 is out of range"); }
	if (UNLIKELY(!range_is_safe(s2, n))) { panic("MEMORY", "memcmp s2 is out of range"); }

	const uint8_t *p1 = (const uint8_t *)s1;
	const uint8_t *p2 = (const uint8_t *)s2;

	for (size_t i = 0; i < n; i++) {
		if (p1[i] != p2[i]) {
			return (int)p1[i] - (int)p2[i];
		}
	}

	return 0;
}

static inline size_t strlen(const char *s) {
	if (UNLIKELY(s == NULL)) { panic("STRING", "strlen s is null"); }

	size_t len = 0;

	for (;;) {
		if (UNLIKELY(!ptr_is_canonical(s + len))) { panic("STRING", "strlen ran into non canonical memory"); }
		if (s[len] == 0) {
			break;
		}
		len++;
	}

	return len;
}

static inline int strcmp(const char *s1, const char *s2) {
	if (UNLIKELY(s1 == NULL)) { panic("STRING", "strcmp s1 is null"); }
	if (UNLIKELY(s2 == NULL)) { panic("STRING", "strcmp s2 is null"); }

	size_t i = 0;
	for (;;) {
		if (UNLIKELY(!ptr_is_canonical(s1 + i) || !ptr_is_canonical(s2 + i))) { panic("STRING", "strcmp ran into non canonical memory"); }

		unsigned char c1 = (unsigned char)s1[i];
		unsigned char c2 = (unsigned char)s2[i];

		if (c1 != c2) {
			return (int)c1 - (int)c2;
		}

		if (c1 == 0) {
			return 0;
		}

		i++;
	}
}

static inline int strncmp(const char *s1, const char *s2, size_t n) {
	if (UNLIKELY(s1 == NULL)) { panic("STRING", "strncmp s1 is null"); }
	if (UNLIKELY(s2 == NULL)) { panic("STRING", "strncmp s2 is null"); }
	if (UNLIKELY(!range_is_safe(s1, n))) { panic("STRING", "strncmp s1 is out of range"); }
	if (UNLIKELY(!range_is_safe(s2, n))) { panic("STRING", "strncmp s2 is out of range"); }

	for (size_t i = 0; i < n; i++) {
		unsigned char c1 = (unsigned char)s1[i];
		unsigned char c2 = (unsigned char)s2[i];

		if (c1 != c2) {
			return (int)c1 - (int)c2;
		}

		if (c1 == 0) {
			return 0;
		}
	}
	return 0;
}

static inline char *strncpy(char *dest, const char *src, size_t n) {
	if (UNLIKELY(dest == NULL)) { panic("STRING", "strncpy dest is null"); }
	if (UNLIKELY(src == NULL)) { panic("STRING", "strncpy src is null"); }
	if (UNLIKELY(!range_is_safe(dest, n))) { panic("STRING", "strncpy dest is out of range"); }
	if (UNLIKELY(!range_is_safe(src, n))) { panic("STRING", "strncpy src is out of range"); }

	size_t i = 0;

	for (; i < n && src[i]; i++) {
		dest[i] = src[i];
	}

	for (; i < n; i++) {
		dest[i] = 0;
	}

	return dest;
}
