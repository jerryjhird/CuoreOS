#ifndef CUORETERM_H
#define CUORETERM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct terminal {
    void *fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;

    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;
    uint8_t red_size;
    uint8_t green_size;
    uint8_t blue_size;

    uint32_t fgcol;

    uint32_t cursor_x;
    uint32_t cursor_y;

    const uint8_t *font_data;
    uint32_t font_width;
    uint32_t font_height;
    uint32_t cols;
    uint32_t rows;
};

void cuoreterm_init(
    struct terminal *term,
    void *fb_addr,
    uint32_t fb_width,
    uint32_t fb_height,
    uint32_t fb_pitch,
    uint32_t fb_bpp,
    uint8_t r_shift, uint8_t g_shift, uint8_t b_shift,
    uint8_t r_size,  uint8_t g_size,  uint8_t b_size,
    const uint8_t *font,
    uint32_t font_w,
    uint32_t font_h
);

void cuoreterm_write(void *ctx, const char *msg, uint64_t len);
void cuoreterm_draw_char(struct terminal *term, char c, uint32_t fg);
void cuoreterm_set_font(struct terminal *term, const uint8_t *font, uint32_t font_w, uint32_t font_h);
void cuoreterm_clear(struct terminal *term);

#ifdef __cplusplus
}
#endif


#ifdef CUORETERM_IMPL
static void *h_memset(void *dst, uint8_t v, uint32_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = v;
    return dst;
}

static void *h_memmove(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

static uint32_t fb_pack_color(struct terminal *term, uint32_t rgb) {
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8)  & 0xFF;
    uint32_t b =  rgb        & 0xFF;

    r >>= (8 - term->red_size);
    g >>= (8 - term->green_size);
    b >>= (8 - term->blue_size);

    return (r << term->red_shift) |
           (g << term->green_shift) |
           (b << term->blue_shift);
}

static void fb_put_pixel(struct terminal *term, uint32_t x, uint32_t y, uint32_t color) {
    if (x >= term->fb_width || y >= term->fb_height) return;
    uint32_t pixel = fb_pack_color(term, color);
    uint32_t bpp = term->fb_bpp / 8;
    uint8_t *p = (uint8_t *)term->fb_addr + y * term->fb_pitch + x * bpp;

    if (bpp == 4) {
        *(uint32_t *)p = pixel;
    } else if (bpp == 3) {
        p[0] = pixel & 0xFF;
        p[1] = (pixel >> 8) & 0xFF;
        p[2] = (pixel >> 16) & 0xFF;
    }
}

static void draw_glyph(struct terminal *term, uint32_t x, uint32_t y,
                       const uint8_t *glyph, uint32_t rows,
                       uint32_t fg) {
    for (uint32_t r = 0; r < rows; r++) {
        uint8_t bits = glyph[r];
        for (uint32_t c = 0; c < 8; c++) {
            if (bits & (1 << (7 - c)))
                fb_put_pixel(term, x + c, y + r, fg);
        }
    }
}

void cuoreterm_init(
    struct terminal *term,
    void *fb_addr,
    uint32_t fb_width,
    uint32_t fb_height,
    uint32_t fb_pitch,
    uint32_t fb_bpp,
    uint8_t r_shift, uint8_t g_shift, uint8_t b_shift,
    uint8_t r_size,  uint8_t g_size,  uint8_t b_size,
    const uint8_t *font,
    uint32_t font_w,
    uint32_t font_h
) {
    term->fb_addr   = fb_addr;
    term->fb_width  = fb_width;
    term->fb_height = fb_height;
    term->fb_pitch  = fb_pitch;
    term->fb_bpp    = fb_bpp;

    term->fgcol = 0xFFFFFF;

    term->red_shift   = r_shift;
    term->green_shift = g_shift;
    term->blue_shift  = b_shift;
    term->red_size    = r_size;
    term->green_size  = g_size;
    term->blue_size   = b_size;

    term->cursor_x = 0;
    term->cursor_y = 0;
    term->font_data   = font;
    term->font_width  = font_w;
    term->font_height = font_h;
    term->cols = fb_width  / font_w;
    term->rows = fb_height / font_h;
}

