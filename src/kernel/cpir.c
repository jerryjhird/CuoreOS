/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */

// this is for a custom format i wrote adapting cpio newc
// CPIR (copy in and read) reader

#include "stdint.h"
#include "memory.h" // for malloc/free
#include "string.h"

#define ARCHIVE_MAGIC   0x43504952  // C P I R

struct archive_hdr {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;

    uint64_t inode;
    uint64_t mode;
    uint64_t nlink;
    uint64_t mtime;
    uint64_t filesize;
    uint64_t namesize;
};

static uintptr_t align8(uintptr_t x) {
    return (x + 7u) & ~(uintptr_t)7u;
}

void *cpir_read_file(void *archive, const char *filename, size_t *out_size) {
    uint8_t *p = (uint8_t *)archive;

    while (1) {
        struct archive_hdr *hdr = (struct archive_hdr *)p;

        if (hdr->magic != ARCHIVE_MAGIC)
            return NULL;

        char *name = (char *)(p + sizeof(*hdr));

        if (!strcmp(name, "TRAILER!!!"))
            return NULL;

        uintptr_t data =
            align8((uintptr_t)(p + sizeof(*hdr) + hdr->namesize));

        if (!strcmp(name, filename)) {
            if (out_size)
                *out_size = (size_t)hdr->filesize;
            return (void *)data;
        }

        p = (uint8_t *)align8(data + hdr->filesize);
    }
}

char *cpir_list_files(void *archive) {
    uint8_t *p = (uint8_t *)archive;
    size_t total = 0;

    while (1) {
        struct archive_hdr *hdr = (struct archive_hdr *)p;
        if (hdr->magic != ARCHIVE_MAGIC)
            break;

        char *name = (char *)(p + sizeof(*hdr));
        if (!strcmp(name, "TRAILER!!!"))
            break;

        total += hdr->namesize;

        p = (uint8_t *)align8((uintptr_t)(p + sizeof(*hdr) + hdr->namesize));
        p = (uint8_t *)align8((uintptr_t)(p + hdr->filesize));
    }

    if (total == 0)
        return NULL;

    char *list = (char *)malloc(total + 1);
    if (!list)
        return NULL;

    char *out = list;
    p = (uint8_t *)archive;

    while (1) {
        struct archive_hdr *hdr = (struct archive_hdr *)p;
        if (hdr->magic != ARCHIVE_MAGIC)
            break;

        char *name = (char *)(p + sizeof(*hdr));
        if (!strcmp(name, "TRAILER!!!"))
            break;

        size_t len = hdr->namesize - 1;
        memcpy(out, name, len);
        out += len;
        *out++ = '\n';

        p = (uint8_t *)align8((uintptr_t)(p + sizeof(*hdr) + hdr->namesize));
        p = (uint8_t *)align8((uintptr_t)(p + hdr->filesize));
    }

    *out = '\0';
    return list;
}