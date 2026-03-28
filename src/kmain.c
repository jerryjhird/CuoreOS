#include <stdint.h>
#include "UART16550.h"
#include "logbuf.h"
#include "mem/heap.h"
#include "limine.h"
#include "mem/pma.h"
#include "mem/mem.h"
#include "kstate.h"
#include "devices.h"
#include "fb_flanterm.h"
#include "ansi_helpers.h"

volatile struct limine_module_request mod_req = { 
    .id = LIMINE_MODULE_REQUEST_ID, 
    .revision = 0 
};

volatile struct limine_memmap_request memmap_request = { 
    .id = LIMINE_MEMMAP_REQUEST_ID, 
    .revision = 0 
};

volatile struct limine_hhdm_request hhdm_req = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

kernel_config_t global_kernel_config = {0};
uint64_t hhdm_offset;

#define FOUND_CONFIG      (1ULL << 0)
#define FOUND_INITRAMFS   (1ULL << 1)

void panic(const char* header_msg, const char* msg) {
    for (size_t i = 0; i < output_devices_c; i++) {
        output_dev_t* dev = output_devices[i];

        if (DEV_CAP_HAS(dev, CAP_ON_ERROR)) {
            const char *style = GET_ANSI_STYLE(dev, ANSI_4B_PANIC, ANSI_8B_PANIC, ANSI_24B_PANIC);
            
            if (style) dev->write(style);
            
            dev->write("\n*** KERNEL PANIC: ");
            dev->write(header_msg);
            dev->write(" ***\n");
            dev->write(msg);
            dev->write("\n\n");

            if (style) dev->write(ANSI_RESET);
        }
    }
    for(;;) { __asm__ ("hlt"); }
}

uint64_t Lmodules_init(struct limine_module_response *mod_res) {
    uint64_t found_mask = 0;

    if (mod_req.response == NULL) {
        return 0;
    }

    for (uint64_t i = 0; i < mod_res->module_count; i++) {
        struct limine_file *file = mod_res->modules[i];
        
        uint64_t raw_magic = *(uint64_t *)(file->address); 

        switch (raw_magic) {
            case CONFIG_MAGIC:
                memcpy(&global_kernel_config, (kernel_config_t *)file->address, sizeof(kernel_config_t));
                found_mask |= FOUND_CONFIG;
                break;

            case INITRAMFS_MAGIC: // to be implemented
                found_mask |= FOUND_INITRAMFS;
                break;
                
            default:
                break;
        }
    }
    return found_mask;
}

output_dev_t* output_devices[MAX_DEVICES];
size_t output_devices_c = 0;

void kernel_main(void) {
    uint64_t modules_found = Lmodules_init(mod_req.response);

    if (!(modules_found & FOUND_CONFIG)) {
        logbuf_write("[WARN] config not found. defaulting to hardcoded config\n");
        global_kernel_config.magic = HARDCODED_CONFIG_MAGIC;
        global_kernel_config.epoch = 1774460058;
        global_kernel_config.debug = 1;
        global_kernel_config.flanterm_is_debug_interface = 0;
        global_kernel_config.uart16550_is_debug_interface = 1;
    }

    uart16550_postinit(); // now the config is loaded we call this so uart can see uart16550_is_debug_interface

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
    _c_flanterm_init(fb);
    logbuf_write("hello world!");
    heap_dump_stats();
    logbuf_flush(&flanterm_dev);
    logbuf_flush(&uart16550_dev);
}

// 256 pages = 1MB
#define HEAP_PAGES 256
#define HEAP_SIZE (HEAP_PAGES << 12)

void _kstartc(void) {
    hhdm_offset = hhdm_req.response->offset;

    pma_init();
    uintptr_t phys_addr = pma_alloc_pages(HEAP_PAGES); // 256 pages = 1MB
    void* virt_addr = (void*)(phys_addr + hhdm_req.response->offset);
    heap_init(virt_addr, HEAP_SIZE);
    kernel_main();
}