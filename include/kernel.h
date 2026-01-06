// header file reserved for kernel specific functions
// that do not fall into the categories of the other
// header files

#ifndef KERNEL_H
#define KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"

// global hashing function
extern uint32_t (*hash)(const char *s);

// limine requests
extern volatile struct limine_framebuffer_request fb_req;
extern volatile struct limine_module_request      module_request;
extern volatile struct limine_memmap_request      memmap_req;
extern volatile struct limine_hhdm_request        hhdm_req;

// kernel tests
void memory_test(struct writeout_t *wo);
void hash_test(struct writeout_t *wo, uint32_t (**hash)(const char *));

// physical memory allocator (PMA)
#define PMA_PAGE_SIZE 4096
extern size_t pma_total_pages;
extern size_t pma_used_pages;

void pma_init(struct limine_memmap_response *mm);

// pma allocation
uintptr_t pma_alloc_page(void);
uintptr_t pma_alloc_pages(size_t count);

// pma free
void pma_free_page(uintptr_t phys);
void pma_free_pages(uintptr_t phys, size_t count);

#ifdef __cplusplus
}
#endif

#endif // KERNEL_H