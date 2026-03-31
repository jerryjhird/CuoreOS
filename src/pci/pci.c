#include "pci.h"
#include "../cpu/io.h"
#include "logbuf.h"
#include <stddef.h>

uint32_t pci_read_word(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset) {
    uint32_t address = (uint32_t)((((uint32_t)bus) << 16) | 
                       (((uint32_t)slot) << 11) |
                       (((uint32_t)func) << 8) | 
                       (offset & 0xfc) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_word(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset, uint32_t data) {
    uint32_t address = (uint32_t)((((uint32_t)bus) << 16) | 
                       (((uint32_t)slot) << 11) |
                       (((uint32_t)func) << 8) | 
                       (offset & 0xfc) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, data);
}

extern pci_driver_entry_t pci_discovery_table[];

pci_bar_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index) {
    pci_bar_t bar = {0};
    uint32_t offset = 0x10 + (bar_index * 4);

    uint32_t low = pci_read_word(bus, slot, func, offset);
    if (low == 0) return bar;

    bar.is_io = (low & 0x1);

    pci_write_word(bus, slot, func, offset, 0xFFFFFFFF);
    uint32_t readback = pci_read_word(bus, slot, func, offset);
    pci_write_word(bus, slot, func, offset, low);

    if (bar.is_io) {
        bar.base = low & ~0x3;
        bar.size = ~(readback & ~0x3) + 1;
    } else {
        bar.base = low & ~0xF;
        bar.size = ~(readback & ~0xF) + 1;

        if ((low & 0x6) == 0x4) {
            uint64_t high = pci_read_word(bus, slot, func, offset + 4);
            bar.base |= (high << 32);
        }
    }

    return bar;
}

void pci_probe_device(pci_dev_t dev) {
    for (int i = 0; pci_discovery_table[i].name != NULL; i++) {
        pci_driver_entry_t* entry = &pci_discovery_table[i];
        bool match = false;

        if (entry->vendor_id != PCI_VENDOR_ANY) {
            if (dev.vendor_id == entry->vendor_id && dev.device_id == entry->device_id) {
                match = true;
            }
        } 

        else if (dev.class_id == entry->class_id && dev.subclass_id == entry->subclass_id) {
            match = true;
        }

        if (match) {
            bool has_driver = (entry->init != NULL);

            logbuf_write("[ PCI  ] ");
            logbuf_write(has_driver ? "Initializing " : "Found ");
            logbuf_write(entry->name);
            
            if (has_driver) {
                logbuf_write(" Driver\n");

                uint32_t cmd = pci_read_word(dev.bus, dev.slot, dev.func, 0x04);
                pci_write_word(dev.bus, dev.slot, dev.func, 0x04, cmd | 0x07);

                entry->init(dev);
            } else {
                logbuf_write("\n");
            }

            return;
        }
    }
}

void pci_init() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint16_t slot = 0; slot < 32; slot++) {
            uint32_t id_reg = pci_read_word(bus, slot, 0, 0x00);
            if ((id_reg & 0xFFFF) == 0xFFFF) continue;

            uint32_t header = pci_read_word(bus, slot, 0, 0x0C);
            int max_func = (header & 0x00800000) ? 8 : 1;

            for (uint8_t func = 0; func < max_func; func++) {
                id_reg = pci_read_word(bus, slot, func, 0x00);
                if ((id_reg & 0xFFFF) == 0xFFFF) continue;

                pci_dev_t dev = {
                    .bus = (uint8_t)bus, .slot = (uint8_t)slot, .func = func,
                    .vendor_id = (uint16_t)(id_reg & 0xFFFF),
                    .device_id = (uint16_t)(id_reg >> 16)
                };

                uint32_t class_reg = pci_read_word(bus, slot, func, 0x08);
                dev.class_id = (class_reg >> 24) & 0xFF;
                dev.subclass_id = (class_reg >> 16) & 0xFF;

                dev.irq = pci_read_word(bus, slot, func, 0x3C) & 0xFF;

                for (int i = 0; i < 6; i++) {
                    dev.bars[i] = pci_get_bar(bus, slot, func, i);
                    uint32_t raw = pci_read_word(bus, slot, func, 0x10 + (i * 4));
                    if (!(raw & 0x1) && ((raw & 0x6) == 0x4)) i++; 
                }

                pci_probe_device(dev);
            }
        }
    }
}
