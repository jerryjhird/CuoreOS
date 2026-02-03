/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef MEMORY_H
#define MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void heapinit(uint8_t* start, uint8_t* end);
bool heap_can_alloc(size_t size); // check if size can be allocated to heap (used internally by malloc, zalloc etc)

void* malloc(size_t size);
void* zalloc(size_t size);
void free(void* ptr);
void sfree(void* ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_H