#pragma once

#include <stdint.h>

#define CONFIG_MAGIC 0x00000000666E6F63
#define HARDCODED_CONFIG_MAGIC 0x464E4F4344524148 // if no config is provided the system will default to a hardcoded config which uses this magic number
#define INITRAMFS_MAGIC 0x0000000074696E69
#define UNUSED(x) ((void)(x)) // helper macro

typedef struct {
    uint64_t magic; // 8   // should be  the same as CONFIG_MAGIC or HARDCODED_CONFIG_MAGIC
    uint64_t epoch; // 8
    uint8_t  debug; // 1
    uint8_t  flanterm_is_debug_interface; // 1
    uint8_t  uart16550_is_debug_interface; // 1
} __attribute__((packed)) kernel_config_t; _Static_assert(sizeof(kernel_config_t) == 19, "kernel_config_t struct is not expected size");

extern kernel_config_t global_kernel_config;
