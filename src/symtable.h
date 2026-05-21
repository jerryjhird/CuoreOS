#pragma once

#include <stdint.h>
#include <stddef.h>

struct __attribute__((packed)) SymTableEntry {
	uint64_t address;
	uint32_t string_offset;
};

struct SymTable {
	const struct SymTableEntry* entries;
	const char* string_blob;
	size_t entry_count;
};

extern struct SymTable* k_symbols;

void symtable_init(const void* table_blob, size_t total_size);
const char* symtable_lookup(size_t index, uint64_t* out_addr);
uint64_t symtable_reverse_lookup(const char* name);
