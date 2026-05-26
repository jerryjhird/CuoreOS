#pragma once

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define UNUSED(x) ((void)(x))
#define ALIGN(alignment, size) (((size) + ((alignment) - 1)) & ~((alignment) - 1))
#define GET_CONTAINER(ptr, type, member) ((type *)((uintptr_t)(ptr) - offsetof(type, member)))
#define SMAX(a, b) ((a) > (b) ? (a) : (b))
#define IP_ADDR(a, b, c, d) ((a) | (b << 8) | (c << 16) | (d << 24))

#define BIT_SET(var, mask) ((var) |=  (mask))
#define BIT_CLEAR(var, mask) ((var) &= ~(mask))
#define BIT_CHECK(var, mask) (((var) & (mask)) == (mask))
#define BIT_TOGGLE(var, mask) ((var) ^=  (mask))
