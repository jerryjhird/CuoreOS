/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef SERIAL_H
#define SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "string.h"

void serial_init(void);

void serial_write(const char *msg, size_t len);
#define serial_print(str) serial_write(str, strlen(str))

#ifdef __cplusplus
}
#endif

#endif // SERIAL_H