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

#define SERIAL_COM1 0x3F8

void serial_init(void);

#ifndef nl_serial_write // defined in panic.h because used to write panic messages
void serial_write(const char *msg, size_t len);
#define nl_serial_write(str) serial_write(str, sizeof(str)-1)
#endif

#ifdef __cplusplus
}
#endif

#endif // SERIAL_H