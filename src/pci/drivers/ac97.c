typedef int dummy0;

// driver for AC'97 sound cards. supports PMIO and MMIO
#include "Config.h"

#ifdef KERNEL_MOD_AC97_ENABLED

#include "pci/pci.h"
#include "ac97.h"
#include "cpu/io.h"
#include "logbuf.h"
#include "mem/heap.h"
#include "mem/mem.h"
#include "apic/ioapic.h"
#include "cpu/IRQ.h"
#include "mem/paging.h"

typedef struct {uint32_t id; const char* name;} ac97_vendor_t;

static const ac97_vendor_t ac97_vendors[] = {
	{0x83847600, "SigmaTel STAC9700"},
	{0x83847609, "SigmaTel STAC9721/23"},
	{0x41445300, "Analog Devices AD1881"},
	{0x414C4300, "Realtek ALC100/200"},
	{0x56494100, "VIA Technologies"},
	{0, NULL}
};

static void ac97_write32(ac97_state_t* s, uintptr_t base, uint16_t reg, uint32_t val) {
	if (s->is_mmio) *(volatile uint32_t*)(base + reg) = val;
	else outl((uint16_t)base + reg, val);
}

static void ac97_write16(ac97_state_t* s, uintptr_t base, uint16_t reg, uint16_t val) {
	if (s->is_mmio) *(volatile uint16_t*)(base + reg) = val;
	else outw((uint16_t)base + reg, val);
}

static void ac97_write8(ac97_state_t* s, uintptr_t base, uint16_t reg, uint8_t val) {
	if (s->is_mmio) *(volatile uint8_t*)(base + reg) = val;
	else outb((uint16_t)base + reg, val);
}

static uint32_t ac97_read32(ac97_state_t* s, uintptr_t base, uint16_t reg) {
	if (s->is_mmio) return *(volatile uint32_t*)(base + reg);
	return inl((uint16_t)base + reg);
}

static uint16_t ac97_read16(ac97_state_t* s, uintptr_t base, uint16_t reg) {
	if (s->is_mmio) return *(volatile uint16_t*)(base + reg);
	return inw((uint16_t)base + reg);
}

static uint8_t ac97_read8(ac97_state_t* s, uintptr_t base, uint16_t reg) {
	if (s->is_mmio) return *(volatile uint8_t*)(base + reg);
	return inb((uint16_t)base + reg);
}

static void ac97_identify_vendor(kernel_audio_dev_t* adev, uint16_t vid1, uint16_t vid2) {
	uint32_t full_id = (vid1 << 16) | vid2;
	uint32_t masked_id = full_id & 0xFFFFFF00;

	for (int i = 0; ac97_vendors[i].name != NULL; i++) {
		if (ac97_vendors[i].id == full_id || ac97_vendors[i].id == masked_id) {
			size_t len = strlen(ac97_vendors[i].name);
			memcpy(adev->model, ac97_vendors[i].name, len < 31 ? len : 31);
			return;
		}
	}

	memcpy(adev->model, "Unknown", 8);
}

static void ac97_set_volume(kernel_audio_dev_t* dev, uint8_t volume) {
	ac97_state_t* state = (ac97_state_t*)dev->private_data;
	uint8_t val = 31 - ((31 * volume) / 100);
	uint16_t reg = (val << 8) | val;
	ac97_write16(state, state->nambar, AC97_MASTER_VOLUME, reg);
}

static void ac97_stop(kernel_audio_dev_t* dev) {
	ac97_state_t* state = (ac97_state_t*)dev->private_data;
	uint8_t ctrl = ac97_read8(state, state->nabmbar, AC97_PO_CR);
	ac97_write8(state, state->nabmbar, AC97_PO_CR, ctrl & ~0x01);
	dev->is_playing = false;
}

static void ac97_play(kernel_audio_dev_t* dev, void* buffer, size_t size) {
	ac97_state_t* state = (ac97_state_t*)dev->private_data;

	uintptr_t cr3;
	__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
	uint64_t* pml4 = (uint64_t*)((cr3 & ~0xFFFULL) + hhdm_offset);

	uintptr_t phys_buffer = vmm_get_phys(pml4, (uintptr_t)buffer);
	uintptr_t bdl_phys = vmm_get_phys(pml4, (uintptr_t)state->bdl);

	if (!phys_buffer || !bdl_phys) return;

	ac97_write8(state, state->nabmbar, AC97_PO_CR, 0x02);
	int timeout = 1000;
	while ((ac97_read8(state, state->nabmbar, AC97_PO_CR) & 0x02) && --timeout > 0);

	state->bdl[0].addr = (uint32_t)phys_buffer;
	state->bdl[0].length = (uint16_t)(size / 2);
	state->bdl[0].flags = 0xC000;

	ac97_write32(state, state->nabmbar, AC97_PO_BDBAR, (uint32_t)bdl_phys);
	ac97_write8(state, state->nabmbar, AC97_PO_LVI, 0);
	ac97_write16(state, state->nabmbar, AC97_PO_SR, 0x1C);

	dev->is_playing = true;
	ac97_write8(state, state->nabmbar, AC97_PO_CR, 0x1D);
}

