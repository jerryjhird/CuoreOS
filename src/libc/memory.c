#include "stdint.h"
#include "stdbool.h"

#define ALIGN_UP(x, a) (((x) + (uintptr_t)((a)-1)) & ~((uintptr_t)((a)-1)))

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }

    return dest;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void ptrhex(char *buf, void *ptr) {
    uintptr_t val = (uintptr_t)ptr;
    const char hex[] = "0123456789ABCDEF";
    for (int i = (sizeof(void*)*2)-1; i >= 0; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[sizeof(void*)*2] = 0;
}

uint8_t* heap_ptr = (uint8_t*)0x100000;  // placeholder (these are set using heapinit and no heap allocation should be done beforehand)
uint8_t* heap_end = (uint8_t*)0x200000; // placeholder

bool heap_can_alloc(size_t size) {
    uintptr_t ptr = (uintptr_t)heap_ptr;
    ptr = ALIGN_UP(ptr, 8);

    size_t total_size = size + sizeof(size_t);
    return (ptr + total_size <= (uintptr_t)heap_end);
}

void* malloc(size_t size) {
    if (!size) return NULL;

    // space for storing size before the block
    size_t total_size = size + sizeof(size_t);

    uintptr_t ptr = (uintptr_t)heap_ptr;
    ptr = ALIGN_UP(ptr, 8); // 8-byte alignment

    if (ptr + total_size > (uintptr_t)heap_end) return NULL;

    // store size before block
    *((size_t*)ptr) = size;

    heap_ptr = (uint8_t*)(ptr + total_size);
    return (void*)(ptr + sizeof(size_t)); // return pointer after size
}

void sfree(void* ptr, size_t size) {
    uintptr_t block = (uintptr_t)ptr;
    if (block + size == (uintptr_t)heap_ptr) {
        heap_ptr = (uint8_t*)block; // rewind heap pointer
    }
}

void free(void* user_ptr) {
    if (!user_ptr) return;

    uintptr_t block_ptr = (uintptr_t)user_ptr;
    size_t size = *((size_t*)(block_ptr - sizeof(size_t)));
    uintptr_t block_start = block_ptr - sizeof(size_t);

    sfree((void*)block_start, size + sizeof(size_t));
}

void* zalloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) return NULL;        
    memset(ptr, 0, size);
    return ptr;
}

void heapinit(uint8_t* start, uint8_t* end) {
    heap_ptr = start;
    heap_end = end;
}