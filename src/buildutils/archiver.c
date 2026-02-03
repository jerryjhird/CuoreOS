/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

// this is for a custom format i wrote adapting cpio newc
// CPIR (copy in and read) archiver

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define ARCHIVE_MAGIC 0x43504952  // C P I R 
#define ARCHIVE_VERSION 2

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

static void pad8(uint64_t n) {
    uint8_t zero[8] = {0};
    uint64_t p = (8 - (n % 8)) % 8;
    if (p)
        fwrite(zero, 1, p, stdout);
}

static void write_header(const char *name, struct stat *st) {
    struct archive_hdr h = {
        .magic    = ARCHIVE_MAGIC,
        .version  = ARCHIVE_VERSION,
        .flags    = 0,

        .inode    = (uint64_t)st->st_ino,
        .mode     = (uint64_t)st->st_mode,
        .nlink    = (uint64_t)st->st_nlink,
        .mtime    = (uint64_t)st->st_mtime,
        .filesize = (uint64_t)st->st_size,
        .namesize = (uint64_t)strlen(name) + 1
    };

    fwrite(&h, sizeof(h), 1, stdout);
    fwrite(name, 1, h.namesize, stdout);

    pad8(sizeof(h) + h.namesize);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <dir> <out-file>\n", argv[0]);
        return 1;
    }

    const char *input_dir = argv[1];
    const char *out_file = argv[2];

    FILE *out = fopen(out_file, "wb");
    if (!out) {
        perror("fopen output file");
        return 1;
    }

    DIR *d = opendir(input_dir);
    if (!d) {
        perror("opendir");
        fclose(out);
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", input_dir, ent->d_name);

        struct stat st;
        if (stat(path, &st)) {
            perror("stat");
            fclose(out);
            closedir(d);
            return 1;
        }

        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "warn: skipping '%s'\n", ent->d_name);
            continue;
        }

        FILE *f = fopen(path, "rb");
        if (!f) {
            perror("fopen input file");
            fclose(out);
            closedir(d);
            return 1;
        }

        // write header
        struct archive_hdr h = {
            .magic    = ARCHIVE_MAGIC,
            .version  = ARCHIVE_VERSION,
            .flags    = 0,
            .inode    = (uint64_t)st.st_ino,
            .mode     = (uint64_t)st.st_mode,
            .nlink    = (uint64_t)st.st_nlink,
            .mtime    = (uint64_t)st.st_mtime,
            .filesize = (uint64_t)st.st_size,
            .namesize = (uint64_t)strlen(ent->d_name) + 1
        };

        fwrite(&h, sizeof(h), 1, out);
        fwrite(ent->d_name, 1, h.namesize, out);

        uint64_t pad = (8 - (sizeof(h) + h.namesize) % 8) % 8;
        if (pad) fwrite((uint8_t[8]){0}, 1, pad, out);

        // write file content
        uint8_t buf[8192];
        size_t r;
        while ((r = fread(buf, 1, sizeof(buf), f)))
            fwrite(buf, 1, r, out);

        pad = (8 - (st.st_size % 8)) % 8;
        if (pad) fwrite((uint8_t[8]){0}, 1, pad, out);

        fclose(f);
    }

    closedir(d);

    // write trailer
    struct stat zero = {0};
    struct archive_hdr h = {
        .magic    = ARCHIVE_MAGIC,
        .version  = ARCHIVE_VERSION,
        .flags    = 0,
        .inode    = 0,
        .mode     = 0,
        .nlink    = 0,
        .mtime    = 0,
        .filesize = 0,
        .namesize = strlen("TRAILER!!!") + 1
    };
    fwrite(&h, sizeof(h), 1, out);
    fwrite("TRAILER!!!", 1, h.namesize, out);
    uint64_t pad;
    pad = (8 - (sizeof(h) + h.namesize) % 8) % 8;
    if (pad) fwrite((uint8_t[8]){0}, 1, pad, out);

    fclose(out);

    return 0;
}