#include "kernel.h"

#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "arch/x86_64.h"
#include "memory.h"

#include "drivers/ps2.h"
#include "drivers/serial.h"
#include "fs/cpio_newc.h"
#include "arch/limine.h"

#include "cuoreterm.h"

struct limine_file *initramfs_mod = NULL;

static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

void term_write_adapter(const char *msg, size_t len, void *ctx) {
    cuoreterm_write(ctx, msg, len);
}

void exec(struct writeout_t *wo, const char *cmd) {
    while (*cmd == ' ') cmd++;

    const char *space = cmd;
    while (*space && *space != ' ') space++;

    // copy command into buffer
    char command[32];
    size_t cmd_len = (size_t)(space - cmd);
    if (cmd_len >= sizeof(command)) cmd_len = sizeof(command) - 1;
    memcpy(command, cmd, cmd_len);
    command[cmd_len] = '\0';

    const char *arg = space;
    while (*arg == ' ') arg++;

    switch (hash(command)) {
        case 0x0030CF41: // "help"
            bwrite(wo, "commands: ls, readf, memtest, divide0\n");
            break;
        case 0x3896F7E7: // "memtest"
            memory_test(wo);
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
                printf(wo, "unknown command: %s\n", cmd);
            }
            break;
    }
}

struct terminal fb_term;

void _start(void) {
    gdt_init();
    idt_init();
    serial_init(); // exception handler writes to serial directly

    heapinit((uint8_t*)0x100000, (uint8_t*)0x200000); // init heap

    struct limine_module_response *resp = module_request.response;
    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];

    cuoreterm_init(
         &fb_term,
         (void *)fb->address,
         (uint32_t)fb->width,
         (uint32_t)fb->height,
         (uint32_t)fb->pitch,
         (uint32_t)fb->bpp
    );

    // register the framebuffer terminal as a writeable interface
    struct writeout_t term_wo;
    term_wo.len = 0;
    term_wo.buf[0] = '\0';
    term_wo.write = term_write_adapter;
    term_wo.ctx = &fb_term;

    memory_test(&term_wo);

    printf(&term_wo, "[%u] [ INFO ] (PS/2) Devices: %u\n", get_epoch(), ps2_dev_count());


    char *brand = cpu_brand();
    printf(&term_wo, "[%u] [ INFO ] (CPU) Brand: %s\n", get_epoch(), brand);
    free(brand, 49);

    if (resp->module_count == 0) {
        bwrite(&term_wo, "[ INFO ] (MOD) No Module\n");
    } else {
        initramfs_mod = resp->modules[0];
    }

    // shell
    char line[256];
    while (1) {
        flush(&term_wo);
        write(&term_wo, "> ", 2);
        readline(&term_wo, line, 256);
        exec(&term_wo, line);
    }

    halt();
}
