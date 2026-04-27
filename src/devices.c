#include "devices.h"

kernel_char_dev_t *char_devices[MAX_CHAR_DEVICES];
size_t char_devices_c = 0;

kernel_cpu_dev_t *cpu_devices[SMP_MAX_CORES];
volatile uint32_t cpu_devices_c = 0;

kernel_disk_dev_t *disk_devices[MAX_DISK_DEVICES];
size_t disk_devices_c = 0;

kernel_power_dev_t *power_devices[MAX_POWER_DEVICES];
size_t power_devices_c = 0;

kernel_audio_dev_t* active_audio_device;

void dev_puts(kernel_char_dev_t* dev, const char* s) {
	while (*s) {
		dev->putc(*s++);
	}
}

void dev_putint(kernel_char_dev_t* dev, uint64_t n) {
	if (n == 0) {
		dev_puts(dev, "0");
		return;
	}

	char buf[21];
	int i = 20;
	buf[i--] = '\0';

	while (n > 0) {
		buf[i--] = (n % 10) + '0';
		n /= 10;
	}

	dev_puts(dev, &buf[i + 1]);
}
