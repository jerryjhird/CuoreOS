#ifndef CUORETERM_H
#define CUORETERM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct terminal {
    void *fb_addr;
    uint32_t fb_width, fb_height, fb_pitch, fb_bpp;

    uint32_t fgcol;
    uint32_t cursor_x, cursor_y;
    uint8_t r_offset, g_offset, b_offset; // byte offsets in fb pixel
    uint8_t pixel_bytes; // number of bytes per pixel

    const uint8_t *font_data;
    uint32_t font_width, font_height;

    uint32_t cols, rows;
};

void cuoreterm_init(
    struct terminal *term,
    void *fb_addr,
    uint32_t fb_width,
    uint32_t fb_height,
    uint32_t fb_pitch,
    uint32_t fb_bpp,
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
    uint8_t *p = dst;
    uint32_t val32 = v | (v << 8) | (v << 16) | (v << 24);

    while (((uintptr_t)p & 3) && n--) *p++ = v;

    uint32_t *p32 = (uint32_t*)p;
    while (n >= 4) { *p32++ = val32; n -= 4; }

    p = (uint8_t*)p32;
    while (n--) *p++ = v;

    return dst;
}

static void *h_memmove(void *dst, const void *src, uint32_t n) {
    if (!n || dst == src) return dst;

    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;

    if (d < s || d >= s + n) {
        // align dest to 4 or 8 bytes
        while (((uintptr_t)d & 7) && n) { *d++ = *s++; n--; }

        uint64_t *d64 = (uint64_t*)d;
        const uint64_t *s64 = (const uint64_t*)s;

        while (n >= 8) { *d64++ = *s64++; n -= 8; }

        d = (uint8_t*)d64;
        s = (const uint8_t*)s64;
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;

        while (((uintptr_t)d & 7) && n) { *--d = *--s; n--; }

        uint64_t *d64 = (uint64_t*)d;
        const uint64_t *s64 = (const uint64_t*)s;

        while (n >= 8) { *--d64 = *--s64; n -= 8; }

        d = (uint8_t*)d64;
        s = (const uint8_t*)s64;
        while (n--) *--d = *--s;
    }

    return dst;
}

static inline void fb_pixel(struct terminal *term, uint32_t x, uint32_t y, uint32_t fg) {
    if (x >= term->fb_width || y >= term->fb_height) return;

    uint8_t *p = (uint8_t*)term->fb_addr + y * term->fb_pitch + x * term->pixel_bytes;

    switch(term->pixel_bytes) {
        case 4: // 32bpp ARGB/RGB
        case 3: // 24bpp RGB
            p[term->r_offset] = (uint8_t)((fg >> 16) & 0xFF);
            p[term->g_offset] = (uint8_t)((fg >> 8) & 0xFF);
            p[term->b_offset] = (uint8_t)(fg & 0xFF);
            break;
        case 2: { // RGB565
            uint16_t val = ((((fg >> 16) * 31 + 127) / 255) << 11) |
                           ((((fg >> 8 & 0xFF) * 63 + 127) / 255) << 5) |
                           (((fg & 0xFF) * 31 + 127) / 255);
            *(uint16_t*)p = val;
            break;
        }
        case 1: // grayscale
            p[0] = (uint8_t)(((fg >> 16) + ((fg >> 8) & 0xFF) + (fg & 0xFF)) / 3);
            break;
    }
}

void cuoreterm_init(
    struct terminal *term,
    void *fb_addr,
    uint32_t fb_width,
    uint32_t fb_height,
    uint32_t fb_pitch,
    uint32_t fb_bpp,
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

    term->cursor_x = 0;
    term->cursor_y = 0;
    term->pixel_bytes = fb_bpp / 8;

    cuoreterm_set_font(term, font, font_w, font_h);

    switch(fb_bpp) {
        case 32: term->r_offset=2; term->g_offset=1; term->b_offset=0; break; // ARGB
        case 24: term->r_offset=0; term->g_offset=1; term->b_offset=2; break; // RGB
        case 16: term->r_offset=0; term->g_offset=0; term->b_offset=0; break; // RGB565
        case 8:  term->r_offset=0; term->g_offset=0; term->b_offset=0; break; // grayscale
        default: term->r_offset=0; term->g_offset=0; term->b_offset=0; break;
    }
}

