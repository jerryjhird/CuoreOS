#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "fs/cpio_newc.h"

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


int cpio_read_file(struct writeout_t *wo, void *archive, const char *filename) {
    uint8_t *p = (uint8_t *)archive;

    while (1) {
        struct cpio_newc_header *hdr = (struct cpio_newc_header *)p;

        // CPIO magic
        if (hdr->c_magic[0] != '0' || hdr->c_magic[1] != '7') {
            lbwrite(wo, "invalid CPIO archive\n", 21);
            return -1;
        }

        unsigned int namesize = hex_to_uint(hdr->c_namesize, 8);
        unsigned int fsize = hex_to_uint(hdr->c_filesize, 8);

        char *name = (char *)(p + sizeof(*hdr));

        if (namesize == 11 && !strncmp(name, "TRAILER", 7)) {
            printf(wo, "file not found: %s\n", filename);
            return -2;
        }

        if (namesize - 1 == strlen(filename) && !strncmp(name, filename, namesize - 1)) {
            uint8_t *data = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
            lbwrite(wo, (const char *)data, fsize); // bulk write
            return 0;
        }

        // advance to next header
        p = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
        p = (uint8_t *)align4((uintptr_t)(p + fsize));
    }
}
void cpio_list_files(struct writeout_t *wo, void *archive) {
    uint8_t *p = (uint8_t *)archive;

    while (1) {
        struct cpio_newc_header *hdr = (struct cpio_newc_header *)p;

        if (hdr->c_magic[0] != '0' || hdr->c_magic[1] != '7') {
            lbwrite(wo, "invalid CPIO archive\n", 21);
            return;
        }

        unsigned int namesize = hex_to_uint(hdr->c_namesize, 8);
        unsigned int fsize = hex_to_uint(hdr->c_filesize, 8);

        char *name = (char *)(p + sizeof(*hdr));

        // end of archive
        if (namesize == 11 && !strncmp(name, "TRAILER", 7)) return;

        lbwrite(wo, name, namesize - 1);
        lbwrite(wo, "\n", 1);

        p = (uint8_t *)align4((uintptr_t)(p + sizeof(*hdr) + namesize));
        p = (uint8_t *)align4((uintptr_t)(p + fsize));
    }
}
