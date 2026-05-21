#include "pci/pci.h"
typedef int dummy0;

// driver for rtl8139 ethernet controller. supports PMIO and MMIO
#include "Config.h"

#ifdef KERNEL_MOD_RTL8139_ENABLED

#include "rtl8139.h"

#include "mem/pma.h"
#include "cpu/io.h"
#include "mem/heap.h"
#include "mem/mem.h"
#include "logbuf.h"
#include "apic/ioapic.h"
#include "devices.h"
#include "mem/paging.h"
#include "panic.h"

static void rtl_write32(rtl8139_state_t* s, uint16_t reg, uint32_t val) {
	if (s->is_mmio) {
		*(volatile uint32_t*)(s->base + reg) = val;
	} else {
		outl((uint16_t)s->base + reg, val);
	}
}

static void rtl_write16(rtl8139_state_t* s, uint16_t reg, uint16_t val) {
	if (s->is_mmio) {
		*(volatile uint16_t*)(s->base + reg) = val;
	} else {
		outw((uint16_t)s->base + reg, val);
	}
}

static void rtl_write8(rtl8139_state_t* s, uint16_t reg, uint8_t val) {
	if (s->is_mmio) {
		*(volatile uint8_t*)(s->base + reg) = val;
	} else {
		outb((uint16_t)s->base + reg, val);
	}
}

static uint32_t rtl_read32(rtl8139_state_t* s, uint16_t reg) {
	if (s->is_mmio) {
		return *(volatile uint32_t*)(s->base + reg);
	}
	return inl((uint16_t)s->base + reg);
}

static uint16_t rtl_read16(rtl8139_state_t* s, uint16_t reg) {
	if (s->is_mmio) {
		return *(volatile uint16_t*)(s->base + reg);
	}
	return inw((uint16_t)s->base + reg);
}

static uint8_t rtl_read8(rtl8139_state_t* s, uint16_t reg) {
	if (s->is_mmio) {
		return *(volatile uint8_t*)(s->base + reg);
	}
	return inb((uint16_t)s->base + reg);
}

static void rtl_rx_drain(rtl8139_state_t* state, kernel_net_dev_t* dev) {
	while (!(rtl_read8(state, RTL_REG_COMMAND) & RTL_CMD_RECV_EMPTY)) {
		if (state->rx_offset >= RTL_RX_BUF_SIZE) {
			state->rx_offset = 0;
		}

		uint32_t header_val;
		memcpy(&header_val, state->rx_buffer + state->rx_offset, sizeof(uint32_t));

		uint16_t rx_flags = (uint16_t)(header_val & 0xFFFF);
		uint16_t packet_len = (uint16_t)(header_val >> 16);

		if (!(rx_flags & RTL_RX_ROK) || (rx_flags & RTL_RX_ERR_MASK)) {
			rtl_write8(state, RTL_REG_COMMAND, RTL_CMD_TX_ENABLE | RTL_CMD_RX_ENABLE);
			break;
		}

		if (packet_len < 64 || packet_len > 1518) {
			break;
		}

		uint8_t* packet_data = state->rx_buffer + state->rx_offset + 4;
		if (dev->on_received) {
			dev->on_received(dev, packet_data, (size_t)(packet_len - 4));
		}

		state->rx_offset = (state->rx_offset + packet_len + 4 + 3) & ~3u;
		state->rx_offset %= RTL_RX_BUF_SIZE;

		uint16_t capr = (state->rx_offset < 0x10)
			? (uint16_t)(RTL_RX_BUF_SIZE - (0x10 - state->rx_offset))
			: (uint16_t)(state->rx_offset - 0x10);
		rtl_write16(state, RTL_REG_CAPR, capr);
	}
}

static struct trap_frame* rtl8139_irq_handler(struct trap_frame* tf) {
	kernel_net_dev_t* dev = active_net_device;
	if (!dev || !dev->private_data) {
		return tf;
	}

	rtl8139_state_t* state = (rtl8139_state_t*)dev->private_data;
	if (state->magic != RTL8139_MAGIC) {
		return tf;
	}

	uint16_t isr = rtl_read16(state, RTL_REG_ISR);
	if (isr == 0 || isr == 0xFFFF) {
		return tf;
	}

	// ack all pending interrupts before processing
	rtl_write16(state, RTL_REG_ISR, isr);

	if (isr & RTL_INT_ROK) {
		rtl_rx_drain(state, dev);
	}

	if (isr & RTL_INT_TOK) {}

	if (isr & RTL_INT_TER) {
		for (int i = 0; i < RTL_TX_DESC_COUNT; i++) {
			uint32_t ts = rtl_read32(state, RTL_REG_TX_STATUS0 + (i * 4));
			if (ts & RTL_TX_ERR) {
				rtl_write32(state, RTL_REG_TX_STATUS0 + (i * 4), 0);
			}
		}
	}

	return tf;
}

