#include "cmm.h"
#include "kstate.h"
#include "panic.h"
#include "logbuf.h"
#include "mem/mem.h"
#include "mem/heap.h"

static struct cmm_region *internal_map = NULL;
static size_t internal_capacity = 0;
static size_t internal_count = 0;
static uint64_t next_id = 1;
static bool is_heap_backed = false;
static uintptr_t max_phys_addr = 0;
static uintptr_t max_ram_addr = 0;

static inline uint64_t cmm_generate_id(void) {
	return next_id++;
}

static const cmm_type_t type_lookup[] = {
    [LIMINE_MEMMAP_USABLE] = CMM_USABLE,
    [LIMINE_MEMMAP_RESERVED] = CMM_RESERVED,
    [LIMINE_MEMMAP_ACPI_RECLAIMABLE] = CMM_ACPI_RECLAIMABLE,
    [LIMINE_MEMMAP_ACPI_NVS] = CMM_ACPI_NVS,
    [LIMINE_MEMMAP_BAD_MEMORY] = CMM_BAD,
    [LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE] = CMM_BOOTLOADER_RECLAIMABLE,
    [LIMINE_MEMMAP_EXECUTABLE_AND_MODULES] = CMM_KERNEL_AND_MODULES,
    [LIMINE_MEMMAP_FRAMEBUFFER] = CMM_FRAMEBUFFER,
    [LIMINE_MEMMAP_RESERVED_MAPPED] = CMM_RESERVED_MAPPED
};

static cmm_type_t translate_type(uint64_t limine_type) {
    if (limine_type >= (sizeof(type_lookup) / sizeof(type_lookup[0]))) {
        return CMM_UNKNOWN;
    }

    return type_lookup[limine_type];
}

static void cmm_ensure_capacity(size_t needed_count) {
    if (needed_count <= internal_capacity) return;

    size_t new_capacity = internal_capacity * 2;
    struct cmm_region *new_map = realloc(internal_map, new_capacity * sizeof(struct cmm_region));

    internal_map = new_map;
    internal_capacity = new_capacity;
    is_heap_backed = true;
}

void cmm_insert_region(uintptr_t base, size_t length, cmm_type_t new_type) {
    if (!is_heap_backed) {
        panic("CMM", "tried to write to memmap before heap migration");
    }

    uintptr_t target_base = base;
    uintptr_t target_end = base + length;

    for (size_t i = 0; i < internal_count && target_base < target_end; i++) {
        struct cmm_region *r = &internal_map[i];
        uintptr_t r_end = r->base + r->length;

        // overlap check
        if (target_base < r_end && target_end > r->base) {
			if (target_base <= r->base && target_end >= r_end) {
				r->type = new_type;
				continue;
			}

            struct cmm_region pieces[3];
            int num_pieces = 0;

			if (target_base > r->base) {
				pieces[num_pieces++] = (struct cmm_region){cmm_generate_id(), r->base, target_base - r->base, r->type};
			}

			uintptr_t chunk_end = (target_end < r_end) ? target_end : r_end;
			pieces[num_pieces++] = (struct cmm_region){cmm_generate_id(), target_base, chunk_end - target_base, new_type};

			if (target_end < r_end) {
				pieces[num_pieces++] = (struct cmm_region){
					.id = cmm_generate_id(), 
					.base = target_end, 
					.length = r_end - target_end, 
					.type = r->type
				};
			}

            cmm_ensure_capacity(internal_count + 2);

            size_t shift_amount = num_pieces - 1;
            memmove(&internal_map[i + num_pieces], &internal_map[i + 1], (internal_count - i - 1) * sizeof(struct cmm_region));

            for (int j = 0; j < num_pieces; j++) {
                internal_map[i + j] = pieces[j];
            }

            internal_count += shift_amount;
            i += (num_pieces - 1);
            target_base = chunk_end;
        }
    }
}

void cmm_init(void) {
	struct limine_memmap_response *resp = memmap_request.response;

	size_t required_slots = resp->entry_count * 2;
	size_t bytes_needed = (required_slots * sizeof(struct cmm_region) + 4095) & ~4095;

	// find a usable region for the CMM memmap
	for (size_t i = 0; i < resp->entry_count; i++) {
		if (resp->entries[i]->type == LIMINE_MEMMAP_USABLE && resp->entries[i]->length >= bytes_needed) {
			internal_map = (struct cmm_region*)((uintptr_t)resp->entries[i]->base + hhdm_offset);
			internal_capacity = required_slots;
			break;
		}
	}

	if (!internal_map) {
		panic("CMM", "failed to carve memory for memmap\n");
	}

	// populate CMM memmap
	for (size_t i = 0; i < resp->entry_count; i++) {
		struct limine_memmap_entry *e = resp->entries[i];
		uintptr_t e_base = (uintptr_t)e->base;
		size_t e_len = (size_t)e->length;

		if (e_base == (uintptr_t)internal_map - hhdm_offset) {
			internal_map[internal_count++] = (struct cmm_region){cmm_generate_id(), e_base, bytes_needed, CMM_KERNEL_EARLY};
			internal_map[internal_count++] = (struct cmm_region){cmm_generate_id(), e_base + bytes_needed, e_len - bytes_needed, CMM_USABLE};
		} else {
			internal_map[internal_count++] = (struct cmm_region){cmm_generate_id(), e_base, e_len, translate_type(e->type)};
		}

		uintptr_t end = internal_map[internal_count-1].base + internal_map[internal_count-1].length;
		if (end > max_phys_addr) max_phys_addr = end;
		if (internal_map[internal_count-1].type == CMM_USABLE && end > max_ram_addr) max_ram_addr = end;
	}

	logbuf_ok("[ CMM  ] CMM initialized successfully\n");
	logbuf_debug("[ CMM  ] Total Regions: %zu\n", internal_count);
	logbuf_debug("[ CMM  ] Top of Physical address space: %p\n", (void*)max_phys_addr);
	logbuf_debug("[ CMM  ] Top of RAM: %p\n", (void*)max_ram_addr);
}

void cmm_migrate_to_heap(void) {
    if (is_heap_backed) return;

    size_t new_capacity = internal_capacity * 2;
    struct cmm_region *new_map = malloc(new_capacity * sizeof(struct cmm_region));

    memcpy(new_map, internal_map, internal_count * sizeof(struct cmm_region));

    internal_map = new_map;
    internal_capacity = new_capacity;
    is_heap_backed = true;

    for (size_t i = 0; i < internal_count; i++) {
        if (internal_map[i].type == CMM_KERNEL_EARLY) {
            internal_map[i].type = CMM_USABLE;
        }
    }

    logbuf_ok("[ CMM  ] Migrated memmap to heap\n");
}

size_t cmm_get_region_count(void) { return internal_count; }
uintptr_t cmm_get_phys_top(void) { return max_phys_addr; }
uintptr_t cmm_get_ram_top(void) { return max_ram_addr; }

const struct cmm_region* cmm_get_region(uint64_t id) {
    for (size_t i = 0; i < internal_count; i++) {
        if (internal_map[i].id == id) return &internal_map[i];
    }
    return NULL;
}

const struct cmm_region* cmm_get_region_by_index(size_t index) {
    if (index >= internal_count) return NULL;
    return &internal_map[index];
}
