#include <stdint.h>
#include <stddef.h>
#include "devices.h"
#include "kstate.h"
#include "drivers/UART16550.h"
#include "cpu/io.h"
#include "cpu/IRQ.h"
#include "apic/lapic.h"

#define UART_COM1 0x3F8

#define UART_DATA	0
#define UART_IER	 1
#define UART_FCR	 2
#define UART_LCR	 3
#define UART_MCR	 4
#define UART_LSR	 5

#define UART_LSR_RX_READY 0x01
#define UART_LSR_TX_EMPTY 0x20

#define RX_BUF_SIZE 1024
static char rx_buffer[RX_BUF_SIZE];
static volatile uint32_t rx_read_ptr = 0;
static volatile uint32_t rx_write_ptr = 0;

struct trap_frame* uart16550_irq_handler(struct trap_frame *tf) {
	while (inb(UART_COM1 + UART_LSR) & UART_LSR_RX_READY) {
		char c = (char)inb(UART_COM1 + UART_DATA);

		uint32_t next = (rx_write_ptr + 1) % RX_BUF_SIZE;
		if (next != rx_read_ptr) {
			rx_buffer[rx_write_ptr] = c;
			rx_write_ptr = next;
		}
	}
	return tf;
}

SETUP_OUTPUT_DEVICE(uart16550_dev,
	CAP_ON_ERROR,
	uart16550_putc, NULL, NULL
);

void uart16550_init(void) {
	REGISTER_OUTPUT_DEVICE(&uart16550_dev, output_devices, output_devices_c);
	if (global_kernel_config.uart16550_is_debug_interface) {DEV_CAP_SET(&uart16550_dev, CAP_ON_DEBUG);}

	rx_read_ptr = 0; rx_write_ptr = 0;
	irq_install_handler(lapic_get_id(), 36, uart16550_irq_handler);

	outb(UART_COM1 + UART_IER, 0x00);
	outb(UART_COM1 + UART_LCR, 0x80);
	outb(UART_COM1 + UART_DATA, 0x01);
	outb(UART_COM1 + UART_IER, 0x00);
	outb(UART_COM1 + UART_LCR, 0x03);
	outb(UART_COM1 + UART_FCR, 0xC7);
	outb(UART_COM1 + UART_MCR, 0x0B);
	outb(UART_COM1 + UART_IER, 0x01);
}

static void uart16550_wait_tx(void) {while (!(inb(UART_COM1 + UART_LSR) & UART_LSR_TX_EMPTY));}

char uart16550_getc(void) {
	while (rx_read_ptr == rx_write_ptr) {
		__asm__ volatile("hlt");
	}

	char c = rx_buffer[rx_read_ptr];
	rx_read_ptr = (rx_read_ptr + 1) % RX_BUF_SIZE;
	return c;
}

void uart16550_putc(char c) {
	uart16550_wait_tx();
	outb(UART_COM1 + UART_DATA, (uint8_t)c);
}
