#include "arch/limine.h"
#include "kernel.h"

#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "arch/x86_64.h"
#include "memory.h"

#include "drivers/ps2.h"
#include "drivers/serial.h"
#include "fs/cpio_newc.h"
#include "cuoreterm.h"

struct limine_file *initramfs_mod = NULL;
uint32_t (*hash)(const char *s);

#define KHEAP_PAGES 512  // 512 * 4096 = 2 MiB
#define KSTACK_PAGES 4

uint8_t *KHEAP_START = NULL;
uint8_t *KHEAP_END   = NULL;

// limine requests

volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

volatile struct limine_memmap_request memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

void term_write_adapter(const char *msg, size_t len, void *ctx) {
    cuoreterm_write(ctx, msg, len);
}

void serial_write_adapter(const char *msg, size_t len, void *ctx) {
    (void)ctx;
    serial_write(msg, len);
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
            lbwrite(wo, "commands: ls, readf, memtest, pma_state\n", 40);
            break;

        case 0xED1ABDF6: // "memtest"
            memory_test(wo);
            break;

        case 0x31CA209B: // "ls" (for cpio initramfs limine module)
            char *cpio_filelist = cpio_list_files(initramfs_mod->address);
            bwrite(wo, cpio_filelist);
            free(cpio_filelist, strlen(cpio_filelist) + 1);
            break;

        case 0x030C68B6: // "readf [filename]" (cpio)
            size_t size;
            void *file_data = cpio_read_file(initramfs_mod->address, arg, &size);

            if (!file_data) {
                printf(wo, "%s not found\n", arg);
                break; // exit and dont free because nothing gets allocated at this point in cpio_read_file i thunk
            }
            
            lbwrite(wo, file_data, size);
            free(file_data, size);
            break;

        case 0x57C5E155: // "pma_state" (gets the current state of the physical memory allocator)
            printf(wo, "(PMA) total pages: %u\n(PMA) used pages: %u\n(PMA) free pages: %u\n", pma_total_pages, pma_used_pages, pma_total_pages - pma_used_pages);
            break;
    }
}

struct terminal fb_term;

void kernel_main(void) {
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

    printf(&term_wo, "[ TIME ] [%u]\n", get_epoch());

    void *heap_phys = (void *)pma_alloc_pages(KHEAP_PAGES); // ask pma
    uint64_t hhdm_offset = hhdm_req.response->offset;
    
    KHEAP_START = (uint8_t *)heap_phys + hhdm_offset;
    KHEAP_END   = KHEAP_START + KHEAP_PAGES * 4096;

    heapinit(KHEAP_START, KHEAP_END);
    memory_test(&term_wo);

    // information prints and SSE + fucky cpu detection
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
    } else {
        lbwrite(&term_wo, "[ WARN ] SSE4.2 Not Supported\n", 30);
        hash = crc32c_swhash; // software based hash
    }

    hash_test(&term_wo, &hash);

    printf(&term_wo, "[ INFO ] (CPU) Brand: %s\n", brand);
    free(brand, 49);

    if (resp->module_count == 0) {
        lbwrite(&term_wo, "[ WARN ] (MOD) No Limine Modules Detected\n", 42);
    } else {
        initramfs_mod = resp->modules[0];
    }

    printf(&term_wo, "[ INFO ] (PMA) page size: %u bytes\n[ INFO ] (PMA) total pages: %u\n[ INFO ] (PMA) used pages: %u\n[ INFO ] (PMA) free pages: %u\n", PMA_PAGE_SIZE, pma_total_pages, pma_used_pages, pma_total_pages - pma_used_pages);

    // shell
    char line[256];
    while (1) {
        flush(&term_wo);
        write(&term_wo, "> ", 2);
        readline(&term_wo, line, 256);
        exec(&term_wo, line);
    }
}

void _start(void) {
    serial_init();
    gdt_init();
    idt_init();

    pma_init();

    // setup stack
    uintptr_t phys = pma_alloc_pages(KSTACK_PAGES);
    uintptr_t top = phys + hhdm_req.response->offset + (KSTACK_PAGES * PMA_PAGE_SIZE);
    top &= ~(uintptr_t)0xF;

    // switch to stack and jmp to kernel_main 
    swstack_jmp((void *)top, kernel_main);
}
