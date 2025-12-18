#include "stdio.h"
#include "stdint.h"
#include "fs/cpio_newc.h"

static unsigned int hex_to_uint(const char *s, int len) {
    unsigned int v = 0;
    for (int i = 0; i < len; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
    }
    return v;
}

static inline uintptr_t align4(uintptr_t x) {
    return (x + 3) & ~3;
}

int cpio_read_file(struct writeout_t *wo, void *archive, const char *filename) {
    uint8_t *p = (uint8_t *)archive;

    while (1) {
        struct cpio_newc_header *hdr = (struct cpio_newc_header *)p;

        if (hdr->c_magic[0] != '0' || hdr->c_magic[1] != '7') {
            bwrite(wo, "invalid CPIO archive\n");
            return -1;
        }

        unsigned int namesize = hex_to_uint(hdr->c_namesize, 8);
        unsigned int fsize = hex_to_uint(hdr->c_filesize, 8);

        char *name = (char *)(p + sizeof(*hdr));

        if (namesize == 11 &&
            name[0]=='T' && name[1]=='R' && name[2]=='A' &&
            name[3]=='I' && name[4]=='L' && name[5]=='E' &&
            name[6]=='R') {
            bwrite(wo, "file not found: ");
            bwrite(wo, filename);
            bwrite(wo, "\n");
            return -2;
        }

        int match = 1;
        for (unsigned int i = 0; i < namesize - 1; i++) {
            if (filename[i] != name[i]) { match = 0; break; }
        }
        if (match && filename[namesize - 1] == 0) {
            uint8_t *data = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
            for (unsigned int i = 0; i < fsize; i++) {
                char c = (char)data[i];
                char buf[2] = {c, 0};
                bwrite(wo, buf);
            }
            return 0;
        }

        p = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
        p = (uint8_t *)align4((uintptr_t)(p + fsize));
    }
}

void cpio_list_files(struct writeout_t *wo, void *archive) {
    uint8_t *p = (uint8_t *)archive;

    while (1) {
        struct cpio_newc_header *hdr = (struct cpio_newc_header *)p;

        if (hdr->c_magic[0] != '0' || hdr->c_magic[1] != '7') {
            bwrite(wo, "invalid CPIO archive\n");
            return;
        }

        unsigned int namesize = hex_to_uint(hdr->c_namesize, 8);
        unsigned int fsize = hex_to_uint(hdr->c_filesize, 8);

        char *name = (char *)(p + sizeof(*hdr));

        // end of archive
        if (namesize == 11 &&
            name[0]=='T' && name[1]=='R' && name[2]=='A' &&
            name[3]=='I' && name[4]=='L' && name[5]=='E' &&
            name[6]=='R') {
            return;
        }

        // write filename
        for (unsigned int i = 0; i < namesize - 1; i++) {
            char buf[2] = {name[i], 0};
            bwrite(wo, buf);
        }
        bwrite(wo, "\n");

        // next header
        p = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
        p = (uint8_t *)align4((uintptr_t)(p + fsize));
    }
}