#include "flanterm.h"
#include "flanterm_backends/fb.h"

#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "x86.h"
#include "x86abs.h"

#include "kernel/kmem.h"
#include "kernel/kio.h"
#include "ktests.h"
#include "drivers/ps2.h"
#include "drivers/serial.h"
#include "limineabs.h"
#include "limine.h"

struct flanterm_context *term_ctx;

static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

void serial_write_adapter(const char *msg, size_t len, void *ctx) {
    (void)ctx;
    serial_write(msg, len);
}

void flanterm_write_adapter(const char *msg, size_t len, void *ctx) {
    flanterm_write((struct flanterm_context*)ctx, msg, len);
}


void exec(struct writeout_t *wo, const char *cmd) {
    switch (hash(cmd)) {
        case 0x0030CF41: // "help"
            bwrite(wo, "commands: memtest, panic, halt\n");
            break;
        case 0x3896F7E7: // "memtest"
            memory_test(wo);
            break;
        case 0x0030C041: // "halt"
            for (;;) __asm__ volatile("hlt");
            break;
        case 0x06580A77: // "panic"
            kpanic(wo);
            break;
        default:
            if (strlen(cmd) > 0) {
                bwrite(wo, "unknown command: ");
                bwrite(wo, cmd);
                bwrite(wo, "\n");
            }
            break;
    }
}

void _start(void) {
    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];
    uint32_t *framebuffer_ptr = (uint32_t *)fb->address;
    size_t width  = fb->width;
    size_t height = fb->height;
    size_t pitch = fb->pitch;

    uint8_t red_mask_size    = fb->red_mask_size;
    uint8_t red_mask_shift   = fb->red_mask_shift;
    uint8_t green_mask_size  = fb->green_mask_size;
    uint8_t green_mask_shift = fb->green_mask_shift;
    uint8_t blue_mask_size   = fb->blue_mask_size;
    uint8_t blue_mask_shift  = fb->blue_mask_shift;

    term_ctx = flanterm_fb_init(
        NULL, NULL,
        framebuffer_ptr, width, height, pitch,
        red_mask_size, red_mask_shift,
        green_mask_size, green_mask_shift,
        blue_mask_size, blue_mask_shift,
        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        0, 0, 0
    );

    // register the framebuffer terminal as a writeable interface
    struct writeout_t term_wo;
    term_wo.len = 0;
    term_wo.buf[0] = '\0';
    term_wo.write = flanterm_write_adapter;
    term_wo.ctx = term_ctx;

    serial_init();

    // register serial as a writeable interface
    struct writeout_t serial_wo;
    serial_wo.len = 0;
    serial_wo.buf[0] = '\0';
    serial_wo.write = serial_write_adapter;
    serial_wo.ctx = NULL;

    // define heap
    kheapinit((uint8_t*)0x100000, (uint8_t*)0x200000);
    memory_test(&term_wo);

    uint64_t count = limine_module_count();

    for (uint64_t i = 0; i < count; i++) {
        struct limine_file *mod = limine_get_module(i);

        klog(&term_wo);
        bwrite(&term_wo, "[ \x1b[94mINFO\x1b[0m ] (LIMINE/MOD:");
        uiota(&term_wo, i);
        bwrite(&term_wo, ") ");

        char hexbuf[32];
        if (mod) {
            ptrhex(hexbuf, mod->address);
            bwrite(&term_wo, hexbuf);
        } else {
            bwrite(&term_wo, "<NULL>");
        }
        bwrite(&term_wo, "\n");
    }

    int pdcount = ps2_dev_count();
    
    klog(&term_wo);
    bwrite(&term_wo, "[ \x1b[94mINFO\x1b[0m ] (PS/2) Devices: ");
    uiota(&term_wo, (uint64_t)pdcount);
    bwrite(&term_wo, "\n");

    klog(&term_wo);
    bwrite(&term_wo, "[ \x1b[94mINFO\x1b[0m ] (CPU) Brand: ");
    cpu_brand(&term_wo);
    bwrite(&term_wo, "\n");

    klog(&serial_wo);
    bwrite(&serial_wo, "Finished Booting\n");
    flush(&serial_wo);

    char line[256];

    // shell
    while (1) {
        flush(&term_wo);
        write(&term_wo, "> ", 2);
        readline(&term_wo, line, 256);
        exec(&term_wo, line);
    }

    for (;;) __asm__ volatile("hlt");
}
