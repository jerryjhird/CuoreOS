#include "bitmap.h"

#include "mem/mem.h"

size_t bitmap_find_next_free(uint8_t *map, size_t bit_offset, size_t total_bits) {
	for (size_t i = bit_offset; i < total_bits; i++) {
		if (!BITMAP_TEST(map, i)) {
			return i;
		}
	}
	return (size_t)-1;
}
