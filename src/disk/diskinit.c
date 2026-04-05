#include "diskinit.h"
#include "devices.h"
#include "mbr.h"
#include "gpt.h"
#include "logbuf.h"
#include "kstate.h"

void generic_disk_init(kernel_disk_dev_t* dev) {
	if (!dev || !dev->read_sector) return;
	supported_disk_exists = true;

	uint8_t return_code = mbr_parse(dev);
	if (return_code == 2) {
		logbuf_write("[ GPT  ] Initializing GPT\n");
		gpt_parse(dev);
	}
}
