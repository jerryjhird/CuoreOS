#ifndef X86_H
#define X86_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

uint8_t cmos_read(uint8_t reg); // read byte from CMOS/RTC register

const char* get_cpu(void);
datetime_st get_datetime(void);
uint32_t datetime_to_epoch(datetime_st dt);

#ifdef __cplusplus
}
#endif

#endif // X86_H