#ifndef PS2_H
#define PS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "arch/x86.h"
#include "stdbool.h"

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

extern const char scta_uk[128];
extern const char scta_uk_shift[128];

// funcs
char ps2_getc(void);
bool ps2_dev_exists(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif // PS2_H