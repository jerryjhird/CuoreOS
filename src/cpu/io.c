#include <stdint.h>

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) );
}

uint8_t inb(uint16_t port) {
    uint8_t ret; 
    __asm__ volatile ( "inb %w1, %b0" : "=a"(ret) : "Nd"(port) ); 
    return ret;
}

void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ( "outw %w0, %w1" : : "a"(val), "Nd"(port) );
}

uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ( "inw %w1, %w0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ( "outl %0, %w1" : : "a"(val), "Nd"(port) );
}

uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ( "inl %w1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

void insw(uint16_t port, void *addr, uint32_t count) {
    __asm__ volatile ("cld; rep insw" :
                      "=D" (addr), "=c" (count) :
                      "d" (port), "0" (addr), "1" (count) :
                      "memory", "cc");
}

void outsw(uint16_t port, const void *addr, uint32_t count) {
    __asm__ volatile ("cld; rep outsw" :
                      "=S" (addr), "=c" (count) :
                      "d" (port), "0" (addr), "1" (count) :
                      "memory", "cc");
}