static struct trap_frame* ac97_irq_handler(struct trap_frame* tf) {
	kernel_audio_dev_t* adev = active_audio_device;
	if (!adev) return tf;

	ac97_state_t* state = (ac97_state_t*)adev->private_data;
	uint32_t glob_sta = ac97_read32(state, state->nabmbar, AC97_GLOB_STA);

	if (!(glob_sta & (1 << 8))) {
		return tf;
	}

	uint16_t sr = ac97_read16(state, state->nabmbar, AC97_PO_SR);

	// 0x18 = IOC (0x08) | LVI (0x10)
	if (sr & 0x18) {
		ac97_write16(state, state->nabmbar, AC97_PO_SR, sr & 0x1C);

		adev->is_playing = false;

		if (adev->hook) {
			void (*current_hook)(void*) = adev->hook;
			void* current_args = adev->hook_args;

			adev->hook = NULL;
			adev->hook_args = NULL;

			current_hook(current_args);
		}
	}

	return tf;
}

pci_driver_status ac97_init(pci_dev_t dev) {
	ac97_state_t* state = zalloc(sizeof(ac97_state_t));

	state->nambar  = dev.bars[0].base;
	state->nabmbar = dev.bars[1].base;
	state->is_mmio = !dev.bars[0].is_io;

	// map na(b)mbar apply offsets if MMIO
	if (state->is_mmio) {
		uintptr_t pml4_phys = vmm_get_pml4();
		uint64_t* pml4_virt = (uint64_t*)(pml4_phys + hhdm_offset);

		// NAMBAR
		vmm_map_page(pml4_virt, state->nambar + hhdm_offset, state->nambar,
					 PTE_PRESENT | PTE_WRITABLE | PTE_TYPE_DRIVER | PTE_CACHE_DISABLE);

		// NABMBAR
		vmm_map_page(pml4_virt, state->nabmbar + hhdm_offset, state->nabmbar,
					 PTE_PRESENT | PTE_WRITABLE | PTE_TYPE_DRIVER | PTE_CACHE_DISABLE);

		state->nambar  += hhdm_offset;
		state->nabmbar += hhdm_offset;
	}

	ac97_write32(state, state->nabmbar, AC97_GLOB_CNT, 0x00000002);

	int timeout = 1000;
	while (!(ac97_read32(state, state->nabmbar, AC97_GLOB_STA) & (1 << 8)) && --timeout > 0);

	// enable GIE
	uint32_t glob_cnt = ac97_read32(state, state->nabmbar, AC97_GLOB_CNT);
	ac97_write32(state, state->nabmbar, AC97_GLOB_CNT, glob_cnt | 0x01);

	ac97_write16(state, state->nambar, AC97_MIXER_RESET, 0x0001);
	ac97_write16(state, state->nambar, AC97_MASTER_VOLUME, 0x0F0F);
	ac97_write16(state, state->nambar, AC97_PCM_VOLUME,	0x0F0F);

	if (dev.irq != 0xFF && dev.irq != 0) {
		uint8_t vector = ioapic_map_irq_to_free_vector(dev.irq, 0, 0);
		irq_install_handler(0, vector, ac97_irq_handler);
	}

	// allocate 32 entries for BDL
	state->bdl = (ac97_bdl_entry_t*)zalloc(sizeof(ac97_bdl_entry_t) * 32);

	uint64_t* pml4 = (uint64_t*)(vmm_get_pml4() + hhdm_offset);
	uintptr_t bdl_phys = vmm_get_phys(pml4, (uintptr_t)state->bdl);
	ac97_write32(state, state->nabmbar, AC97_PO_BDBAR, (uint32_t)bdl_phys);

	kernel_audio_dev_t* adev = zalloc(sizeof(kernel_audio_dev_t));

	uint16_t vid1 = ac97_read16(state, state->nambar, AC97_VENDOR_ID1);
	uint16_t vid2 = ac97_read16(state, state->nambar, AC97_VENDOR_ID2);

	// get "model name"
	ac97_identify_vendor(adev, vid1, vid2);

	adev->is_playing = false;
	adev->sample_rate = 44100;
	adev->channels = 2;
	adev->bit_depth = 16;
	adev->private_data = state;
	adev->play = ac97_play;
	adev->stop = ac97_stop;
	adev->set_volume = ac97_set_volume;
	active_audio_device = adev;

	logbuf_write("[ AC97 ] Initialized ");
	logbuf_write(adev->model);
	logbuf_write("\n");

	return DRIVER_OK;
}

#endif
