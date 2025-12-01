#ifndef PS2_H
#define PS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "x86.h"

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

static const char scta_uk[128] = { // scancode to ascii (uk layout)
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b', '\t',  // 0x00-0x0F
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,              // 0x10-0x1E
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'#',              // 0x1E-0x2B
    'z','x','c','v','b','n','m',',','.','/', 0,                            // 0x2C-0x35
    '*', 0, ' ', 0                                                          // 0x36-0x39
};

char getc(void);
bool ps2_dev_exists(uint8_t port);
int ps2_dev_count(void);

#ifdef __cplusplus
}
#endif

#endif // PS2_H