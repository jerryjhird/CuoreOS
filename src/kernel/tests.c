#include "memory.h"
#include "stdio.h"
#include "stdint.h"

void memory_test(struct writeout_t *wo) {
    const size_t size = 16;
    char* block1 = zalloc(size);
    if (!block1) {
        write_epoch(wo);
        bwrite(wo, "[ \x1b[31mFAIL \x1b[0m] zalloc returned NULL\n");
        return;
    }

    // verify zeroed memory
    for (size_t i = 0; i < size; i++) {
        if (block1[i] != 0) {
            write_epoch(wo);
            bwrite(wo, "[ \x1b[31mFAIL \x1b[0m] zalloc did not zero memory\n");
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
            write_epoch(wo);
            bwrite(wo, "[ \x1b[31mFAIL \x1b[0m] memory write/read mismatch\n");
            free(block1, size);
            return;
        }
    }

    // Free
    free(block1, size);

    char* block2 = zalloc(size);
    if (block2 != block1) {
        write_epoch(wo);
        bwrite(wo, "[ \x1b[31mFAIL\x1b[0m ] free did not rewind heap pointer\n");
        free(block2, size);
        return;
    }

    free(block2, size);
    
    write_epoch(wo);
    bwrite(wo, "[ \x1b[92mPASS\x1b[0m ] memory_test successful\n");
    flush(wo);        
}