static void term_scroll(struct terminal *term) {
    uint32_t nrows = 1;

    uint32_t row_bytes = term->fb_pitch * term->font_height * nrows;
    uint8_t *fb = (uint8_t*)term->fb_addr;

    h_memmove(fb, fb + row_bytes, (term->fb_height - term->font_height * nrows) * term->fb_pitch);
    h_memset(fb + (term->fb_height - term->font_height * nrows) * term->fb_pitch, 0x00, row_bytes);

    term->cursor_y = (term->cursor_y >= nrows) ? (term->cursor_y - nrows) : 0;
}

void cuoreterm_draw_char(struct terminal *term, char c, uint32_t fg) {
    if (c == '\n') { term->cursor_x = 0; term->cursor_y++; if(term->cursor_y >= term->rows) term_scroll(term); return; }

    uint32_t px = term->cursor_x * term->font_width;
    uint32_t py = term->cursor_y * term->font_height;

    const uint8_t *glyph = term->font_data + 4 + ((uint8_t)c * term->font_height);

    uint16_t fg16 = (((fg >> 19) & 0x1F) << 11) | (((fg >> 10) & 0x3F) << 5) | ((fg >> 3) & 0x1F);
    uint8_t fg_gray = ((fg>>16) + ((fg>>8)&0xFF) + (fg&0xFF)) / 3;

    for (uint32_t r = 0; r < term->font_height; r++) {
        uint8_t bits = glyph[r];
        uint8_t *row_ptr = (uint8_t*)term->fb_addr + (py + r) * term->fb_pitch + px * term->pixel_bytes;

        switch(term->pixel_bytes) {
            case 4:
            case 3: {
                uint8_t r_off = term->r_offset, g_off = term->g_offset, b_off = term->b_offset;
                for (int col = 0; col < 8; col++, row_ptr += term->pixel_bytes)
                    if (bits & (1 << (7 - col))) {
                        row_ptr[r_off] = (uint8_t)((fg >> 16) & 0xFF);
                        row_ptr[g_off] = (uint8_t)((fg >> 8) & 0xFF);
                        row_ptr[b_off] = (uint8_t)(fg & 0xFF);
                    }
                break;
            }
            case 2:
                for (int col = 0; col < 8; col++, row_ptr += 2)
                    if (bits & (1 << (7 - col))) *(uint16_t*)row_ptr = fg16;
                break;
            case 1:
                for (int col = 0; col < 8; col++, row_ptr++)
                    if (bits & (1 << (7 - col))) *row_ptr = fg_gray;
                break;
        }
    }

    term->cursor_x++;
    if (term->cursor_x >= term->cols) { term->cursor_x = 0; term->cursor_y++; if(term->cursor_y >= term->rows) term_scroll(term); }
}

static uint8_t hex_lut[256] = {
    ['0']=0, ['1']=1, ['2']=2, ['3']=3, ['4']=4,
    ['5']=5, ['6']=6, ['7']=7, ['8']=8, ['9']=9,
    ['a']=10,['b']=11,['c']=12,['d']=13,['e']=14,['f']=15,
    ['A']=10,['B']=11,['C']=12,['D']=13,['E']=14,['F']=15,
};

static inline uint32_t hex_digit(char c) {
    return hex_lut[(uint8_t)c];
}

static inline uint32_t parse_hex_color(const char *s) {
    return (hex_lut[(uint8_t)s[0]] << 20) |
           (hex_lut[(uint8_t)s[1]] << 16) |
           (hex_lut[(uint8_t)s[2]] << 12) |
           (hex_lut[(uint8_t)s[3]] << 8)  |
           (hex_lut[(uint8_t)s[4]] << 4)  |
            hex_lut[(uint8_t)s[5]];
}

static inline void handle_hex_ansi(struct terminal *term, char **p_c) {
    char *c = *p_c;
    if (*c++ != '[') return;

    if (c[0] == '0' && c[1] == 'm') {
        term->fgcol = 0xFFFFFF;
        *p_c = c + 2;
        return;
    }

    if (*c == '#') {
        term->fgcol = parse_hex_color(c + 1);
        c += 7;
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

            uint8_t *start = (uint8_t *)term->fb_addr + py * term->fb_pitch + px * (term->fb_bpp / 8);
            for (uint32_t r = 0; r < term->font_height; r++) {
                h_memset(start + r * term->fb_pitch, 0x00, term->font_width * (term->fb_bpp / 8));
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
