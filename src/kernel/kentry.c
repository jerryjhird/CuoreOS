/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "limine.h"
#include "kernel.h"

#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "x86.h"
#include "memory.h"

#include "serial.h"
#include "cpir.h"

#define CUORETERM_IMPL
#include "Cuoreterm.h"

#include "kfont.h"

#include "bmp_render.h"
#include "liminereq.h"
#include "GDT.h"
#include "PMA.h"

struct limine_file *initramfs_mod = NULL;
uint32_t (*hash)(const char *s);
struct terminal fb_term;

#define KHEAP_PAGES 512  // 512 * 4096 = 2 MiB
#define KSTACK_PAGES 4

uint8_t *KHEAP_START = NULL;
uint8_t *KHEAP_END   = NULL;

struct writeout_t *stdio;
struct writeout_t serial_wo;
struct writeout_t term_wo;

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

volatile struct limine_efi_system_table_request efi_req = {
    .id = LIMINE_EFI_SYSTEM_TABLE_REQUEST_ID,
    .revision = 0
};

// terminal functions
void term_write_adapter(const char *msg, size_t len, void *ctx) {
    cuoreterm_write(ctx, msg, len);
}

void serial_write_adapter(const char *msg, size_t len, void *ctx) {
    (void)ctx;
    serial_write(msg, len);
}

// kernel panic
void panic(void) {
    void *bmp_data = cpir_read_file(initramfs_mod->address, "panic.bmp", NULL);

    if (initramfs_mod != NULL) {
        bmp_render(bmp_data, fb_req.response->framebuffers[0], 100, 100);
        free(bmp_data); // freeing dosent matter here but fuck it
    }

    halt();
}

// shell
void exec(const char *cmd) {
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
            bwrite(stdio, "commands: setout, ls, readf, memtest, pma_state, clear, cls, div0, dumpreg, panic\n");
            break;

        case 0x5AE561ED: // "div0" (divide by zero)
        {
            int a = 42;
            int b = 0;
            int c = a / b;
            (void)c;
            break;
        }

        case 0xB160F2BE: // "panic"
            nl_serial_write("kernel panic!\npanic executed in shell\n");
            panic();
            break;

        case 0x214CE06B: // "dumpreg" dump registers
        {
            uint64_t regs[15];
            __asm__ volatile(
                "mov %%rax, %0\n\t"
                "mov %%rbx, %1\n\t"
                "mov %%rcx, %2\n\t"
                "mov %%rdx, %3\n\t"
                "mov %%rsi, %4\n\t"
                "mov %%rdi, %5\n\t"
                "mov %%rbp, %6\n\t"
                "mov %%r8,  %7\n\t"
                "mov %%r9,  %8\n\t"
                "mov %%r10, %9\n\t"
                "mov %%r11, %10\n\t"
                "mov %%r12, %11\n\t"
                "mov %%r13, %12\n\t"
                "mov %%r14, %13\n\t"
                "mov %%r15, %14\n\t"
            : "=m"(regs[0]), "=m"(regs[1]), "=m"(regs[2]), "=m"(regs[3]),
              "=m"(regs[4]), "=m"(regs[5]), "=m"(regs[6]), "=m"(regs[7]),
              "=m"(regs[8]), "=m"(regs[9]), "=m"(regs[10]), "=m"(regs[11]),
              "=m"(regs[12]), "=m"(regs[13]), "=m"(regs[14])
            :
            : 
        );
            regtserial(regs);
            break;
        }

        case 0xED1ABDF6: // "memtest"
            memory_test(stdio);
            break;

        case 0x31CA209B: // "ls" (for cpir initramfs limine module)
        {
            char *cpir_filelist = cpir_list_files(initramfs_mod->address);
            bwrite(stdio, cpir_filelist);
            free(cpir_filelist);
            break;
        }
        case 0x030C68B6: // "readf [filename]" (cpir)
        {
            size_t size;
            void *file_data = cpir_read_file(initramfs_mod->address, arg, &size);

            if (!file_data) {
                printf(stdio, "%s not found\n", arg);
                break; // exit and dont free because nothing gets allocated at this point in cpir_read_file i thunk
            }
            
            lbwrite(stdio, file_data, size);
            free(file_data);
            break;
        }

        case 0x5BDBBC9A: // "setout [<term|serial>]" changes exec output
        {
            uint32_t arg_hash = hash(arg);
            if (arg_hash == 0xB589BDD3) { // "term"
                stdio = &term_wo;
                bwrite(stdio, "now outputting over terminal\n");
            } else if (arg_hash == 0xA5E75702) { // "serial"
                stdio = &serial_wo;
                bwrite(stdio, "now outputting over serial\n");
            } else {
                bwrite(stdio, "usage: setout [term|serial]\n");
            }
            break;
        }

        case 0xFDC59B25: // "cls" Windows style clear command
            cuoreterm_clear(&fb_term);
            break;

        case 0x8B0CA256: // "clear" posix style clear
            cuoreterm_clear(&fb_term);
            break;

        case 0x57C5E155: // "pma_state" (gets the current state of the physical memory allocator)
            printf(stdio, "(PMA) total pages: %u\n(PMA) used pages: %u\n(PMA) free pages: %u\n", pma_total_pages, pma_used_pages, pma_total_pages - pma_used_pages);
            break;
    }
}

