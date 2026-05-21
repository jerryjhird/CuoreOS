#include "symtable.h"
#include "panic.h"
#include "mem/mem.h"

static struct SymTable k_symtable_instance = {0};
struct SymTable* k_symbols = &k_symtable_instance;

static size_t k_string_pool_max = 0;

void symtable_init(const void* table_blob, size_t total_size) {
	if (!table_blob || total_size < sizeof(uint64_t)) {
		panic("SYMTABLE", "invalid arguments");
	}

	uint64_t count = *(const uint64_t*)table_blob;
	size_t expected_entries_size = count * sizeof(struct SymTableEntry);
	size_t header_plus_entries = sizeof(uint64_t) + expected_entries_size;

	if (header_plus_entries > total_size) {
		panic("SYMTABLE", "malformed data");
	}

	k_symbols->entries = (const struct SymTableEntry*)((const char*)table_blob + sizeof(uint64_t));
	k_symbols->string_blob = (const char*)table_blob + header_plus_entries;
	k_symbols->entry_count = count;

	k_string_pool_max = total_size - header_plus_entries;
}

const char* symtable_lookup(size_t index, uint64_t* out_addr) {
	if (!k_symbols || !k_symbols->entries || index >= k_symbols->entry_count) {
		return NULL;
	}

	const struct SymTableEntry* entry = &k_symbols->entries[index];
	if (out_addr) {
		*out_addr = entry->address;
	}

	if (entry->string_offset >= k_string_pool_max) {
		return NULL;
	}

	return &k_symbols->string_blob[entry->string_offset];
}

uint64_t symtable_reverse_lookup(const char* name) {
	if (!name || !k_symbols || k_symbols->entry_count == 0) {
		return 0;
	}

	size_t low = 0;
	size_t high = k_symbols->entry_count - 1;

	// binary search
	while (low <= high) {
		size_t mid = low + (high - low) / 2;

		uint64_t addr = 0;
		const char* sym_name = symtable_lookup(mid, &addr);

		if (!sym_name) {
			return 0;
		}

		int res = strcmp(name, sym_name);

		if (res == 0) {
			return addr;
		} else if (res < 0) {
			if (mid == 0) break;
			high = mid - 1;
		} else {
			low = mid + 1;
		}
	}

	return 0; // not found
}
