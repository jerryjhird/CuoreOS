#include "elf64.h"
#include "mem/mem.h"
#include "mem/paging.h"
#include "mem/pma.h"
#include <stdbool.h>
#include <stdint.h>
#include "scheduler.h"

elf64_parser_ret elf64_parse(void *file_data, size_t file_size, uint64_t target_pml4_phys, bool user) {
	elf64_parser_ret ret = {0};

	if (!file_data) { return ret; }

	Elf64_Ehdr *ehdr = (Elf64_Ehdr *)file_data;

	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
		ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
		ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
		ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
		ehdr->e_ident[EI_DATA] != ELFDATA2LSB ||
		ehdr->e_machine != EM_X86_64) {
		return ret;
	}

	if (ehdr->e_phoff + (uint64_t)ehdr->e_phnum * ehdr->e_phentsize > file_size) { return ret; }
	if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) { return ret; }

	uint64_t *pml4 = (uint64_t *)(target_pml4_phys + hhdm_offset);
	uintptr_t highest_vaddr = 0;

	for (Elf64_Half i = 0; i < ehdr->e_phnum; i++) {
		Elf64_Phdr *phdr = (Elf64_Phdr *)((uintptr_t)file_data + ehdr->e_phoff + (uint64_t)i * ehdr->e_phentsize);

		if (phdr->p_type != PT_LOAD) { continue; }
		if (phdr->p_offset + phdr->p_filesz > file_size) { return ret; }

		uintptr_t seg_start = phdr->p_vaddr & ~(PAGE_SIZE - 1);
		uintptr_t seg_end = (phdr->p_vaddr + phdr->p_memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

		uint64_t flags = PTE_PRESENT;

		if (user) {
			flags |= PTE_USER;
		}

		if (phdr->p_flags & PF_W) { flags |= PTE_WRITABLE; }
		if (!(phdr->p_flags & PF_X)) { flags |= PTE_NX; }

		if (seg_end > highest_vaddr) {
			highest_vaddr = seg_end;
		}

		// 2mb-aligned huge page portion
		uintptr_t huge_start = (seg_start + PAGE_SIZE_2MB - 1) & ~(uintptr_t)(PAGE_SIZE_2MB - 1);
		uintptr_t huge_end = seg_end & ~(uintptr_t)(PAGE_SIZE_2MB - 1);

		// head: 4kb pages before the 2mb-aligned region
		for (uintptr_t addr = seg_start; addr < huge_start && addr < seg_end; addr += PAGE_SIZE) {
			uintptr_t existing_phys = paging_get_phys(pml4, addr);

			if (!existing_phys) {
				uintptr_t phys = pma_alloc_pages(1);
				if (!phys) { return ret; }

				paging_map_page(pml4, addr, phys, flags);
				memset((void *)(phys + hhdm_offset), 0, PAGE_SIZE);
			} else {
				// combine flags for overlapping segments
				uint64_t *pte = paging_get_pte(pml4, addr, 0);
				uint64_t existing_flags = pte ? (*pte & (PTE_PRESENT | PTE_USER | PTE_WRITABLE | PTE_NX)) : flags;

				// if either segment wants write page must be writeable
				// if either wants execute clear NX
				uint64_t combined_flags = existing_flags | (flags & PTE_WRITABLE);
				if (!(flags & PTE_NX)) { combined_flags &= ~PTE_NX; }

				paging_map_page(pml4, addr, existing_phys, combined_flags);
			}
		}

		// middle: 2mb huge pages
		for (uintptr_t addr = huge_start; addr < huge_end; addr += PAGE_SIZE_2MB) {
			uintptr_t existing_phys = paging_get_phys(pml4, addr);

			if (!existing_phys) {
				uintptr_t phys = pma_alloc_pages_2mb(1);
				if (!phys) { return ret; }

				paging_map_2mb(pml4, addr, phys, flags);
				memset((void *)(phys + hhdm_offset), 0, PAGE_SIZE_2MB);
			} else {
				// combine flags for overlapping segments
				uintptr_t existing_base = existing_phys & ~(uintptr_t)(PAGE_SIZE_2MB - 1);
				uint64_t existing_flags = 0;
				uint64_t *pte = paging_get_pte(pml4, addr, 0);
				if (pte) {
					existing_flags = *pte & (PTE_PRESENT | PTE_USER | PTE_WRITABLE | PTE_NX);
				} else {
					existing_flags = flags;
				}

				// if either segment wants write page must be writeable
				// if either wants execute clear NX
				uint64_t combined_flags = existing_flags | (flags & PTE_WRITABLE);
				if (!(flags & PTE_NX)) { combined_flags &= ~PTE_NX; }

				paging_map_2mb(pml4, addr, existing_base, combined_flags);
			}
		}

		// tail: 4kb pages after the 2mb-aligned region
		for (uintptr_t addr = huge_end; addr < seg_end; addr += PAGE_SIZE) {
			uintptr_t existing_phys = paging_get_phys(pml4, addr);

			if (!existing_phys) {
				uintptr_t phys = pma_alloc_pages(1);
				if (!phys) { return ret; }

				paging_map_page(pml4, addr, phys, flags);
				memset((void *)(phys + hhdm_offset), 0, PAGE_SIZE);
			} else {
				// combine flags for overlapping segments
				uint64_t *pte = paging_get_pte(pml4, addr, 0);
				uint64_t existing_flags = pte ? (*pte & (PTE_PRESENT | PTE_USER | PTE_WRITABLE | PTE_NX)) : flags;

				// if either segment wants write page must be writeable
				// if either wants execute clear NX
				uint64_t combined_flags = existing_flags | (flags & PTE_WRITABLE);
				if (!(flags & PTE_NX)) { combined_flags &= ~PTE_NX; }

				paging_map_page(pml4, addr, existing_phys, combined_flags);
			}
		}

		uintptr_t copy_start = phdr->p_vaddr;
		uintptr_t copy_size = phdr->p_filesz;
		uint8_t *src = (uint8_t *)((uintptr_t)file_data + phdr->p_offset);

		// copy segment data into virtual space
		while (copy_size) {
			uintptr_t page = copy_start & ~(PAGE_SIZE - 1);
			uintptr_t off = copy_start & (PAGE_SIZE - 1);
			uintptr_t chunk = PAGE_SIZE - off;

			if (chunk > copy_size) { chunk = copy_size; }

			uintptr_t phys = paging_get_phys(pml4, page);

			if (!phys) { return ret; }

			memcpy((void *)(phys + hhdm_offset + off), src, chunk);

			copy_start += chunk;
			src += chunk;
			copy_size -= chunk;
		}

		if (phdr->p_memsz > phdr->p_filesz) {
			uintptr_t bss_start = phdr->p_vaddr + phdr->p_filesz;
			size_t bss_size = phdr->p_memsz - phdr->p_filesz;

			while (bss_size) {
				uintptr_t page = bss_start & ~(PAGE_SIZE - 1);
				uintptr_t off = bss_start & (PAGE_SIZE - 1);
				uintptr_t chunk = PAGE_SIZE - off;

				if (chunk > bss_size) { chunk = bss_size; }

				uintptr_t phys = paging_get_phys(pml4, page);

				if (!phys) { return ret; }

				memset((void *)(phys + hhdm_offset + off), 0, chunk);

				bss_start += chunk;
				bss_size -= chunk;
			}
		}
	}

	paging_flush_tlb_all();

	ret.status = 1;
	ret.entry = ehdr->e_entry;
	ret.phnum = ehdr->e_phnum;
	ret.top_vaddr = highest_vaddr;

	return ret;
}

task_t* elf64_alloc(void *file_data, size_t file_size) {
	uintptr_t proc_pml4_phys = pma_alloc_pages(1);
	if (!proc_pml4_phys) return NULL;

	uint64_t *proc_pml4_virt = (uint64_t *)(proc_pml4_phys + hhdm_offset);
	memset(proc_pml4_virt, 0, PAGE_SIZE);

	// clone higher half
	uint64_t *kernel_pml4 = (uint64_t *)(paging_get_pml4() + hhdm_offset);
	for (int i = 256; i < 512; i++) {
		proc_pml4_virt[i] = kernel_pml4[i];
	}

	elf64_parser_ret elf = elf64_parse(file_data, file_size, proc_pml4_phys, false);
	if (!elf.status) { return NULL; }

	task_t* new_task = scheduler_create_task((void(*)(void))elf.entry);
	if (!new_task) return NULL;

	new_task->cr3 = proc_pml4_phys;

	return new_task;
}
