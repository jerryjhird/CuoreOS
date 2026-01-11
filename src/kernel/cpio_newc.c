/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "stdint.h"
#include "memory.h" // for malloc/free
#include "cpio_newc.h"
#include "string.h"

// convert hex string to uint
static unsigned int hex_to_uint(const char *s, int len)
{
    unsigned int v = 0u;
    for (int i = 0; i < len; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9')
            v |= (unsigned int)(c - '0');
        else if (c >= 'A' && c <= 'F')
            v |= (unsigned int)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f')
            v |= (unsigned int)(c - 'a' + 10);
    }
    return v;
}

static uintptr_t align4(uintptr_t x) {
    return (x + 3u) & ~(uintptr_t)3u;
}

void *cpio_read_file(void *archive, const char *filename, size_t *out_size) {
    uint8_t *p = archive;

    while (1) {
        struct cpio_newc_header *hdr = (struct cpio_newc_header *)p;

        if (hdr->c_magic[0] != '0' || hdr->c_magic[1] != '7')
            return NULL;

        unsigned int namesize = hex_to_uint(hdr->c_namesize, 8);
        unsigned int fsize    = hex_to_uint(hdr->c_filesize, 8);
        char *name = (char *)(p + sizeof(*hdr));

        if (namesize == 11 && !strncmp(name, "TRAILER", 7))
            return NULL;

        if (namesize - 1 == strlen(filename) && !strncmp(name, filename, namesize - 1)) {
            uint8_t *data = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
            if (out_size) *out_size = fsize;
            return data;
        }

        p = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
        p = (uint8_t *)align4((uintptr_t)(p + fsize));
    }
}

// list all files
char *cpio_list_files(void *archive) {
    uint8_t *p = (uint8_t *)archive;
    size_t total_len = 0;

    // compute required size
    while (1) {
        struct cpio_newc_header *hdr = (struct cpio_newc_header *)p;
        if (hdr->c_magic[0] != '0' || hdr->c_magic[1] != '7') break;

        unsigned int namesize = hex_to_uint(hdr->c_namesize, 8);
        char *name = (char *)(p + sizeof(*hdr));

        if (namesize == 11 && !strncmp(name, "TRAILER", 7)) break;

        total_len += (namesize - 1) + 1; // name + newline

        p = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
        unsigned int fsize = hex_to_uint(hdr->c_filesize, 8);
        p = (uint8_t *)align4((uintptr_t)(p + fsize));
    }

    if (total_len == 0) return NULL;

    // allocate heap memory
    char *list = malloc(total_len + 1); // +1 for final null terminator
    if (!list) return NULL;

    char *ptr = list;
    p = (uint8_t *)archive;

    // copy names
    while (1) {
        struct cpio_newc_header *hdr = (struct cpio_newc_header *)p;
        if (hdr->c_magic[0] != '0' || hdr->c_magic[1] != '7') break;

        unsigned int namesize = hex_to_uint(hdr->c_namesize, 8);
        char *name = (char *)(p + sizeof(*hdr));

        if (namesize == 11 && !strncmp(name, "TRAILER", 7)) break;

        memcpy(ptr, name, namesize - 1);
        ptr += (namesize - 1);
        *ptr++ = '\n'; // separator

        p = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
        unsigned int fsize = hex_to_uint(hdr->c_filesize, 8);
        p = (uint8_t *)align4((uintptr_t)(p + fsize));
    }

    *ptr = '\0'; // null terminate
    return list;
}