#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include "pci/pci.h"

void rtl8139_init(pci_dev_t dev);
void rtl8139_send_packet(void *data, uint32_t len);

#endif