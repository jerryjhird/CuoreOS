#include "diskinit.h"
#include "mbr.h"
#include "gpt.h"
#include "logbuf.h"

void generic_disk_init(kernel_dev_t* dev) {
    if (!dev || !dev->read_sector) return;

    uint16_t buffer[256];
    dev->read_sector(0, buffer);

    uint8_t return_code = mbr_parse(dev, (uint8_t*)buffer);
    if (return_code == 1) {
        logbuf_write("[ MBR  ] Invalid MBR signature. Skipping disk\n");
    } else if (return_code == 2) {
        logbuf_write("[ MBR  ] GPT Protective MBR detected Initializing GPT...\n");
        gpt_parse(dev);
    }
}