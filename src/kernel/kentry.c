#include "kernel.h"

#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "arch/cwarch.h"
#include "memory.h"

#include "drivers/ps2.h"
#include "drivers/serial.h"
#include "fs/cpio_newc.h"
#include "arch/limine.h"

#include "graphics.h"

struct limine_file *initramfs_mod = NULL;

static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

static inline void serial_write_adapter(const char *msg, size_t len, void *ctx) {
    (void)ctx;
    serial_write(msg, len);
}

void term_write_adapter(const char *msg, size_t len, void *ctx) {
    term_write(ctx, msg, len);
}

void exec(struct writeout_t *wo, const char *cmd) {
    while (*cmd == ' ') cmd++;

    // find first space
    const char *space = cmd;
    while (*space && *space != ' ') space++;

    // copy command into buffer
    char command[32];
    size_t cmd_len = space - cmd;
    if (cmd_len >= sizeof(command)) cmd_len = sizeof(command) - 1;
    memcpy(command, cmd, cmd_len);
    command[cmd_len] = '\0';

    // get arg
    const char *arg = space;
    while (*arg == ' ') arg++;

    switch (hash(command)) {
        case 0x0030CF41: // "help"
            bwrite(wo, "commands: ls, readf, memtest, panic, divide0\n");
            break;
        case 0x3896F7E7: // "memtest"
            memory_test(wo);
            break;
        case 0x06580A77: // "panic"
            panic(wo);
            break;
        case 0X00000D87: // "ls" (for cpio initramfs limine module)
            cpio_list_files(wo, initramfs_mod->address);
            break;
        case 0x0675D990: // "readf" (cpio)
            cpio_read_file(wo, initramfs_mod->address, arg);
            break;
        case 0x63CC12D7: // "divide0"
        {
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wdiv-by-zero"
            volatile int z = 1 / 0;   // exception #0
            #pragma GCC diagnostic pop
            (void)z;
            break;
        }

        
        default:
            if (strlen(cmd) > 0) {
                bwrite(wo, "unknown command: ");
                bwrite(wo, cmd);
                bwrite(wo, "\n");
            }
            break;
    }
}

struct terminal fb_term;

void _start(void) {
    gdt_init();
    idt_init();

    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];
    
    struct framebuffer fb_ctx = {
        .addr = fb->address,
        .width = fb->width,
        .height = fb->height,
        .pitch = fb->pitch,
        .bpp = fb->bpp,
        .r_size = fb->red_mask_size,
        .r_shift = fb->red_mask_shift,
        .g_size = fb->green_mask_size,
        .g_shift = fb->green_mask_shift,
        .b_size = fb->blue_mask_size,
        .b_shift = fb->blue_mask_shift,
    };

    term_init(&fb_term, &fb_ctx);

    // register the framebuffer terminal as a writeable interface
    struct writeout_t term_wo;
    term_wo.len = 0;
    term_wo.buf[0] = '\0';
    term_wo.write = term_write_adapter;
    term_wo.ctx = &fb_term;

    serial_init();

    // register serial as a writeable interface
    struct writeout_t serial_wo;
    serial_wo.len = 0;
    serial_wo.buf[0] = '\0';
    serial_wo.write = serial_write_adapter;
    serial_wo.ctx = NULL;

    // define heap
    heapinit((uint8_t*)0x100000, (uint8_t*)0x200000);
    memory_test(&term_wo);

    int pdcount = ps2_dev_count();
    write_epoch(&term_wo);
    bwrite(&term_wo, "[ \x1b[94mINFO\x1b[0m ] (PS/2) Devices: ");
    uiota(&term_wo, (uint64_t)pdcount);
    bwrite(&term_wo, "\n");

    write_epoch(&term_wo);
    bwrite(&term_wo, "[ \x1b[94mINFO\x1b[0m ] (CPU) Brand: ");
    cpu_brand(&term_wo);
    bwrite(&term_wo, "\n");

    struct limine_module_response *resp = module_request.response;

    if (resp->module_count == 0) {
        bwrite(&term_wo, "[ \x1b[94mINFO\x1b[0m ] (MOD) No Module");
    } else {
        initramfs_mod = resp->modules[0];
    }

    char line[256];

    // shell
    while (1) {
        flush(&term_wo);
        write(&term_wo, "> ", 2);
        readline(&term_wo, line, 256);
        exec(&term_wo, line);
    }

    halt();
}
