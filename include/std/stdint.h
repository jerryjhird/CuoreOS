#ifndef STDINT_H
#define STDINT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed long long   int64_t;
typedef unsigned long size_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef uint64_t uintptr_t;
typedef int64_t  intptr_t;

#ifdef __cplusplus
}
#endif

#endif // STDINT_H
