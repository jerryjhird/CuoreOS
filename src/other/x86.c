#include "stdint.h"
#include "stdio.h"
#include "time.h"
#include "x86.h"
#include "kernel/kio.h"
#include "string.h"

uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

// reads 64 bit timestamp counter
uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

uint8_t bcdtbin(uint8_t val) { // bcd to binary
    return (uint8_t)((val & 0x0F) + ((val >> 4) * 10));
}

void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx),
                       "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}

datetime_st getdatetime(void) {
    datetime_st dt;

    while (cmos_read(0x0A) & 0x80); // wait until update-in-progress clear

    dt.sec   = bcdtbin(cmos_read(0x00));
    dt.min   = bcdtbin(cmos_read(0x02));
    dt.hour  = bcdtbin(cmos_read(0x04));
    dt.day   = bcdtbin(cmos_read(0x07));
    dt.month = bcdtbin(cmos_read(0x08));
    dt.year  = 2000 + bcdtbin(cmos_read(0x09));

    return dt;
}

uint32_t dttepoch(datetime_st dt) { // time.h 
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

void sleep_ms(uint64_t ms, uint64_t cpu_hz) { // time.h
    uint64_t start = rdtsc();
    uint64_t cycles_to_wait = (cpu_hz / 1000) * ms;
    while ((rdtsc() - start) < cycles_to_wait) {
        __asm__ volatile("pause"); // prevents busy-wait from hammering the CPU
    }
}

void cpu_brand(struct writeout_t *wo) {
    char brand[49];
    uint32_t eax, ebx, ecx, edx;
    char *p = brand;

    for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
        cpuid(leaf, 0, &eax, &ebx, &ecx, &edx);

        /* copy 4 registers (16 bytes) per leaf into the brand buffer */
        memcpy(p, &eax, 4); p += 4;
        memcpy(p, &ebx, 4); p += 4;
        memcpy(p, &ecx, 4); p += 4;
        memcpy(p, &edx, 4); p += 4;
    }

    brand[48] = '\0';

    /* trim trailing spaces and newlines (optional, but nice) */
    int end = 47;
    while (end >= 0 && (brand[end] == ' ' || brand[end] == '\0' || brand[end] == '\n' || brand[end] == '\r'))
        brand[end--] = '\0';

    bwrite(wo, brand);
}