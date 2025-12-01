#include "x86.h"
#include "drivers/ps2.h"

static uint8_t read_scancode(void) {
    while (!(inb(KBD_STATUS_PORT) & 0x01)) {
        // slep
    }
    return inb(KBD_DATA_PORT);
}

char getc(void) {
    while (1) {
        uint8_t sc = read_scancode();
        if (sc & 0x80) continue;
        char c = scta_uk[sc];
        if (c != 0) return c;
    }
}

bool ps2_dev_exists(uint8_t port) {
    if (port == 2) {
        outb(0x64, 0xA8);
    }

    outb(0x60, 0xFF);
    uint8_t ack = inb(0x60);
    uint8_t selftest = inb(0x60);

    return (ack == 0xFA && selftest == 0xAA);
}

int ps2_dev_count(void) {
    int count = 0;
    if (ps2_dev_exists(1)) count++;
    if (ps2_dev_exists(2)) count++;
    return count;
}
