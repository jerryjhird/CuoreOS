/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef GDT_H
#define GDT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

void gdt_init(void);
void idt_init(void);

#ifdef __cplusplus
}
#endif

#endif // GDT_H