/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "stdint.h"
#include "bmp_render.h"
#include "limine.h"

static inline uint32_t limine_pack_rgb(
    const struct limine_framebuffer *fb,
    uint8_t r, uint8_t g, uint8_t b
) {
    uint32_t pixel = 0;

    pixel |= ((uint32_t)r >> (8 - fb->red_mask_size))   << fb->red_mask_shift;
    pixel |= ((uint32_t)g >> (8 - fb->green_mask_size)) << fb->green_mask_shift;
    pixel |= ((uint32_t)b >> (8 - fb->blue_mask_size))  << fb->blue_mask_shift;

    return pixel;
}

void bmp_render(
    const void *bmp_data,
    const struct limine_framebuffer *fb,
    int dst_x,
    int dst_y
) {
    const BMPFileHeader *file = bmp_data;
    const BMPInfoHeader *info =
        (const BMPInfoHeader *)((const uint8_t *)bmp_data + sizeof(BMPFileHeader));

    if (file->bfType != 0x4D42) return;
    if (info->biCompression != 0) return;
    if (info->biBitCount != 24 && info->biBitCount != 32) return;

    int width  = info->biWidth;
    int height = info->biHeight;
    int bpp    = info->biBitCount / 8;

    const uint8_t *pixels = (const uint8_t *)bmp_data + file->bfOffBits;
    int row_stride = ((width * bpp) + 3) & ~3;

    for (int y = 0; y < height; y++) {
        int src_y = height - 1 - y;

        uint8_t *fb_row =
            (uint8_t *)fb->address + (dst_y + y) * fb->pitch;

        for (int x = 0; x < width; x++) {
            int fb_x = dst_x + x;
            int fb_y = dst_y + y;

            if (fb_x < 0 || fb_x >= (int)fb->width ||
                fb_y < 0 || fb_y >= (int)fb->height)
                continue;

            const uint8_t *p = pixels + src_y * row_stride + x * bpp;

            uint8_t b = p[0];
            uint8_t g = p[1];
            uint8_t r = p[2];

            uint32_t pixel = limine_pack_rgb(fb, r, g, b);

            *(uint32_t *)(fb_row + fb_x * (fb->bpp / 8)) = pixel;
        }
    }
}