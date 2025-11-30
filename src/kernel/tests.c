#include "kernel/kmem.h"
#include "flanterm.h"

#include "stdio.h"
#include "stdint.h"
#include "kernel/kio.h"

void memory_test(buffer *buf_ctx, struct flanterm_context *ft_ctx) {
    const size_t size = 16;
    char* block1 = kzalloc(size);
    if (!block1) {
        klog(buf_ctx, ft_ctx, "[TEST/MEMORY] [FAIL] kzalloc returned NULL");
        return;
    }

    // verify zeroed memory
    for (size_t i = 0; i < size; i++) {
        if (block1[i] != 0) {
            klog(buf_ctx, ft_ctx, "[TEST/MEMORY] [FAIL] kzalloc did not zero memory");
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
            klog(buf_ctx, ft_ctx, "[TEST/MEMORY] [FAIL] memory write/read mismatch");
            kfree(block1, size);
            return;
        }
    }

    // Free
    kfree(block1, size);

    char* block2 = kzalloc(size);
    if (block2 != block1) {
        klog(buf_ctx, ft_ctx, "[TEST/MEMORY] [FAIL] kfree did not rewind heap pointer");
        kfree(block2, size);
        return;
    }

    kfree(block2, size);
    
    klog(buf_ctx, ft_ctx, "[TEST/MEMORY] [PASS] Test successful");
    flush(buf_ctx);
}

