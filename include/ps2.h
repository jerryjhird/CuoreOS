/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef PS2_H
#define PS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "x86.h"
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