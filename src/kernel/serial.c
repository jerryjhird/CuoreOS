/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include <stdint.h>
#include "x86.h"
#include <stddef.h>
#include "serial.h"

void serial_init(void) {
    outb(0x3F8 + 1, 0x00);    // disable interrupts
    outb(0x3F8 + 3, 0x80);    // enable DLAB
    outb(0x3F8 + 0, 0x01);    // divisor low byte (115200 baud)
    outb(0x3F8 + 1, 0x00);    // divisor high byte
    outb(0x3F8 + 3, 0x03);    
    outb(0x3F8 + 2, 0xC7);    // enable FIFO
    outb(0x3F8 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

// wait for transmitter to be empty
static void serial_wait_tx(void) {
    while (!(inb(0x3F8 + 5) & 0x20)) {}
}

static void serial_putc(char c) {
    serial_wait_tx();
    outb(0x3F8, (uint8_t)c);
}

void serial_write(const char *msg, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (msg[i] == '\n') serial_putc('\r');
        serial_putc(msg[i]);
    }
}
