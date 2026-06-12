#include "pcspeaker.h"

#include "pit.h"
#include "cpu/io.h"

void pcspeaker_init(void) {
	pcspeaker_beep(0);
}

void pcspeaker_beep(uint32_t frequency) {
	uint8_t tmp = inb(PORT_SYSTEM_CONTROL_B);

	if (frequency == 0) {
		outb(PORT_SYSTEM_CONTROL_B, tmp & 0xFC);
		return;
	}

	uint32_t divisor = 1193182 / frequency;

	outb(PIT_COMMAND, 0xB6);
	outb(PIT_CHANNEL_2, (uint8_t)(divisor & 0xFF));
	outb(PIT_CHANNEL_2, (uint8_t)((divisor >> 8) & 0xFF));
	outb(PORT_SYSTEM_CONTROL_B, tmp | 0x03);
}
