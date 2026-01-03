#include "memory.h"
#include "stdio.h"
#include "stdint.h"
#include "arch/x86_64.h"
#include "string.h"

void memory_test(struct writeout_t *wo) {
    const size_t size = 16;
    char* block1 = zalloc(size);
    if (!block1) {
        printf(wo, "[%u] [ FAIL ] zalloc returned NULL\n", get_epoch());
        return;
    }

    // verify zeroed memory
    for (size_t i = 0; i < size; i++) {
        if (block1[i] != 0) {
            printf(wo, "[%u] [ FAIL ] zalloc did not zero memory\n", get_epoch());
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
            printf(wo, "[%u] [ FAIL ] memory write/read mismatch\n", get_epoch());
            free(block1, size);
            return;
        }
    }

    // Free
    free(block1, size);

    char* block2 = zalloc(size);
    if (block2 != block1) {
        printf(wo, "[%u] [ FAIL ] free did not rewind heap pointer\n", get_epoch());
        free(block2, size);
        return;
    }

    free(block2, size);

    printf(wo, "[%u] [ PASS ] memory_test successful\n", get_epoch());
    flush(wo);        
}

void hash_test(struct writeout_t *wo) {
    const char *tests[] = {"hello", "world", "123456789", "the quick brown fox jumps over the lazy dog", "uwu", "nya"};

    const size_t ntests = sizeof(tests) / sizeof(tests[0]);
    int failed = 0;

    for (size_t i = 0; i < ntests; i++) {
        const char *s = tests[i];

        uint32_t hw = crc32c_hwhash(s);
        uint32_t sw = crc32c_swhash(s);

        if (hw != sw) {
            failed = 1;   // mark as failed if any mismatch occurs
            break;        // no need to continue once a failure is found
        }
    }

    if (failed) {
        printf(wo, "[%u] [ FAIL ] Hash Test\n", get_epoch());
    } else {
        printf(wo, "[%u] [ PASS ] Hash Test\n", get_epoch());
    }

    flush(wo);
}
