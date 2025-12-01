#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "flanterm.h"
#include "flanterm_backends/fb.h"
#include "limine.h"

#include "kernel/kmem.h"
#include "kernel/kio.h"
#include "ktests.h"
#include "x86.h"
#include "limineabs.h"
#include "drivers/ps2.h"

static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

void exec(const char *cmd) {
    switch (hash(cmd)) {
        case 0x0030CF41: // "help"
            bwrite("commands: memtest, panic, halt\n");
            break;
        case 0x3896F7E7: // "memtest"
            memory_test();
            break;
        case 0x0030C041: // "halt"
            for (;;) __asm__ volatile("hlt");
            break;
        case 0x06580A77: // "panic"
            kpanic();
            break;
        default:
            if (strlen(cmd) > 0) {
                bwrite("unknown command: ");
                bwrite(cmd);
                bwrite("\n");
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

    g_ft_ctx = flanterm_fb_init(
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

    kheapinit((uint8_t*)0x100000, (uint8_t*)0x200000);
    memory_test();

    uint64_t count = limine_module_count();

    for (uint64_t i = 0; i < count; i++) {
        struct limine_file *mod = limine_get_module(i);

        char numbuf[32];
        char hexbuf[32];

        klog();
        bwrite("[ \x1b[94mINFO\x1b[0m ] (LIMINE/MOD:");
        uitoa(i, numbuf);
        bwrite(numbuf);
        bwrite(") ");

        if (mod) {
            ptrhex(hexbuf, mod->address);
            bwrite(hexbuf);
        } else {
            bwrite("<NULL>");
        }
        bwrite("\n");
    }

    int pdcount = ps2_dev_count();
    char pdbuf[16];
    uitoa((uint64_t)pdcount, pdbuf);

    klog();
    bwrite("[ \x1b[94mINFO\x1b[0m ] (PS/2) Devices: ");
    bwrite(pdbuf);
    bwrite("\n");

    bwrite(
        "\x1b[31m"
        "_________                            ________    _________\n"
        "\\_   ___ \\ __ __  ___________   ____ \\_____  \\  /   _____/\n"
        "/    \\  \\/|  |  \\/  _ \\_  __ \\_/ __ \\ /   |   \\ \\_____  \\ \n"
        "\\     \\___|  |  (  <_> )  | \\\\/  ___//    |    \\/        \\\n"
        " \\______  /____/ \\____/|__|    \\___  >_______  /_______  /\n"
        "        \\/                         \\/        \\/        \\/\n"
        "\x1b[0m"
    );

    char line[256];

    // shell
    while (1) {
        flush();
        write("> ", 2);
        readline(line, 256);
        exec(line);
    }

    for (;;) __asm__ volatile("hlt");
}
