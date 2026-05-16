typedef int dummy0;

// driver for E1000 eth. supports MMIO
#include "Config.h"

#ifdef KERNEL_MOD_E1000_ENABLED
#include "e1000.h"
#include "mem/paging.h"
#include "mem/heap.h"
#include "mem/mem.h"
#include "logbuf.h"

static const char* e1000_get_model(uint16_t device_id) {
	switch (device_id) {
		case 0x100E: return "Intel 82540EM Ethernet";
		case 0x100F: return "Intel 82545EM Ethernet";
		case 0x10EA: return "Intel 82577LM Ethernet";
		case 0x10D3: return "Intel 82574L Ethernet";
		case 0x153A: return "Intel I217-LM Ethernet";
		case 0x155A: return "Intel I218-LM Ethernet";
		default: return "Intel E1000 Generic Ethernet";
	}
}

static inline void e1000_write(e1000_state_t* s, uint16_t reg, uint32_t val) {
	*(volatile uint32_t*)(s->mmio_virt + reg) = val;
}

static inline uint32_t e1000_read(e1000_state_t* s, uint16_t reg) {
	return *(volatile uint32_t*)(s->mmio_virt + reg);
}

static uint16_t e1000_eeprom_read(e1000_state_t* s, uint8_t addr) {
	uint32_t tmp = 0;
	e1000_write(s, E1000_REG_EERD, E1000_EERD_START | ((uint32_t)addr << 8));
	while (!((tmp = e1000_read(s, E1000_REG_EERD)) & E1000_EERD_DONE)) {
		__asm__ volatile("pause");
	}
	return (uint16_t)(tmp >> 16);
}

void e1000_init(pci_dev_t dev) {
	e1000_state_t* s = zalloc(sizeof(e1000_state_t));
	uint64_t* pml4 = (uint64_t*)(vmm_get_pml4() + hhdm_offset);

	for (uintptr_t offset = 0; offset < dev.bars[0].size; offset += 4096) {
		vmm_map_page(pml4, dev.bars[0].base + hhdm_offset + offset,
					 dev.bars[0].base + offset,
					 PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE | PTE_TYPE_DRIVER);
	}
	s->mmio_virt = dev.bars[0].base + hhdm_offset;

	for(int i = 0; i < 3; i++) {
		uint16_t val = e1000_eeprom_read(s, i);
		s->mac[i*2] = val & 0xFF;
		s->mac[i*2+1] = val >> 8;
	}

	const char* model_name = e1000_get_model(dev.device_id);
	logbuf_printf("[ ETH  ] Found %s with MAC: %M\n", model_name, s->mac);
}
#endif