void kernel_main(void) {
    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];

    cuoreterm_init(
         &fb_term,
         (void *)fb->address,
         (uint32_t)fb->width,
         (uint32_t)fb->height,
         (uint32_t)fb->pitch,
         (uint32_t)fb->bpp,
         fb->red_mask_shift,
         fb->green_mask_shift,
         fb->blue_mask_shift,
         fb->red_mask_size,
         fb->green_mask_size,
         fb->blue_mask_size,
         2,
         iso10_f14_psf,
         8,
         14
    );

    // register serial as a writeable interface
    serial_wo.len = 0;
    serial_wo.buf[0] = '\0';
    serial_wo.write = serial_write_adapter;
    serial_wo.ctx = NULL;

    // register the framebuffer terminal as a writeable interface
    term_wo.len = 0;
    term_wo.buf[0] = '\0';
    term_wo.write = term_write_adapter;
    term_wo.ctx = &fb_term;

    stdio = &term_wo;

    printf(&term_wo, TIME_LOG_STR" [%u]\n", get_epoch());

    void *heap_phys = (void *)pma_alloc_pages(KHEAP_PAGES); // ask pma
    
    KHEAP_START = (uint8_t *)heap_phys + hhdm_req.response->offset;;
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
        bwrite(&term_wo, WARN_LOG_STR " SSE4.2 Not Supported\n");
        hash = crc32c_swhash; // software based hash
    }

    hash_test(&term_wo);

    printf(&term_wo, INFO_LOG_STR " (CPU) Brand: %s\n", brand);
    free(brand);

    // shell   
    char line[256];
    while (1) {
        flush(&term_wo);
        flush(&serial_wo);
        write(&term_wo, "> ", 2);
        readline(&term_wo, line, 256);
        exec(line);
    }
}

void _start(void) {
    serial_init();
    gdt_init();
    idt_init();

    // have initramfs module ready for pma_init in case it throws a panic
    struct limine_module_response *resp = module_request.response;
    if (resp == NULL || resp->module_count == 0 || resp->modules == NULL) {
        initramfs_mod = NULL;
    } else {
        if (resp->modules[0] != NULL) {
            initramfs_mod = resp->modules[0];
        } else {
            initramfs_mod = NULL;
        }
    }

    if (initramfs_mod == NULL) {
        nl_serial_write("kernel panic!\nNo initramfs module found\n");
        panic();
    }

    pma_init();

    // setup stack
    uintptr_t phys = pma_alloc_pages(KSTACK_PAGES);
    uintptr_t top = phys + hhdm_req.response->offset + (KSTACK_PAGES * PMA_PAGE_SIZE);
    top &= ~(uintptr_t)0xF;

    // switch to stack and jmp to kernel_main 
    swstack_jmp((void *)top, kernel_main);
}
