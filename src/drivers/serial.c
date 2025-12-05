#include "stddef.h"
#include "x86.h"
#include "drivers/serial.h"

void serial_init(void) {
    outb(SERIAL_COM1 + 1, 0x00);    // disable interrupts
    outb(SERIAL_COM1 + 3, 0x80);    // enable DLAB
    outb(SERIAL_COM1 + 0, 0x01);    // divisor low byte (115200 baud)
    outb(SERIAL_COM1 + 1, 0x00);    // divisor high byte
    outb(SERIAL_COM1 + 3, 0x03);    
    outb(SERIAL_COM1 + 2, 0xC7);    // enable FIFO
    outb(SERIAL_COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

// wait for transmitter to be empty
static void serial_wait_tx(void) {
    while (!(inb(SERIAL_COM1 + 5) & 0x20)) {}
}

static void serial_putc(char c) {
    serial_wait_tx();
    outb(SERIAL_COM1, (uint8_t)c);
}

void serial_write(const char *msg, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (msg[i] == '\n') serial_putc('\r');
        serial_putc(msg[i]);
    }
}
