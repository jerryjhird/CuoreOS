typedef int dummy0 ;

// driver for rtl8139 ethernet controller. supports PMIO and MMIO

#ifdef KERNEL_MOD_RTL8139_ENABLED

#include "rtl8139.h"

#include "mem/pma.h"
#include "cpu/io.h"
#include "mem/heap.h"
#include "builtinabs.h"
#include "mem/mem.h"
#include "logbuf.h"
#include "apic/ioapic.h"
#include "devices.h"
#include "mem/paging.h"

void panic(const char* header_msg, const char* msg);

static void rtl_write32(rtl8139_state_t* s, uint16_t reg, uint32_t val) {
	if (s->is_mmio) *(volatile uint32_t*)(s->base + reg) = val;
	else outl((uint16_t)s->base + reg, val);
}

static void rtl_write16(rtl8139_state_t* s, uint16_t reg, uint16_t val) {
	if (s->is_mmio) *(volatile uint16_t*)(s->base + reg) = val;
	else outw((uint16_t)s->base + reg, val);
}

static void rtl_write8(rtl8139_state_t* s, uint16_t reg, uint8_t val) {
	if (s->is_mmio) *(volatile uint8_t*)(s->base + reg) = val;
	else outb((uint16_t)s->base + reg, val);
}

static uint32_t rtl_read32(rtl8139_state_t* s, uint16_t reg) {
	if (s->is_mmio) return *(volatile uint32_t*)(s->base + reg);
	return inl((uint16_t)s->base + reg);
}

static uint16_t rtl_read16(rtl8139_state_t* s, uint16_t reg) {
	if (s->is_mmio) return *(volatile uint16_t*)(s->base + reg);
	return inw((uint16_t)s->base + reg);
}

static uint8_t rtl_read8(rtl8139_state_t* s, uint16_t reg) {
	if (s->is_mmio) return *(volatile uint8_t*)(s->base + reg);
	return inb((uint16_t)s->base + reg);
}

static struct trap_frame* rtl8139_irq_handler(struct trap_frame* tf) {
	kernel_net_dev_t* dev = active_net_device;
	if (!dev || !dev->private_data) return tf;

	rtl8139_state_t* state = (rtl8139_state_t*)dev->private_data;
	uint16_t isr = rtl_read16(state, RTL_REG_ISR);

	if (isr == 0 || isr == 0xFFFF) return tf;
	rtl_write16(state, RTL_REG_ISR, isr);

	if (isr & RTL_INT_ROK) {
		while (!(rtl_read8(state, RTL_REG_COMMAND) & RTL_CMD_RECV_EMPTY)) {
			uint32_t header_val;
			memcpy(&header_val, state->rx_buffer + state->rx_offset, sizeof(uint32_t));
			uint16_t packet_len	= (uint16_t)(header_val >> 16);

			if (packet_len > 1792 || packet_len < 4) break;

			uint8_t* packet_data = state->rx_buffer + state->rx_offset + 4;
			if (dev->on_received) dev->on_received(dev, packet_data, packet_len);

			state->rx_offset = (state->rx_offset + packet_len + 4 + 3) & ~3;
			if (state->rx_offset >= 8192) state->rx_offset -= 8192;

			int32_t capr_val = (int32_t)state->rx_offset - 0x10;
			rtl_write16(state, RTL_REG_CAPR, (uint16_t)(capr_val < 0 ? 8192 + capr_val : capr_val));
		}
	}
	return tf;
}

static void rtl_send_packet(kernel_net_dev_t* dev, void* data, size_t len) {
	rtl8139_state_t* state = (rtl8139_state_t*)dev->private_data;
	if (len > 1792) return;

	uint32_t slot_phys = (uint32_t)state->tx_buffer_phys + (state->tx_cur * 2048);
	uint8_t* slot_virt = (uint8_t*)((uintptr_t)slot_phys + hhdm_offset);

	// Wait for OWN bit
	while (!(rtl_read32(state, RTL_REG_TX_STATUS0 + (state->tx_cur * 4)) & (1 << 13))) {
		__asm__ volatile("pause");
	}

	memcpy(slot_virt, data, len);

	// ensure data is in memory before poking the card
	__asm__ volatile("" ::: "memory");

	rtl_write32(state, RTL_REG_TX_ADDR0 + (state->tx_cur * 4), slot_phys);
	rtl_write32(state, RTL_REG_TX_STATUS0 + (state->tx_cur * 4), (len & 0x1FFF));

	state->tx_cur = (state->tx_cur + 1) % 4;
}

void rtl8139_init(pci_dev_t pdev) {
	rtl8139_state_t* state = zalloc(sizeof(rtl8139_state_t));
	state->magic = 0x524C3831;

	// BAR0 = IO
	// BAR1 = MMIO

	state->is_mmio = !pdev.bars[0].is_io;
	state->base = state->is_mmio ? pdev.bars[1].base : pdev.bars[0].base;

	if (state->is_mmio) {
		uint64_t* pml4 = (uint64_t*)(vmm_get_pml4() + hhdm_offset);
		vmm_map_page(pml4, state->base + hhdm_offset, state->base,
					 PTE_PRESENT | PTE_WRITABLE | PTE_TYPE_DRIVER | PTE_CACHE_DISABLE | PTE_TYPE_DRIVER);
		state->base += hhdm_offset;
	}

	// reset controller
	rtl_write8(state, RTL_REG_COMMAND, RTL_CMD_RESET);
	while (rtl_read8(state, RTL_REG_COMMAND) & RTL_CMD_RESET);

	// RX buffer allocation
	state->rx_buffer_phys = pma_alloc_pages_low(3);
	state->rx_buffer = (uint8_t*)(state->rx_buffer_phys + hhdm_offset);
	rtl_write32(state, RTL_REG_RBSTART, (uint32_t)state->rx_buffer_phys);

	// TX buffer allocation
	state->tx_buffer_phys = pma_alloc_pages_low(2);

	rtl_write16(state, RTL_REG_IMR, RTL_INT_ROK | RTL_INT_TOK | RTL_INT_TER);
	rtl_write32(state, RTL_REG_RCR, 0x8F);
	rtl_write8(state, RTL_REG_COMMAND, RTL_CMD_TX_ENABLE | RTL_CMD_RX_ENABLE);

	kernel_net_dev_t* ndev = zalloc(sizeof(kernel_net_dev_t));
	for (int i = 0; i < 6; i++) ndev->mac[i] = rtl_read8(state, RTL_REG_MAC + i);

	strncpy(ndev->model, "RTL8139", sizeof(ndev->model));
	ndev->private_data = state;
	ndev->send_packet = rtl_send_packet;
	active_net_device = ndev;

	uint8_t vector = ioapic_map_irq_to_free_vector(pdev.irq, 0, 0);
	irq_install_handler(0, vector, rtl8139_irq_handler);

	logbuf_printf("[ ETH  ] Initialized %s with MAC: %M\n", ndev->model, ndev->mac);
}

#endif
