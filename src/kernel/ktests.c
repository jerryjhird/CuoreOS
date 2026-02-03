/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "memory.h"
#include "stdio.h"
#include <stdint.h>
#include "x86.h"
#include "string.h"
#include "kernel.h"
#include "kernel.h"
#include "helpers.h"

void memory_test(void) {
    const size_t size = 16;
    char* block1 = zalloc(size);
    if (!block1) {
        kpanic("kernel panic!\nzalloc returned NULL\n");
    }

    // verify zeroed memory
    for (size_t i = 0; i < size; i++) {
        if (block1[i] != 0) {
            kpanic("kernel panic!\nzalloc did not zero memory\n");
        }
    }

    // write a known pattern
    for (size_t i = 0; i < size; i++) {
        block1[i] = (char)(i + 1);
    }

    // read the pattern back
    for (size_t i = 0; i < size; i++) {
        if (block1[i] != (char)(i + 1)) {
            kpanic("kernel panic!\nmemory write/read mismatch\n");
        }
    }

    // free
    free(block1);

    char* block2 = zalloc(size);
    if (block2 != block1) {
        kpanic("kernel panic!\nfree did not rewind heap pointer\n");
    }

    free(block2);

    logbuf_store(&klog, PASS_LOG_STR " memory_test successful\n");
    return;
}