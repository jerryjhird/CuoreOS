#include "rtl8139.h"
#include "cpu/IRQ.h"

void rtl8139_handler(struct trap_frame* frame) {
    (void)frame;
}

void rtl8139_init(pci_dev_t dev) {
    // uintptr_t io_base = dev.bars[0].base;
    // stub (work in progress)
}
