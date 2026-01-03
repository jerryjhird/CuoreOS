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
uint32_t (*hash)(const char *s);

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
        case 0xB5E3B64C: // "help"
            bwrite(wo, "commands: ls, readf, memtest\n");
            break;
        case 0xED1ABDF6: // "memtest"
            memory_test(wo);
            break;
        case 0x31CA209B: // "ls" (for cpio initramfs limine module)
            cpio_list_files(wo, initramfs_mod->address);
            break;
        case 0x030C68B6: // "readf" (cpio)
            cpio_read_file(wo, initramfs_mod->address, arg);
            break;
        default:
            if (strlen(cmd) > 0) {
                printf(wo, "unknown command: %s\n", cmd);
            }
            break;
    }
}

struct terminal fb_term;

void _start(void) {
    
    serial_init();
    gdt_init();
    idt_init();

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
    const char fucky_cpu[] = "QEMU Virtual CPU version 2.5+"; // cpu tends to be fucky with reporting SSE4.2 avaliability in my own testing (this is why the makefile dosent use it by default but the following code makes it usable)
    
    int is_fucky = 1;
    for (int i = 0; i < 27; i++) {
        if (brand[i] != fucky_cpu[i]) {
            is_fucky = 0;
            break;
        }
    }

    if (sse_init() >= 0 && !is_fucky) {
        hash = crc32c_hwhash; // hardware based hash
        hash_test(&term_wo);
    } else {
        printf(&term_wo, "[%u] [ WARN ] SSE4.2 Not Supported \n", get_epoch());
        hash = crc32c_swhash; // software based hash
    }

    printf(&term_wo, "[%u] [ INFO ] (CPU) Brand: %s\n", get_epoch(), brand);
    free(brand, 49);

    if (resp->module_count == 0) {
        printf(&term_wo, "[%u] [ WARN ] (MOD) No Limine Modules Detected\n", get_epoch());
        halt();
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
