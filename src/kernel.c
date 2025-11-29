#include <stdint.h>
#include "flanterm.h"
#include "flanterm_backends/fb.h"
#include "limine.h"

static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

void _start(void) {
    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];

    uint32_t *framebuffer_ptr = (uint32_t *)fb->address;
    size_t width  = fb->width;
    size_t height = fb->height;
    size_t pitch = fb->pitch;   // pitch in bytes

    uint8_t red_mask_size    = fb->red_mask_size;
    uint8_t red_mask_shift   = fb->red_mask_shift;
    uint8_t green_mask_size  = fb->green_mask_size;
    uint8_t green_mask_shift = fb->green_mask_shift;
    uint8_t blue_mask_size   = fb->blue_mask_size;
    uint8_t blue_mask_shift  = fb->blue_mask_shift;
    
    struct flanterm_context *ft_ctx = flanterm_fb_init(
        NULL,
        NULL,
        framebuffer_ptr, width, height, pitch,
        red_mask_size, red_mask_shift,
        green_mask_size, green_mask_shift,
        blue_mask_size, blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0,
        0
    );

    const char msg[] = "\n\n\nhello world";
    flanterm_write(ft_ctx, msg, sizeof(msg));

    for (;;) __asm__("hlt");
}
