#include "CMOS.h"

#include "cpu/io.h"

void cmos_write(uint8_t reg, uint8_t value) {
	outb(CMOS_ADDR, reg);
	outb(CMOS_DATA, value);
}

uint8_t cmos_read(uint8_t reg) {
	outb(CMOS_ADDR, reg);
	return inb(CMOS_DATA);
}
