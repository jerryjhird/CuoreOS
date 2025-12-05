#include "kernel/kmem.h"
#include "flanterm.h"

#include "stdio.h"
#include "stdint.h"
#include "kernel/kio.h"

void memory_test(struct writeout_t *wo) {
    const size_t size = 16;
    char* block1 = kzalloc(size);
    if (!block1) {
        klog(wo);
        bwrite(wo, "[ \x1b[31mFAIL \x1b[0m] kzalloc returned NULL\n");
        return;
    }

    // verify zeroed memory
    for (size_t i = 0; i < size; i++) {
        if (block1[i] != 0) {
            klog(wo);
            bwrite(wo, "[ \x1b[31mFAIL \x1b[0m] kzalloc did not zero memory\n");
            kfree(block1, size);
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
            klog(wo);
            bwrite(wo, "[ \x1b[31mFAIL \x1b[0m] memory write/read mismatch\n");
            kfree(block1, size);
            return;
        }
    }

    // Free
    kfree(block1, size);

    char* block2 = kzalloc(size);
    if (block2 != block1) {
        klog(wo);
        bwrite(wo, "[ \x1b[31mFAIL\x1b[0m ] kfree did not rewind heap pointer\n");
        kfree(block2, size);
        return;
    }

    kfree(block2, size);
    
    klog(wo);
    bwrite(wo, "[ \x1b[92mPASS\x1b[0m ] memory_test successful\n");
    flush(wo);        
}

