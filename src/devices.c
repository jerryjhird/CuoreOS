#include "devices.h"

void dev_puts(kernel_dev_t* dev, const char* s) {
    while (*s) {
        dev->putc(*s++);
    }
}
