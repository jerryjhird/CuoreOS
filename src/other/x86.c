#include "stdint.h"
#include "types.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return (val & 0x0F) + ((val >> 4) * 10);
}

void get_cpu(char *buf) {
    char name[49];
    uint32_t eax, ebx, ecx, edx;
    char *ptr = name;

    for (uint32_t i = 0x80000002; i <= 0x80000004; i++) {
        __asm__ volatile("cpuid"
                         : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(i), "c"(0));

        *(uint32_t*)ptr = eax; ptr += 4;
        *(uint32_t*)ptr = ebx; ptr += 4;
        *(uint32_t*)ptr = ecx; ptr += 4;
        *(uint32_t*)ptr = edx; ptr += 4;
    }

    *ptr = '\0';

    // copy to caller-provided buffer
    for (int i = 0; i <= 48; i++)
        buf[i] = name[i];
}

datetime_st get_datetime(void) {
    datetime_st dt;

    while (cmos_read(0x0A) & 0x80); // wait until update-in-progress clear

    dt.sec   = bcd_to_bin(cmos_read(0x00));
    dt.min   = bcd_to_bin(cmos_read(0x02));
    dt.hour  = bcd_to_bin(cmos_read(0x04));
    dt.day   = bcd_to_bin(cmos_read(0x07));
    dt.month = bcd_to_bin(cmos_read(0x08));
    dt.year  = 2000 + bcd_to_bin(cmos_read(0x09));

    return dt;
}

uint32_t datetime_to_epoch(datetime_st dt) {
    uint16_t year = dt.year;
    uint8_t month = dt.month;
    uint8_t day = dt.day;
    uint8_t hour = dt.hour;
    uint8_t min = dt.min;
    uint8_t sec = dt.sec;

    if (month <= 2) {
        month += 12;
        year -= 1;
    }

    uint32_t days = 365*year + year/4 - year/100 + year/400 + (153*month + 8)/5 + day - 719469;
    return days*86400 + hour*3600 + min*60 + sec;
}