static void term_scroll(struct terminal *term) {
    uint32_t row_bytes = term->fb_pitch * term->font_height;
    uint32_t total     = term->fb_pitch * term->fb_height;

    h_memmove(term->fb_addr,
              (uint8_t *)term->fb_addr + row_bytes,
              total - row_bytes);

    h_memset((uint8_t *)term->fb_addr + total - row_bytes, 0, row_bytes);

    if (term->cursor_y > 0) term->cursor_y--;
}

void cuoreterm_draw_char(struct terminal *term, char c, uint32_t fg) {
    if (c == '\n') {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows) term_scroll(term);
        return;
    }

    uint32_t px = term->cursor_x * term->font_width;
    uint32_t py = term->cursor_y * term->font_height;

    const uint8_t *glyph = term->font_data + 4 + ((uint8_t)c * term->font_height);
    draw_glyph(term, px, py, glyph, term->font_height, fg);

    term->cursor_x++;
    if (term->cursor_x >= term->cols) {
        term->cursor_x = 0;
        term->cursor_y++;
        if (term->cursor_y >= term->rows) term_scroll(term);
    }
}

static uint32_t hex_digit(char c) {
    if (c >= '0' && c <= '9') return (uint32_t)(c - '0');
    if (c >= 'a' && c <= 'f') return 10 + (uint32_t)(c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (uint32_t)(c - 'A');
    return 0;
}

static uint32_t parse_hex_color(const char *s) {
    uint32_t color = 0;
    for (uint32_t i = 0; i < 6; i++)
        color = (color << 4) | hex_digit(*s++);
    return color;
}

static void handle_hex_ansi(struct terminal *term, char **p_c) {
    char *c = *p_c;
    if (!c || *c != '[') return;
    c++;

    if (*c == '0' && *(c + 1) == 'm') {
        term->fgcol = 0xFFFFFF;
        *p_c = c + 2;
        return;
    }

    if (*c == '#') {
        c++;
        term->fgcol = parse_hex_color(c);
        c += 6;
    }

    if (*c == 'm') c++;
    *p_c = c;
}

void cuoreterm_write(void *ctx, const char *msg, uint64_t len) {
    struct terminal *term = (struct terminal *)ctx;
    char *p_c = (char *)msg;
    char *end = p_c + len;

    while (p_c < end) {
        char c = *p_c++;

        if (c == '\n') {
            term->cursor_x = 0;
            term->cursor_y++;
            if (term->cursor_y >= term->rows) term_scroll(term);
        }
        else if (c == '\b') {
            if (term->cursor_x == 0) {
                if (term->cursor_y == 0) continue;
                term->cursor_y--;
                term->cursor_x = term->cols - 1;
            } else {
                term->cursor_x--;
            }

            uint32_t px = term->cursor_x * term->font_width;
            uint32_t py = term->cursor_y * term->font_height;

            for (uint32_t y = 0; y < term->font_height; y++) {
                for (uint32_t x = 0; x < term->font_width; x++) {
                    fb_put_pixel(term, px + x, py + y, 0x000000);
                }
            }
        }
        else if (c == '\x1b') {
            if (p_c < end && *p_c == '[')
                handle_hex_ansi(term, &p_c);
        }
        else {
            cuoreterm_draw_char(term, c, term->fgcol);
        }
    }
}

void cuoreterm_set_font(struct terminal *term, const uint8_t *font, uint32_t font_w, uint32_t font_h) {
    term->font_data   = font;
    term->font_width  = font_w;
    term->font_height = font_h;
    term->cols = term->fb_width / font_w;
    term->rows = term->fb_height / font_h;
}

void cuoreterm_clear(struct terminal *term) {
    h_memset(term->fb_addr, 0x00, term->fb_pitch * term->fb_height);
    term->cursor_x = 0;
    term->cursor_y = 0;
}

#endif // CUORETERM_IMPL
#endif // CUORETERM_H
