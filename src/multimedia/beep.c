#include "beep.h"

#include "devices.h"
#include "mem/mem.h"
#include "mem/pma.h"
#include "mem/heap.h"
#include "abs.h"
#include <stdint.h>

typedef struct {
	uintptr_t phys_addr;
	size_t pages;
} beep_metadata_t;

static void beep_cleanup_hook(void* arg) {
	beep_metadata_t* meta = (beep_metadata_t*)arg;
	pma_free_pages(meta->phys_addr, meta->pages);
	free(meta);
}

void audio_beep(kernel_audio_dev_t* dev, uint32_t freq, uint32_t duration_ms) {
	if (UNLIKELY(!dev || freq == 0 || duration_ms == 0)) return;
	while (dev->is_playing || dev->hook != NULL) {
		__builtin_ia32_pause();
	}

	uint32_t num_samples = (dev->sample_rate * duration_ms) / 1000;
	size_t buffer_size = num_samples * dev->channels * (dev->bit_depth / 8);
	size_t pages_needed = (buffer_size + 4095) / 4096;

	uintptr_t phys_addr = pma_alloc_pages_low(pages_needed);
	int16_t* buffer = (int16_t*)(phys_addr + hhdm_offset);
	memset(buffer, 0, pages_needed * 4096);

	beep_metadata_t* meta = malloc(sizeof(beep_metadata_t));
	meta->phys_addr = phys_addr;
	meta->pages = pages_needed;

	uint32_t period = dev->sample_rate / freq;
	uint32_t half_period = period / 2;

	for (uint32_t i = 0; i < num_samples; i++) {
		int16_t sample = ((i / half_period) % 2) ? 3000 : -3000;
		for (int c = 0; c < dev->channels; c++) {
			buffer[i * dev->channels + c] = sample;
		}
	}

	dev->hook = beep_cleanup_hook;
	dev->hook_args = (void*)meta;
	dev->play(dev, buffer, buffer_size);
}
