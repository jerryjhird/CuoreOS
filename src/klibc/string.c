/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "string.h"
#include "stdio.h"

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (uint8_t)*s1 - (uint8_t)*s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c1 = (unsigned char)s1[i];
        unsigned char c2 = (unsigned char)s2[i];
        if (c1 != c2) return c1 - c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

void u32dec(char *buf, uint32_t val) {
    char temp[11]; // max 10 digits + null
    int i = 0;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = 0;
        return;
    }
    while (val > 0) {
        temp[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int j = 0; j < i; j++) {
        buf[j] = temp[i - 1 - j];
    }
    buf[i] = 0;
}

uint32_t crc32c_swhash(const char *s)
{
    const uint32_t poly = 0x82F63B78;
    uint32_t crc = 0;

    while (*s) {
        uint32_t c = (uint8_t)*s++;
        crc ^= c;

        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (poly & -(crc & 1));
        }
    }

    return crc;
}