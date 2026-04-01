#include "diskinit.h"
#include "mbr.h"
#include "gpt.h"
#include "logbuf.h"
#include "kstate.h"

void generic_disk_init(kernel_dev_t* dev) {
	if (!dev || !dev->read_sector) return;

	active_disk_device = *dev;

	uint8_t return_code = mbr_parse(dev);
	if (return_code == 2) {
		logbuf_write("[ MBR  ] GPT Protective MBR detected Initializing GPT...\n");
		gpt_parse(dev);
	}
}
