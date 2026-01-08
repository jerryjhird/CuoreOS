#include "memory.h"
#include "stdio.h"
#include "stdint.h"
#include "arch/x86_64.h"
#include "string.h"

#define CRC32C_UWU 0xEC416DF0

void memory_test(struct writeout_t *wo) {
    const size_t size = 16;
    char* block1 = zalloc(size);
    if (!block1) {
        lbwrite(wo, "[ FAIL ] zalloc returned NULL\n", 26);
        return;
    }

    // verify zeroed memory
    for (size_t i = 0; i < size; i++) {
        if (block1[i] != 0) {
            lbwrite(wo, "[ FAIL ] zalloc did not zero memory\n", 35);
            free(block1, size);
            return;
        }
    }

    // write a known pattern
    for (size_t i = 0; i < size; i++) {
        block1[i] = (char)(i + 1);  // 1, 2, 3, ...
    }

    // read the pattern back
    for (size_t i = 0; i < size; i++) {
        if (block1[i] != (char)(i + 1)) {
            lbwrite(wo, "[ FAIL ] memory write/read mismatch\n", 38);
            free(block1, size);
            return;
        }
    }

    // Free
    free(block1, size);

    char* block2 = zalloc(size);
    if (block2 != block1) {
        lbwrite(wo, "[ FAIL ] free did not rewind heap pointer\n", 41);
        free(block2, size);
        return;
    }

    free(block2, size);

    lbwrite(wo, "[ PASS ] memory_test successful\n", 32);
}

void hash_test(struct writeout_t *wo, uint32_t (**hash)(const char *)) {
    if (*hash == crc32c_hwhash) {
        uint32_t hw = crc32c_hwhash("uwu");
        uint32_t sw = crc32c_swhash("uwu");

        if (hw == CRC32C_UWU)
            bwrite(wo, "[ PASS ] crc32c_hwhash matches expected hash\n");
        else {
            bwrite(wo, "[ FAIL ] crc32c_hwhash does NOT match expected hash\n");
            if (sw == CRC32C_UWU) {
                bwrite(wo, "[ INFO ] Falling back to crc32c_swhash\n");
                *hash = crc32c_swhash;
            } else {
                bwrite(wo, "[ FAIL ] Both HW and SW crc32c checks failed!\n");
            }
        }

        if (sw != CRC32C_UWU)
            bwrite(wo, "[ WARN ] SW hash isnt functional, may break CPUs without CRC32C support\n");

    } else if (*hash == crc32c_swhash) {
        bwrite(wo,
            crc32c_swhash("uwu") == CRC32C_UWU
                ? "[ PASS ] crc32c_swhash matches expected hash\n"
                : "[ FAIL ] crc32c_swhash does NOT match expected hash\n");
    } else {
        bwrite(wo, "[ WARN ] Unknown hash function pointer!\n");
    }
}
