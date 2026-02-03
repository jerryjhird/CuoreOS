/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef KERNEL_H
#define KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"

void kpanic(const char *msg);

// kernel tests
void memory_test(void);

#ifdef __cplusplus
}
#endif

#endif // KERNEL_H