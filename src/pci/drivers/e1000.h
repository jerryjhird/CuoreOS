#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "pci/pci.h"
#include "mem/dmalloc.h"

#define E1000_REG_CTRL 0x0000 // device control register
#define E1000_REG_STATUS 0x0008 // device status register
#define E1000_REG_EERD 0x0014 // eeprom read register
#define E1000_REG_ICR 0x00C0 // interrupt cause read register
#define E1000_REG_IMS 0x00D0 // interrupt mask set register
#define E1000_REG_IMC 0x00D8 // interrupt mask clear register
#define E1000_REG_RCTL 0x0100 // receive control register
#define E1000_REG_TCTL 0x0400 // transmit control register
#define E1000_REG_TIPG 0x0410 // transmit interpacket gap register
#define E1000_REG_RDBAL 0x2800 // receive descriptor base low register
#define E1000_REG_RDBAH 0x2804 // receive descriptor base high register
#define E1000_REG_RDLEN 0x2808 // receive descriptor length register
#define E1000_REG_RDH 0x2810 // receive descriptor head register
#define E1000_REG_RDT 0x2818 // receive descriptor tail register
#define E1000_REG_RDTR 0x2820 // receive delay timer register
#define E1000_REG_RADV 0x282C // receive interrupt absolute delay register
#define E1000_REG_TDBAL 0x3800 // transmit descriptor base low register
#define E1000_REG_TDBAH 0x3804 // transmit descriptor base high register
#define E1000_REG_TDLEN 0x3808 // transmit descriptor length register
#define E1000_REG_TDH 0x3810 // transmit descriptor head register
#define E1000_REG_TDT 0x3818 // transmit descriptor tail register
#define E1000_REG_TIDV 0x3820 // transmit interrupt delay value register
#define E1000_REG_TADV 0x382C // transmit absolute interrupt delay register
#define E1000_REG_MTA 0x5200 // multicast table array base register up to 0x53fc
#define E1000_REG_RAL 0x5400 // receive address low array base register
#define E1000_REG_RAH 0x5404 // receive address high array base register
#define E1000_CTRL_FD 0x00000001 // full duplex
#define E1000_CTRL_ASDE 0x00000020 // autospeed detection enable
#define E1000_CTRL_SLU 0x00000040 // set link up
#define E1000_CTRL_FRCSPD 0x00000800 // force speed
#define E1000_CTRL_FRCDPX 0x00001000 // force duplex
#define E1000_CTRL_RST 0x04000000 // device reset
#define E1000_STATUS_LU 0x00000002 // link up status
#define E1000_EERD_START 0x00000001 // start eeprom read
#define E1000_EERD_DONE 0x00000010 // eeprom read done
#define E1000_IRQ_TXDW 0x00000001 // transmit descriptor written back
#define E1000_IRQ_TXQE 0x00000002 // transmit queue empty
#define E1000_IRQ_LSC 0x00000004 // link status change
#define E1000_IRQ_RXSEQ 0x00000008 // receive sequence error
#define E1000_IRQ_RXT0 0x00000080 // receiver timer interrupt
#define E1000_IMS_ALL 0x0001F6DC // standard flag mask for driver operations
#define E1000_RCTL_EN 0x00000002 // receiver enable
#define E1000_RCTL_SBP 0x00000004 // store bad packets
#define E1000_RCTL_UPE 0x00000008 // unicast promiscuous enable
#define E1000_RCTL_MPE 0x00000010 // multicast promiscuous enable
#define E1000_RCTL_LPE 0x00000020 // long packet enable
#define E1000_RCTL_LBM_NONE 0x00000000 // no loopback
#define E1000_RCTL_RDMTS_HALF 0x00000000 // receive descriptor minimum threshold size half
#define E1000_RCTL_BAM 0x00008000 // broadcast accept mode
#define E1000_RCTL_BSIZE_2048 0x00000000 // receive buffer size 2048 bytes
#define E1000_RCTL_SECRC 0x04000000 // strip ethernet crc
#define E1000_TCTL_EN 0x00000002 // transmit enable
#define E1000_TCTL_PSP 0x00000008 // pad short packets
#define E1000_TCTL_CT_SHIFT 4 // collision threshold position shift
#define E1000_TCTL_COLD_SHIFT 12 // collision distance position shift
#define E1000_TXD_CMD_EOP 0x01 // end of packet command bit
#define E1000_TXD_CMD_IFCS 0x02 // insert fcscrc command bit
#define E1000_TXD_CMD_IC 0x04 // insert checksum command bit
#define E1000_TXD_CMD_RS 0x08 // report status command bit
#define E1000_NUM_RX_DESC 256 // number of receive descriptors
#define E1000_NUM_TX_DESC 256 // number of transmit descriptors
#define E1000_DEFAULT_MTU 2048 // default safe buffer size

struct e1000_rx_desc {
	uint64_t addr;
	uint16_t length;
	uint16_t checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
} __attribute__((packed));

typedef struct {
	uintptr_t mmio_virt;
	uint8_t mac[6];
	struct e1000_rx_desc* rx_ring;
	uint64_t rx_ring_phys;
	dmalloc_ret_t rx_bufs[E1000_NUM_RX_DESC];
	uint16_t rx_cur;
	struct e1000_tx_desc* tx_ring;
	uint64_t tx_ring_phys;
	dmalloc_ret_t tx_bufs[E1000_NUM_TX_DESC];
	uint16_t tx_cur;
} e1000_state_t;

typedef struct kernel_net_dev_t kernel_net_dev_t;

pci_driver_status e1000_init(pci_dev_t dev);
