#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

__attribute__((noinline, optimize("O0")))
unsigned int crc32c_hwhash(const char *s) {
    unsigned int h = 0;
    while (*s) {
        unsigned char c = (unsigned char)(*s++);
        asm volatile ("crc32b %1, %0" : "+r"(h) : "rm"(c));
    }
    return h;
}

uint32_t crc32c_swhash(const char *s)
{
    const uint32_t poly = 0x82F63B78;
    uint32_t crc = 0;

    while (*s) {
        uint32_t c = (uint8_t)*s++;
        crc ^= c;

        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (poly & -(crc & 1));
        }
    }

    return crc;
}


int main(int argc, char *argv[]) {
    if (argc < 2)
        return 1;

    for (int i = 1; i < argc; i++) {
        const char *s = argv[i];

        uint32_t hw = crc32c_hwhash(s);
        uint32_t sw = crc32c_swhash(s);

        printf("\"%s\"\n", s);
        printf("  hw crc32  = 0x%08X\n", hw);
        printf("  sw crc32c = 0x%08X\n", sw);
    }

    return 0;
}
