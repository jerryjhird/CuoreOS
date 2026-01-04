// header file reserved for kernel specific functions
// that do not fall into the categories of the other
// header files

#ifndef KERNEL_H
#define KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"

// kernel tests
void memory_test(struct writeout_t *wo);

// physical memory allocator (PMA)
void pma_init(struct limine_memmap_response *mm);
void hash_test(struct writeout_t *wo, uint32_t (**hash)(const char *));

// pma allocation
uintptr_t pma_alloc_page(void);
uintptr_t pma_alloc_pages(size_t count);

// pma free
void pma_free_page(uintptr_t phys);
void pma_free_pages(uintptr_t phys, size_t count);

// pma info getters
size_t pma_page_size(void);
size_t pma_total(void);
size_t pma_used(void);
size_t pma_free(void);

#ifdef __cplusplus
}
#endif

#endif // KERNEL_H