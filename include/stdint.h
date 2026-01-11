/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef STDINT_H
#define STDINT_H

#ifdef __cplusplus
extern "C" {
#endif

// 8-bit
typedef unsigned char      uint8_t;
typedef signed char        int8_t;

// 16-bit
typedef unsigned short     uint16_t;
typedef signed short       int16_t;

// 32-bit
typedef unsigned int       uint32_t;
typedef int                int32_t;

// 64-bit
typedef unsigned long long uint64_t;
typedef signed long long   int64_t;

// pointer-sized
typedef unsigned long      size_t;
typedef uint64_t           uintptr_t;
typedef int64_t            intptr_t;

// max-width
typedef uint64_t           uintmax_t;
typedef int64_t            intmax_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define MAX_INT_STR_SIZE 21  // 19 digits + 1 sign + 1 null terminator
#define MAX_UINT_STR_SIZE 21  // 20 digits + 1 null terminator

#ifdef __cplusplus
}
#endif

#endif // STDINT_H
