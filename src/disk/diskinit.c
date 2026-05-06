#include "diskinit.h"
#include "devices.h"
#include "mbr.h"
#include "gpt.h"

void generic_disk_init(kernel_disk_dev_t* dev) {
	if (!dev || !dev->read_sectors) return;

	uint8_t return_code = mbr_parse(dev);
	if (return_code == 2) {
		gpt_parse(dev);
	}
}
