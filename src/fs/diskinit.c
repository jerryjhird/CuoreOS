#include "diskinit.h"
#include "mbr.h"

void generic_disk_init(kernel_dev_t* dev) {
    if (!dev || !dev->read_sector) return;

    uint16_t buffer[256];
    dev->read_sector(0, buffer);

    mbr_parse(dev, (uint8_t*)buffer);
}