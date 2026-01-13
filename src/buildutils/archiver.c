/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

static void pad4(size_t n) {
    static const char zero[4] = {0};
    size_t p = (4 - (n % 4)) % 4;
    if(p) fwrite(zero, 1, p, stdout);
}

static void write_hex(uint32_t v) {
    fprintf(stdout, "%08X", v);
}

static void write_header(
    const char *name,
    struct stat *st
) {
    fwrite("070701", 1, 6, stdout); // c_magic
    write_hex(1); // c_ino
    write_hex(st->st_mode); // c_mode
    write_hex(st->st_uid); // c_uid
    write_hex(st->st_gid); // c_gid
    write_hex(1); // c_nlink
    write_hex(st->st_mtime); // c_mtime
    write_hex(st->st_size); // c_filesize
    write_hex(0); write_hex(0); // devmajor/minor
    write_hex(0); write_hex(0); // rdevmajor/minor
    write_hex(strlen(name) + 1); // c_namesize
    write_hex(0); // c_check

    fwrite(name, 1, strlen(name) + 1, stdout);
    pad4(110 + strlen(name) + 1);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <dir>\n", argv[0]);
        return 1;
    }

    DIR *d = opendir(argv[1]);
    if (!d) {
        perror("opendir");
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", argv[1], ent->d_name);

        struct stat st;
        if (stat(path, &st)) {
            perror("stat");
            return 1;
        }

        if (S_ISDIR(st.st_mode)) {
            fprintf(stderr, "warn: subdirectory '%s' skipped\n", ent->d_name);
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "warn: unsupported file '%s' skipped\n", ent->d_name);
            continue;
        }

        FILE *f = fopen(path, "rb");
        if (!f) {
            perror("fopen");
            return 1;
        }

        write_header(ent->d_name, &st);

        char buf[4096];
        size_t r;
        while ((r = fread(buf, 1, sizeof(buf), f)))
            fwrite(buf, 1, r, stdout);

        pad4(st.st_size);
        fclose(f);
    }

    closedir(d);

    /* trailer */
    struct stat st = {0};
    write_header("TRAILER!!!", &st);

    return 0;
}
