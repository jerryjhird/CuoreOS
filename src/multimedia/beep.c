#include "devices.h"
#include "mem/heap.h"

void beep_cleanup_hook(void* arg) {
	free(arg);
}

void audio_beep(kernel_audio_dev_t* dev, uint32_t freq, uint32_t duration_ms) {
	if (!dev) return;
	while (dev->is_playing || dev->hook != NULL) {
		__builtin_ia32_pause();
	}

	uint32_t num_samples = (dev->sample_rate * duration_ms) / 1000;
	size_t buffer_size = num_samples * dev->channels * (dev->bit_depth / 8);

	int16_t* buffer = zalloc(buffer_size);
	if (!buffer) return;

	uint32_t period = dev->sample_rate / freq;
	uint32_t half_period = period / 2;

	for (uint32_t i = 0; i < num_samples; i++) {
		int16_t sample = ((i / half_period) % 2) ? 3000 : -3000;

		for (int c = 0; c < dev->channels; c++) {
			buffer[i * dev->channels + c] = sample;
		}
	}

	dev->hook = beep_cleanup_hook;
	dev->hook_args = buffer;
	dev->play(dev, buffer, buffer_size);
}
