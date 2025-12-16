#ifndef GRAPHICS_H
#define GRAPHICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

struct framebuffer {
    void *addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;     // bytes per scanline
    uint8_t  bpp;

    uint8_t r_size, r_shift;
    uint8_t g_size, g_shift;
    uint8_t b_size, b_shift;
};

struct terminal {
    struct framebuffer *fb;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t cols;
    uint32_t rows;
};

void term_init(struct terminal *term, struct framebuffer *fb);
void term_draw_char(
    struct terminal *term,
    char c,
    uint32_t fg,
    uint32_t bg
);

void term_write(void *ctx, const char *msg, size_t len);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_H