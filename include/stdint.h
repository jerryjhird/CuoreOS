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

// unsigned fast
typedef unsigned int uint_fast8_t;
typedef unsigned int uint_fast16_t;
typedef unsigned int uint_fast32_t;

// signed fast
typedef signed int   int_fast8_t;
typedef signed int   int_fast16_t;
typedef signed int   int_fast32_t;

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

#ifdef __cplusplus
}
#endif

#endif // STDINT_H
