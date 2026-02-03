/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef CPIR_NEWC_H
#define CPIR_NEWC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"

void *cpir_read_file(void *archive, const char *filename, size_t *out_size);
char *cpir_list_files(void *archive);

#ifdef __cplusplus
}
#endif

#endif // CPIR_NEWC_H