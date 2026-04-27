#pragma once
#include <stdint.h>
#include "pci/pci.h"

// RTL8139 Register Map
#define RTL_REG_MAC		  0x00 // MAC Address (6 bytes)
#define RTL_REG_MAR		  0x08 // Multicast Registers (8 bytes)
#define RTL_REG_TX_STATUS0   0x10 // Transmit Status (4 x 4 bytes)
#define RTL_REG_TX_ADDR0	 0x20 // Transmit Start Address (4 x 4 bytes)
#define RTL_REG_RBSTART	  0x30 // Receive Buffer Start Address
#define RTL_REG_COMMAND	  0x37 // Command Register
#define RTL_REG_CAPR		 0x38 // Current Address of Packet Read
#define RTL_REG_IMR		  0x3C // Interrupt Mask Register
#define RTL_REG_ISR		  0x3E // Interrupt Status Register
#define RTL_REG_TCR		  0x40 // Transmit Configuration Register
#define RTL_REG_RCR		  0x44 // Receive Configuration Register
#define RTL_REG_CONFIG1	  0x52 // Configuration Register 1

// Register Bits
#define RTL_CMD_RECV_EMPTY   0x01
#define RTL_CMD_TX_ENABLE	0x04
#define RTL_CMD_RX_ENABLE	0x08
#define RTL_CMD_RESET		0x10

#define RTL_INT_ROK		  0x01 // Receive OK
#define RTL_INT_TER		  0x02 // Transmit Error
#define RTL_INT_TOK		  0x04 // Transmit OK

// Buffer sizes
#define RX_BUF_SIZE		  8192
#define RX_BUF_PADDING	   1536 // Max packet size for the "wrap" area
#define RX_BUF_ALLOC_SIZE	(RX_BUF_SIZE + 16 + RX_BUF_PADDING)

typedef struct {
	uint32_t magic;
	uintptr_t base;
	bool is_mmio;
	uint8_t* rx_buffer;
	uint32_t rx_offset;
	uintptr_t rx_buffer_phys;
	int tx_cur;
	uint8_t* tx_buffer_virt;
	uintptr_t tx_buffer_phys;
} rtl8139_state_t;

void rtl8139_init(pci_dev_t dev);
