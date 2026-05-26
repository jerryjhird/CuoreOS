#include "fnv1a.h"

#include <stdint.h>

uint64_t hash_fnv1a(const char* key) {
	uint64_t hash = FNV_OFFSET_BASIS;
	while (*key) {
		hash ^= (uint64_t)*key++;
		hash *= FNV_PRIME;
	}
	return hash;
}
