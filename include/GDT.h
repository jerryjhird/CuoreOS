/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef GDT_H
#define GDT_H

#ifdef __cplusplus
extern "C" {
#endif

#define IDT_ENTRIES 256
#define NUM_GPRS 15

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;       // bits 0-2 = IST, rest zero
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void gdt_init(void);
void idt_init(void);

#ifdef __cplusplus
}
#endif

#endif // GDT_H