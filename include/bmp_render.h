#ifndef BMP_RENDER_H
#define BMP_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "arch/limine.h"

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;      // 'BM'
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPFileHeader;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

void bmp_render(
    const void *bmp_data,
    const struct limine_framebuffer *fb,
    int dst_x,
    int dst_y
);

#ifdef __cplusplus
}
#endif

#endif // BMP_RENDER_H