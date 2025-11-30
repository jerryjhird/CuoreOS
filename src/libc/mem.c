#include "stdint.h"

#define ALIGN_UP(x, a) (((x) + ((a)-1)) & ~((a)-1))

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

uint8_t* heap_ptr = (uint8_t*)0x100000; 
uint8_t* heap_end = (uint8_t*)0x200000;

void* kmalloc(size_t size) {
    uintptr_t ptr = (uintptr_t)heap_ptr;
    ptr = ALIGN_UP(ptr, 8); // 8-byte alignment
    if (ptr + size > (uintptr_t)heap_end) return NULL;

    heap_ptr = (uint8_t*)(ptr + size);
    return (void*)ptr;
}

void kfree(void* ptr, size_t size) {
    // free the last allocated block
    uintptr_t block = (uintptr_t)ptr;
    if (block + size == (uintptr_t)heap_ptr) {
        heap_ptr = (uint8_t*)block; // rewind heap pointer
    }
}

void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (!ptr) return NULL;        
    memset(ptr, 0, size);
    return ptr;
}

void kheapinit(uint8_t* start, uint8_t* end) {
    heap_ptr = start;
    heap_end = end;
}