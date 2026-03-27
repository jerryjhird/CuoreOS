#include <stdint.h>
#include <stddef.h>
#include "cpu/instruction_helpers.h"
#include "devices.h"
#include "devicetypes.h"
#include "kstate.h"
#include "UART16550.h"

#define UART_COM1 0x3F8

#define UART_DATA    0
#define UART_IER     1
#define UART_FCR     2
#define UART_LCR     3
#define UART_MCR     4
#define UART_LSR     5

#define UART_LSR_RX_READY 0x01
#define UART_LSR_TX_EMPTY 0x20

SETUP_OUTPUT_DEVICE(uart16550_dev, 
    CAP_ANSI_4BIT  | CAP_ON_ERROR,
    uart16550_putc, uart16550_write
);

void uart16550_init(void) {
    REGISTER_OUTPUT_DEVICE(&uart16550_dev, output_devices, output_devices_c);

    outb(UART_COM1 + UART_IER, 0x00);
    outb(UART_COM1 + UART_LCR, 0x80);
    outb(UART_COM1 + UART_DATA, 0x01);
    outb(UART_COM1 + UART_IER, 0x00);
    outb(UART_COM1 + UART_LCR, 0x03);
    outb(UART_COM1 + UART_FCR, 0xC7);
    outb(UART_COM1 + UART_MCR, 0x0B);
}

void uart16550_postinit(void) {
    if (global_kernel_config.uart16550_is_debug_interface) {DEV_CAP_SET(&uart16550_dev, CAP_ON_DEBUG);}
}

static void uart16550_wait_tx(void) {
    uint32_t timeout = 1000000; 
    while (!(inb(UART_COM1 + UART_LSR) & UART_LSR_TX_EMPTY) && --timeout > 0);
}

static void uart16550_wait_rx(void) {while (!(inb(UART_COM1 + UART_LSR) & UART_LSR_RX_READY)) {}}

char uart16550_getc(void) {
    uart16550_wait_rx();
    return (char)inb(UART_COM1 + UART_DATA);
}

void uart16550_putc(char c) {
    uart16550_wait_tx();
    outb(UART_COM1 + UART_DATA, (uint8_t)c);
}

void uart16550_lwrite(const char *msg, size_t len) {
    if (!msg) return;
    for (size_t i = 0; i < len; i++) {
        if (msg[i] == '\n') uart16550_putc('\r');
        uart16550_putc(msg[i]);
    }
}

void uart16550_write(const char* str) {
    if (!str) return;
    
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }

    uart16550_lwrite(str, len);
}

void uart16550_puthex(uint64_t val) {
    const char *hex_chars = "0123456789ABCDEF";

    uart16550_lwrite("0x", 2);

    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (val >> (i * 4)) & 0xF;
        uart16550_putc(hex_chars[nibble]);
    }
}