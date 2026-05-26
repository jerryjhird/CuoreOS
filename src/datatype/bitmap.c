#include "bitmap.h"

#include "mem/mem.h"

size_t bitmap_find_next_free(uint8_t *map, size_t bit_offset, size_t total_bits) {
	size_t num_words = (total_bits + 63) / 64;

	for (size_t i = (bit_offset / 64); i < num_words; i++) {
		uint64_t word;
		memcpy(&word, &map[i * 8], sizeof(uint64_t));

		if (word == ~0ULL) continue;

		for (int bit = 0; bit < 64; bit++) {
			size_t absolute_bit = (i * 64) + bit;
			if (absolute_bit >= total_bits) break;
			if (absolute_bit < bit_offset) continue;

			if (!BITMAP_TEST(map, absolute_bit)) {
				return absolute_bit;
			}
		}
	}
	return (size_t)-1;
}
