/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#ifndef CPIO_NEWC_H
#define CPIO_NEWC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"

struct cpio_newc_header {
    char c_magic[6];
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
};

void *cpio_read_file(void *archive, const char *filename, size_t *out_size);
char *cpio_list_files(void *archive);

#ifdef __cplusplus
}
#endif

#endif // CPIO_NEWC_H