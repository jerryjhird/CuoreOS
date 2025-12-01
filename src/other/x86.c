#include "stdint.h"
#include "time.h"
#include "x86.h"
#include "kernel/kio.h"

uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

// reads 64-bit timestamp counter
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint8_t bcd_to_bin(uint8_t val) {
    return (uint8_t)((val & 0x0F) + ((val >> 4) * 10));
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
    dt.year  = 2000 + bcd_to_bin(cmos_read(0x09)); // oh no y2k in a 1000 years

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

    int32_t days_signed = 365*year + year/4 - year/100 + year/400 + (153*month + 8)/5 + day - 719469;
    uint32_t days = (uint32_t)days_signed;
    return days*86400 + hour*3600 + min*60 + sec;
}

void sleep_ms(uint64_t ms, uint64_t cpu_hz) {
    uint64_t start = rdtsc();
    uint64_t cycles_to_wait = (cpu_hz / 1000) * ms;
    while ((rdtsc() - start) < cycles_to_wait) {
        __asm__ volatile("pause"); // prevents busy-wait from hammering the CPU
    }
}
