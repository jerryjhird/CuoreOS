#include <stdint.h>
#include "devicetypes.h"

void ptrthex(char *buf, uint64_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex_chars[val & 0xF];
        val >>= 4;
    }
    buf[16] = 0;
}

void puthex(output_dev_t *dev, uint64_t val) {
    const char *hex_chars = "0123456789ABCDEF";
    dev->write("0x");

    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (val >> (i * 4)) & 0xF;
        dev->putc(hex_chars[nibble]);
    }
}
