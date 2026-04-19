typedef int dummy0;

#ifdef KERNEL_MOD_AC97_ENABLED

#include "pci/pci.h"
#include "ac97.h"
#include "cpu/io.h"
#include "logbuf.h"
#include "mem/heap.h"
#include "mem/mem.h"
#include "apic/ioapic.h"
#include "cpu/IRQ.h"

typedef struct {uint32_t id; const char* name;} ac97_vendor_t;

static const ac97_vendor_t ac97_vendors[] = {
	{0x83847600, "SigmaTel STAC9700"},
	{0x83847609, "SigmaTel STAC9721/23"},
	{0x41445300, "Analog Devices AD1881"},
	{0x414C4300, "Realtek ALC100/200"},
	{0x56494100, "VIA Technologies"},
	{0, NULL}
};

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
	uint16_t reg = (val << 8) | val; // set left and right
	outw(state->nambar + AC97_MASTER_VOLUME, reg);
}

static void ac97_stop(kernel_audio_dev_t* dev) {
	ac97_state_t* state = (ac97_state_t*)dev->private_data;
	uint8_t ctrl = inb(state->nabmbar + AC97_PO_CR);
	outb(state->nabmbar + AC97_PO_CR, ctrl & ~0x01);
	dev->is_playing = false;
}

static void ac97_play(kernel_audio_dev_t* dev, void* buffer, size_t size) {
	ac97_state_t* state = (ac97_state_t*)dev->private_data;
	uintptr_t phys_buffer = (uintptr_t)buffer - hhdm_offset;

	outb(state->nabmbar + AC97_PO_CR, 0x02);
	int timeout = 1000;
	while ((inb(state->nabmbar + AC97_PO_CR) & 0x02) && --timeout > 0);

	state->bdl[0].addr = (uint32_t)phys_buffer;
	state->bdl[0].length = (uint16_t)(size / 2);
	state->bdl[0].flags = 0xC000; // IOC | BUP

	outl(state->nabmbar + AC97_PO_BDBAR, (uint32_t)((uintptr_t)state->bdl - hhdm_offset));
	outb(state->nabmbar + AC97_PO_LVI, 0);
	outw(state->nabmbar + AC97_PO_SR, 0x1C);

	dev->is_playing = true;

	outb(state->nabmbar + AC97_PO_CR, 0x1D);
}

static struct trap_frame* ac97_irq_handler(struct trap_frame* tf) {
	kernel_audio_dev_t* adev = active_audio_device;
	if (!adev) return tf;

	ac97_state_t* state = (ac97_state_t*)adev->private_data;
	uint16_t sr = inw(state->nabmbar + AC97_PO_SR);

	// 0x18 = IOC (0x08) | LVI (0x10)
	if (sr & 0x18) {
		outw(state->nabmbar + AC97_PO_SR, sr & 0x18);

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

void ac97_init(pci_dev_t dev) {
	uint32_t nambar  = (uint32_t)dev.bars[0].base;
	uint32_t nabmbar = (uint32_t)dev.bars[1].base;

	outl(nabmbar + AC97_GLOB_CNT, 0x00000002);

	int timeout = 1000;
	while (!(inl(nabmbar + AC97_GLOB_STA) & (1 << 8)) && --timeout > 0);

	// enable GIE
	uint32_t glob_cnt = inl(nabmbar + AC97_GLOB_CNT);
	outl(nabmbar + AC97_GLOB_CNT, glob_cnt | 0x01);

	outw(nambar + AC97_MIXER_RESET, 0x0001);
	outw(nambar + AC97_MASTER_VOLUME, 0x0F0F);
	outw(nambar + AC97_PCM_VOLUME,	0x0F0F);

	ac97_state_t* state = zalloc(sizeof(ac97_state_t));
	state->nambar = nambar;
	state->nabmbar = nabmbar;

	uint8_t vector = 42;
	if (dev.irq != 0xFF && dev.irq != 0) {
		ioapic_map_irq(dev.irq, vector, 0, 0);
		irq_install_handler(0, vector, ac97_irq_handler);
	}

	// allocate 32 entries for BDL
	state->bdl = (ac97_bdl_entry_t*)zalloc(sizeof(ac97_bdl_entry_t) * 32);
	outl(nabmbar + AC97_PO_BDBAR, (uint32_t)((uintptr_t)state->bdl - hhdm_offset)); // point hardware to the BDL physical addr

	kernel_audio_dev_t* adev = zalloc(sizeof(kernel_audio_dev_t));

	// get "model name"
	ac97_identify_vendor(adev, inw(nambar + AC97_VENDOR_ID1), inw(nambar + AC97_VENDOR_ID2));

	adev->is_playing = false;
	adev->sample_rate = 44100;
	adev->channels = 2;
	adev->bit_depth = 16;
	adev->private_data = (void*)(uintptr_t)nabmbar;
	adev->play = ac97_play;
	adev->stop = ac97_stop;
	adev->set_volume = ac97_set_volume;
	adev->private_data = state; // point to our state struct

	active_audio_device = adev;

	logbuf_write("[ AC97 ] Initialized ");
	logbuf_write(adev->model);
	logbuf_write(" at ");
	logbuf_puthex(nabmbar);
	logbuf_write("\n");
}

#endif
