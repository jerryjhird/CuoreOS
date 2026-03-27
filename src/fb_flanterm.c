#include "../../build/flanterm/src/flanterm.c"
#include "../../build/flanterm/src/flanterm_backends/fb.c"
#include "../../build/flanterm/src/flanterm.h"
#include "../../build/flanterm/src/flanterm_backends/fb.h"
#include "limine.h"
#include "devicetypes.h"
#include "devices.h"
#include "fb_flanterm.h"
#include "mem/heap.h"
#include <stdint.h>
#include "kstate.h"

struct flanterm_context *ft_ctx = NULL;

SETUP_OUTPUT_DEVICE(flanterm_dev, 
    CAP_ANSI_24BIT | CAP_ANSI_8BIT | CAP_ANSI_4BIT  | CAP_ON_ERROR,
    _c_flanterm_putc, _c_flanterm_write
);

void _c_flanterm_init(struct limine_framebuffer *fb) {
    REGISTER_OUTPUT_DEVICE(&flanterm_dev, output_devices, output_devices_c);
    if (global_kernel_config.flanterm_is_debug_interface) {DEV_CAP_SET(&flanterm_dev, CAP_ON_DEBUG);}

    ft_ctx = flanterm_fb_init(
        malloc,
        sfree,
        fb->address,
        fb->width,
        fb->height,
        fb->pitch,
        fb->red_mask_size,
        fb->red_mask_shift,
        fb->green_mask_size,
        fb->green_mask_shift,
        fb->blue_mask_size,
        fb->blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL,
        0, 0,
        1,
        0, 0,
        0,
        0
    );
}

void _c_flanterm_free() {
    flanterm_deinit(ft_ctx, sfree);
}

void _c_flanterm_putc(char c) {
    if (!ft_ctx) return;

    flanterm_write(ft_ctx, &c, 1);
    
    if (c == '\n') {
        char r = '\r';
        flanterm_write(ft_ctx, &r, 1);
    }
}

void _c_flanterm_lwrite(const char *msg, size_t len) {
    if (!ft_ctx || !msg) return;

    size_t start = 0;
    for (size_t i = 0; i < len; i++) {
        if (msg[i] == '\n') {
            flanterm_write(ft_ctx, msg + start, (i - start) + 1);
            
            char r = '\r';
            flanterm_write(ft_ctx, &r, 1);

            start = i + 1;
        }
    }

    if (start < len) {
        flanterm_write(ft_ctx, msg + start, len - start);
    }
}

void _c_flanterm_write(const char* str) {
    if (!str) return;
    
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }

    _c_flanterm_lwrite(str, len);
}

void _c_flanterm_puthex(uint64_t val) {
    const char *hex_chars = "0123456789ABCDEF";

    _c_flanterm_lwrite("0x", 2);

    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (val >> (i * 4)) & 0xF;
        _c_flanterm_putc(hex_chars[nibble]);
    }
}