#ifndef X86_H
#define X86_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint8_t cmos_read(uint8_t reg);
void get_cpu(char *buf);

#ifdef __cplusplus
}
#endif

#endif // X86_H