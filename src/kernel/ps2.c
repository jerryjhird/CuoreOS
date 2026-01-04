#include "arch/x86_64.h"
#include "drivers/ps2.h"

static bool shift_state = false;
static bool caps_lock_state = false;

const char scta_uk[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'#',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
};

const char scta_uk_shift[128] = {
    0, 27, '!','"','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','@','~',0,'#',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',
};

static void wait_kbd_bit(uint8_t bit, int set) {
    if (set) {
        // bit is set
        while (!(inb(KBD_STATUS_PORT) & bit)) {}
    } else {
        // bit is clear
        while (inb(KBD_STATUS_PORT) & bit) {}
    }
}

static uint8_t read_scancode(void) {
    wait_kbd_bit(0x01, 1);
    return inb(KBD_DATA_PORT);
}

char ps2_getc(void) {
    while (1) {
        uint8_t sc = read_scancode();
        bool released = sc & 0x80;
        sc &= 0x7F;

        if (sc == 42 || sc == 54) { // l/r shift
            shift_state = !released;
            continue;
        }
        if (sc == 58 && !released) {
            caps_lock_state = !caps_lock_state;
            continue;
        }

        if (released) continue;

        char c = shift_state ? scta_uk_shift[sc] : scta_uk[sc];

        if (c >= 'a' && c <= 'z') {
            if (caps_lock_state && !shift_state) c -= 32;
            if (caps_lock_state && shift_state) c += 32;
        }

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