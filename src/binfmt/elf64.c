#include "elf64.h"
#include "../fs/ramfs.h"
#include "../mem/mem.h"
#include "../mem/paging.h"
#include "../mem/pma.h"

#define PF_X 1
#define PF_W 2
#define PF_R 4

uint64_t elf64_parse(const char *name) {
    ramfs_handle_t ramfs_handle;
    ramfs_file_t file = ramfs_get_file(&ramfs_handle, name);
    if (!file.data) return 0;
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)file.data;
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
        ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr->e_ident[EI_DATA] != ELFDATA2LSB ||
        ehdr->e_machine != EM_X86_64) return 0;
    if (ehdr->e_phoff + (uint64_t)ehdr->e_phnum * ehdr->e_phentsize > file.size) return 0;
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) return 0;
    uint64_t pml4_phys = vmm_get_pml4();
    uint64_t *pml4 = (uint64_t *)(pml4_phys + hhdm_offset);
    for (Elf64_Half i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = (Elf64_Phdr *)((uintptr_t)file.data + ehdr->e_phoff + (uint64_t)i * ehdr->e_phentsize);
        if (phdr->p_type != PT_LOAD) continue;
        if (phdr->p_offset + phdr->p_filesz > file.size) return 0;
        uintptr_t seg_start = phdr->p_vaddr & ~(PAGE_SIZE - 1);
        uintptr_t seg_end = (phdr->p_vaddr + phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t flags = PTE_PRESENT | PTE_USER;
        if (phdr->p_flags & PF_W) flags |= PTE_WRITABLE;
        if (!(phdr->p_flags & PF_X)) flags |= PTE_NX;
        for (uintptr_t addr = seg_start; addr < seg_end; addr += PAGE_SIZE) {
            uintptr_t phys = pma_alloc_pages(1);
            if (!phys) return 0;
            vmm_map_page(pml4, addr, phys, flags);
            memset((void *)(phys + hhdm_offset), 0, PAGE_SIZE);
        }
        uintptr_t copy_start = phdr->p_vaddr;
        uintptr_t copy_size = phdr->p_filesz;
        uint8_t *src = (uint8_t *)((uintptr_t)file.data + phdr->p_offset);
        while (copy_size) {
            uintptr_t page = copy_start & ~(PAGE_SIZE - 1);
            uintptr_t off = copy_start & (PAGE_SIZE - 1);
            uintptr_t chunk = PAGE_SIZE - off;
            if (chunk > copy_size) chunk = copy_size;
            uintptr_t phys = vmm_get_phys(pml4, page);
            if (!phys) return 0;
            memcpy((void *)(phys + hhdm_offset + off), src, chunk);
            copy_start += chunk;
            src += chunk;
            copy_size -= chunk;
        }
    }
    return ehdr->e_entry;
}
