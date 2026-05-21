#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pci/pci.h"

#define RTL8139_MAGIC 0x52544C39

#define RTL_REG_MAC 0x00
#define RTL_REG_TX_STATUS0 0x10
#define RTL_REG_TX_ADDR0 0x20
#define RTL_REG_RBSTART 0x30
#define RTL_REG_COMMAND 0x37
#define RTL_REG_CAPR 0x38
#define RTL_REG_IMR 0x3C
#define RTL_REG_ISR 0x3E
#define RTL_REG_RCR 0x44

#define RTL_CMD_RECV_EMPTY 0x01
#define RTL_CMD_TX_ENABLE 0x04
#define RTL_CMD_RX_ENABLE 0x08
#define RTL_CMD_RESET 0x10

#define RTL_INT_ROK 0x0001
#define RTL_INT_TOK 0x0002
#define RTL_INT_TER 0x0004
#define RTL_INT_RXOVW 0x0010
#define RTL_INT_LNKCHG 0x0020

#define RTL_RX_ROK 0x0001
#define RTL_RX_ERR_MASK 0x003E

#define RTL_TX_ERR 0x40000000
#define RTL_TX_OWN 0x20000000

#define RTL_RX_BUF_PAGES 3
#define RTL_RX_BUF_SIZE 8192
#define RTL_TX_BUF_PAGES 2
#define RTL_TX_BUF_SIZE 2048
#define RTL_TX_DESC_COUNT 4

#define RTL_RCR_AAP 0x00000001
#define RTL_RCR_APM 0x00000002
#define RTL_RCR_AM 0x00000004
#define RTL_RCR_AB 0x00000008
#define RTL_RCR_MXDMA_UNLIM 0x00000700
#define RTL_RCR_DEFAULT (RTL_RCR_APM | RTL_RCR_AM | RTL_RCR_AB | RTL_RCR_MXDMA_UNLIM)

typedef struct rtl8139_state {
	uint32_t magic;
	uintptr_t base;
	bool is_mmio;
	uintptr_t rx_buffer_phys;
	uint8_t* rx_buffer;
	uint32_t rx_offset;
	uintptr_t tx_buffer_phys;
	uint8_t tx_cur;
} rtl8139_state_t;

pci_driver_status rtl8139_init(pci_dev_t pdev);
