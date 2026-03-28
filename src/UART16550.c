#include <stdint.h>
#include <stddef.h>
#include "devices.h"
#include "kstate.h"
#include "UART16550.h"
#include "cpu/io.h"
#include "cpu/IRQ.h"

#define UART_COM1 0x3F8

#define UART_DATA    0
#define UART_IER     1
#define UART_FCR     2
#define UART_LCR     3
#define UART_MCR     4
#define UART_LSR     5

#define UART_LSR_RX_READY 0x01
#define UART_LSR_TX_EMPTY 0x20

#define RX_BUF_SIZE 1024
static char rx_buffer[RX_BUF_SIZE];
static volatile uint32_t rx_read_ptr = 0;
static volatile uint32_t rx_write_ptr = 0;

void uart16550_irq_handler(struct trap_frame *tf) {
    (void)tf;

    while (inb(UART_COM1 + UART_LSR) & UART_LSR_RX_READY) {
        char c = (char)inb(UART_COM1 + UART_DATA);

        uint32_t next = (rx_write_ptr + 1) % RX_BUF_SIZE;
        if (next != rx_read_ptr) {
            rx_buffer[rx_write_ptr] = c;
            rx_write_ptr = next;
        }
    }
}

SETUP_OUTPUT_DEVICE(uart16550_dev,
    CAP_ANSI_4BIT | CAP_ON_ERROR,
    uart16550_putc, uart16550_write
);

void uart16550_init(void) {
    REGISTER_OUTPUT_DEVICE(&uart16550_dev, output_devices, output_devices_c); 

    rx_read_ptr = 0; rx_write_ptr = 0;
    irq_install_handler(36, uart16550_irq_handler);

    outb(UART_COM1 + UART_IER, 0x00);
    outb(UART_COM1 + UART_LCR, 0x80);
    outb(UART_COM1 + UART_DATA, 0x01);
    outb(UART_COM1 + UART_IER, 0x00);
    outb(UART_COM1 + UART_LCR, 0x03);
    outb(UART_COM1 + UART_FCR, 0xC7);
    outb(UART_COM1 + UART_MCR, 0x0B);
    outb(UART_COM1 + UART_IER, 0x01);
}

void uart16550_postinit(void) {
    if (global_kernel_config.uart16550_is_debug_interface) {DEV_CAP_SET(&uart16550_dev, CAP_ON_DEBUG);}
}

char uart16550_getc(void) {
    while (rx_read_ptr == rx_write_ptr) {
        __asm__ volatile("hlt"); 
    }

    char c = rx_buffer[rx_read_ptr];
    rx_read_ptr = (rx_read_ptr + 1) % RX_BUF_SIZE;
    return c;
}

static void uart16550_wait_tx(void) {
    while (!(inb(UART_COM1 + UART_LSR) & UART_LSR_TX_EMPTY));
}

void uart16550_putc(char c) {
    uart16550_wait_tx();
    outb(UART_COM1 + UART_DATA, (uint8_t)c);
}

void uart16550_write(const char* str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') uart16550_putc('\r');
        uart16550_putc(str[i]);
    }
}

void uart16550_puthex(uint64_t val) {
    const char *hex_chars = "0123456789ABCDEF";
    uart16550_write("0x");
    for (int i = 15; i >= 0; i--) {
        uart16550_putc(hex_chars[(val >> (i * 4)) & 0xF]);
    }
}