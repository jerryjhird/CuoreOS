#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t DevCAP; // capabilities bitmask
    void (*putc)(char c);
    void (*write)(const char* str);
    void (*lwrite)(const char *msg, size_t len);
} output_dev_t;

#define SETUP_OUTPUT_DEVICE(name, _cap, _putc, _write) \
    output_dev_t name = {                                        \
        .DevCAP = (_cap),                                        \
        .putc   = _putc,                                         \
        .write  = _write,                                        \
    }

#define FILL_OUTPUT_DEVICE(ptr, _cap, _putc, _write) \
    do {                                                     \
        (ptr)->DevCAP = (_cap);                              \
        (ptr)->putc   = _putc;                               \
        (ptr)->write  = _write;                              \
    } while(0)

#define REGISTER_OUTPUT_DEVICE(ptr, list, counter) \
    do { \
        if ((ptr) != NULL && (counter) < MAX_DEVICES) { \
            (list)[(counter)++] = (ptr); \
        } \
    } while(0)

// what to feed the device
#define CAP_ON_ERROR (1ULL << 0) // device should be used to display unrecoverable error's (e.g cpu exceptions)
#define CAP_ON_DEBUG (1ULL << 1) // device should be used to display debug messages

// color capabilities
#define CAP_ANSI_4BIT (1ULL << 2) // device supports ANSI X3.64
#define CAP_ANSI_8BIT (1ULL << 3) // device supports ANSI 256-color
#define CAP_ANSI_24BIT (1ULL << 4) // device supports ANSI TrueColor

#define DEV_CAP_SET(dev_ptr, cap) ((dev_ptr)->DevCAP |= (cap))
#define DEV_CAP_CLEAR(dev_ptr, cap) ((dev_ptr)->DevCAP &= ~(cap))
#define DEV_CAP_HAS(dev_ptr, cap) (((dev_ptr)->DevCAP & (cap)) == (cap))
