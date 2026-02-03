/*
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at 
https://mozilla.org/MPL/2.0/.
*/

#include "kernel.h"
#include "stddef.h"
#include "serial.h"
#include "stdio.h"
#include "string.h"
#include "x86.h"
#include "cpir.h"
#include "memory.h"
#include "klimine.h"
#include "PMA.h"
#include "paging.h"

#include <flanterm.h>
#include <flanterm_backends/fb.h>
#include "limine.h"

void gdt_init(uint64_t kernel_stack_top);
void idt_init(void);

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

void kpanic(const char *msg) {
    serial_print(msg);
    while (1) {}
}

struct limine_file *initramfs_mod = NULL; // pointer to initramfs
struct flanterm_context *ft_ctx = NULL;

static inline void *mflanterm_write(const char *msg, size_t len) {
    flanterm_write(ft_ctx, msg, len);
    return NULL;
}

#define KHEAP_PAGES 512 // 512 * 4096 = 2 MiB
#define KSTACK_PAGES 4 // kernel stack
#define USTACK_PAGES 4 // userspace stack

uint8_t *KHEAP_START = NULL;
uint8_t *KHEAP_END   = NULL;
logbuf_t klog = { .len = 0 };

void kernel_main(void) {
    void *heap_phys = (void *)pma_alloc_pages(KHEAP_PAGES); // ask pma
    KHEAP_START = (uint8_t *)heap_phys + hhdm_req.response->offset;;
    KHEAP_END   = KHEAP_START + KHEAP_PAGES * 4096;
    heapinit(KHEAP_START, KHEAP_END);

    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];

    ft_ctx = flanterm_fb_init(
        NULL,
        NULL,

        fb->address,
        fb->width,
        fb->height,
        fb->pitch,

        fb->red_mask_size,
        fb->red_mask_shift,

        fb->green_mask_size,
        fb->green_mask_shift,

        fb->blue_mask_size,
        fb->blue_mask_shift,

        NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL,
        0, 0, 1,
        0, 0, 0, 0
    );

    memory_test();

    logbuf_flush(&klog, mflanterm_write);

    // TODO: FIX, everything here should work besides enter_userspace(...) and maybe the gdt_init needs adjusting
    // size_t flat_size;
    // void *flat_data = cpir_read_file(initramfs_mod->address, "init", &flat_size);
    // if (!flat_data) {kpanic("Could not find userspace init binary in archive");}

    // size_t code_pages = (flat_size + PMA_PAGE_SIZE - 1) / PMA_PAGE_SIZE;
    // uintptr_t user_code_phys  = pma_alloc_pages(code_pages);
    // uintptr_t user_stack_phys = pma_alloc_pages(USTACK_PAGES);
    
    // uint64_t user_code_vaddr  = 0x400000; // map user code at canonical lower-half address
    // uint64_t user_stack_vaddr = 0x800000; // map user stack at some lower-half address
    // uint64_t user_stack_top   = user_stack_vaddr + USTACK_PAGES * PMA_PAGE_SIZE;

    // user_stack_top &= ~0xFULL;
    // memcpy((void*)(user_code_phys + hhdm_req.response->offset), flat_data, flat_size);

    // uint64_t* pml4 = (uint64_t*)(read_cr3() + hhdm_req.response->offset);

    // // user-mode, executable
    // map_pages(code_pages, pml4, user_code_vaddr, user_code_phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    // // user-mode, read/write, NX
    // map_pages(USTACK_PAGES, pml4, user_stack_vaddr, user_stack_phys, PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_NX);

    // enter_userspace(user_code_vaddr, user_stack_top);
    while (1) {}
}

void _kstart(void) {
    serial_init();

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
        kpanic("kernel panic!\nNo initramfs module found\n");
    }

    pma_init();

    // setup stack
    uintptr_t phys = pma_alloc_pages(KSTACK_PAGES);
    uintptr_t top = phys + hhdm_req.response->offset + (KSTACK_PAGES * PMA_PAGE_SIZE);
    top &= ~(uintptr_t)0xF;

    gdt_init(top);
    idt_init();

    // switch to stack and jmp to kernel_main 
    swstack_jmp((void *)top, kernel_main);
}