static void rtl_send_packet(kernel_net_dev_t* dev, void* data, size_t len) {
	if (!dev || !dev->private_data || !data) {
		return;
	}

	rtl8139_state_t* state = (rtl8139_state_t*)dev->private_data;
	if (state->magic != RTL8139_MAGIC) {
		return;
	}

	if (!dev->initialized) {
		return;
	}

	if (len == 0 || len > 1514) {
		return;
	}

	uint8_t slot = state->tx_cur;
	uint16_t tx_status_reg = (uint16_t)(RTL_REG_TX_STATUS0 + (slot * 4));
	uint16_t tx_addr_reg = (uint16_t)(RTL_REG_TX_ADDR0 + (slot * 4));

	uint32_t ts;
	do {
		ts = rtl_read32(state, tx_status_reg);
		if (ts & RTL_TX_ERR) {
			rtl_write32(state, tx_status_reg, 0);
			break;
		}
		if (!(ts & RTL_TX_OWN)) {
			break;
		}
		__asm__ volatile("pause");
	} while (1);

	uint32_t slot_phys = (uint32_t)(state->tx_buffer_phys + (slot * RTL_TX_BUF_SIZE));
	uint8_t* slot_virt = (uint8_t*)((uintptr_t)slot_phys + hhdm_offset);

	memcpy(slot_virt, data, len);

	// ensure memcpy is not reordered past the nic poke
	__asm__ volatile("" ::: "memory");

	rtl_write32(state, tx_addr_reg, slot_phys);
	rtl_write32(state, tx_status_reg, len & 0x1FFF);

	state->tx_cur = (uint8_t)((slot + 1) % RTL_TX_DESC_COUNT);
}

static void rtl_set_promiscuous(kernel_net_dev_t* dev, bool enabled) {
	if (!dev || !dev->private_data) {
		return;
	}

	rtl8139_state_t* state = (rtl8139_state_t*)dev->private_data;
	if (state->magic != RTL8139_MAGIC) {
		return;
	}

	uint32_t rcr = enabled
		? (RTL_RCR_DEFAULT | RTL_RCR_AAP)
		: RTL_RCR_DEFAULT;

	rtl_write32(state, RTL_REG_RCR, rcr);
}

pci_driver_status rtl8139_init(pci_dev_t pdev) {
	rtl8139_state_t* state = zalloc(sizeof(rtl8139_state_t));

	state->magic = RTL8139_MAGIC;

	// BAR0 = PMIO base, BAR1 = MMIO base
	// prefer MMIO
	state->is_mmio = !pdev.bars[1].is_io && pdev.bars[1].base != 0;

	if (state->is_mmio) {
		uintptr_t phys = pdev.bars[1].base;
		uint64_t* pml4 = (uint64_t*)(vmm_get_pml4() + hhdm_offset);
		vmm_map_page(pml4, phys + hhdm_offset, phys,
			PTE_PRESENT | PTE_WRITABLE | PTE_TYPE_DRIVER | PTE_CACHE_DISABLE);
		state->base = phys + hhdm_offset;
	} else {
		if (!pdev.bars[0].is_io || pdev.bars[0].base == 0) {
			return INVALID_BAR;
		}
		state->base = pdev.bars[0].base;
	}

	// reset controller and spin until the reset bit self clears
	rtl_write8(state, RTL_REG_COMMAND, RTL_CMD_RESET);
	uint32_t timeout = 100000;

	while ((rtl_read8(state, RTL_REG_COMMAND) & RTL_CMD_RESET) && timeout--) {
		__asm__ volatile("pause");
	}

	if (rtl_read8(state, RTL_REG_COMMAND) & RTL_CMD_RESET) {
		return CARD_HARDWARE_STALLING;
	}

	// rx buffer: 8k + wrap guard pages
	state->rx_buffer_phys = pma_alloc_pages_low(RTL_RX_BUF_PAGES);
	if (!state->rx_buffer_phys) {
		return DOWNLOAD_MORE_RAM_32BIT;
	}

	state->rx_buffer = (uint8_t*)(state->rx_buffer_phys + hhdm_offset);
	state->rx_offset = 0;
	memset(state->rx_buffer, 0, RTL_RX_BUF_PAGES * 4096);
	rtl_write32(state, RTL_REG_RBSTART, (uint32_t)state->rx_buffer_phys);

	// tx buffers: 4 * 2k descriptors
	state->tx_buffer_phys = pma_alloc_pages_low(RTL_TX_BUF_PAGES);
	if (!state->tx_buffer_phys) {
		return DOWNLOAD_MORE_RAM_32BIT;
	}

	state->tx_cur = 0;

	// write tx physical base addresses for all 4 descriptors upfront
	for (int i = 0; i < RTL_TX_DESC_COUNT; i++) {
		uint32_t slot_phys = (uint32_t)(state->tx_buffer_phys + (i * RTL_TX_BUF_SIZE));
		rtl_write32(state, RTL_REG_TX_ADDR0 + (i * 4), slot_phys);
	}

	rtl_write16(state, RTL_REG_CAPR, 0);
	rtl_write16(state, RTL_REG_IMR,RTL_INT_ROK | RTL_INT_TOK | RTL_INT_TER | RTL_INT_RXOVW | RTL_INT_LNKCHG);

	rtl_write32(state, RTL_REG_RCR, RTL_RCR_DEFAULT);
	rtl_write8(state, RTL_REG_COMMAND, RTL_CMD_TX_ENABLE | RTL_CMD_RX_ENABLE);

	uint8_t vector = ioapic_map_irq_to_free_vector(pdev.irq, 0, 0);
	irq_install_handler(0, vector, rtl8139_irq_handler);

	kernel_net_dev_t* ndev = zalloc(sizeof(kernel_net_dev_t));

	for (int i = 0; i < 6; i++) {
		ndev->mac[i] = rtl_read8(state, RTL_REG_MAC + i);
	}

	strncpy(ndev->model, "RTL8139", sizeof(ndev->model));
	ndev->private_data = state;
	ndev->send_packet = rtl_send_packet;
	ndev->set_promiscuous = rtl_set_promiscuous;

	__asm__ volatile("" ::: "memory");

	ndev->initialized = true;
	active_net_device = ndev;

	logbuf_printf("[ ETH  ] Initialized %s with MAC: %M\n", ndev->model, ndev->mac);
	return DRIVER_OK;
}

#endif
