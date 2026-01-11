/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef PANIC_H
#define PANIC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef nl_serial_write
void serial_write(const char *msg, size_t len);
#define nl_serial_write(str) serial_write(str, sizeof(str)-1)
#endif

void panic(void);

#ifdef __cplusplus
}
#endif

#endif // PANIC_H