#include "graphics.h"
#include "memory.h"
#include "kfont.h"
#include "stdint.h"

#define FONT_W 8
#define FONT_H 14

// fb

void fb_put_pixel(
    struct framebuffer *fb,
    uint32_t x,
    uint32_t y,
    uint32_t color
) {
    if (x >= fb->width || y >= fb->height)
        return;

    uint32_t bytes_per_pixel = fb->bpp / 8;

    uint8_t *p =
        (uint8_t *)fb->addr +
        y * fb->pitch +
        x * bytes_per_pixel;

    *(uint32_t *)p = color;
}

static void draw_glyph(
    struct framebuffer *fb,
    uint32_t x,
    uint32_t y,
    uint8_t *glyph,      // 1 byte per row
    uint32_t rows,       // glyph height
    uint32_t fg,
    uint32_t bg
) {
    for (uint32_t row = 0; row < rows; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < 8; col++) {  // 8-bit wide font
            uint32_t color = (bits & (1 << (7 - col))) ? fg : bg;
            fb_put_pixel(fb, x + col, y + row, color);
        }
    }
}

void fb_clear(struct framebuffer *fb, uint32_t color) {
    if (color == 0) {
        memset(fb->addr, 0, fb->pitch * fb->height);
        return;
    }

    for (uint32_t y = 0; y < fb->height; y++) {
        uint32_t *row =
            (uint32_t *)((uint8_t *)fb->addr + y * fb->pitch);
        for (uint32_t x = 0; x < fb->width; x++) {
            row[x] = color;
        }
    }
}

// terminal

void term_init(struct terminal *term, struct framebuffer *fb) {
    term->fb = fb;
    term->cursor_x = 0;
    term->cursor_y = 0;
    term->cols = fb->width / FONT_W;
    term->rows = fb->height / FONT_H;
}

static void term_scroll(struct terminal *term) {
    struct framebuffer *fb = term->fb;
    uint32_t row_height = FONT_H;

    memmove(
        fb->addr,
        (uint8_t *)fb->addr + fb->pitch * row_height,
        fb->pitch * (fb->height - row_height)
    );

    memset(
        (uint8_t *)fb->addr + fb->pitch * (fb->height - row_height),
        0,
        fb->pitch * row_height
    );

    term->cursor_y--;
}

void term_draw_char(struct terminal *term, char c, uint32_t fg, uint32_t bg) {
    if (c == '\n') {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows)
            term_scroll(term);
        return;
    }

    uint32_t px = term->cursor_x * FONT_W;
    uint32_t py = term->cursor_y * FONT_H;
    uint8_t *glyph = iso10_f14_psf + 4 + (uint8_t)c * FONT_H; // skip psf1 header
    draw_glyph(term->fb, px, py, glyph, FONT_H, fg, bg);

    term->cursor_x++;
    if (term->cursor_x >= term->cols) {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows)
            term_scroll(term);
    }
}

void term_write(void *ctx, const char *msg, size_t len) {
    struct terminal *term = (struct terminal *)ctx;

    for (size_t i = 0; i < len; i++) {
        char c = msg[i];

        if (c == '\n') {
            term->cursor_x = 0;
            term->cursor_y++;
            if (term->cursor_y >= term->rows)
                term_scroll(term);
        }
        else if (c == '\b') {
            // move cursor back
            if (term->cursor_x == 0) {
                if (term->cursor_y == 0)
                    continue;
                term->cursor_y--;
                term->cursor_x = term->cols - 1;
            } else {
                term->cursor_x--;
            }

            uint32_t px = term->cursor_x * FONT_W;
            uint32_t py = term->cursor_y * FONT_H;

            // draw space to erase char
            uint8_t *glyph =
                iso10_f14_psf + 4 + (' ' * FONT_H);
            draw_glyph(term->fb, px, py, glyph, FONT_H,
                       0x00FFFFFF, 0x00000000);
        }
        else {
            term_draw_char(term, c,
                           0x00FFFFFF,
                           0x00000000);
        }
    }
}