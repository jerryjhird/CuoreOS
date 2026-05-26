#pragma once

#include <stdint.h>
#include <stddef.h>

#define BITMAP_SET(map, bit) ((map)[(bit) / 8] |=  (1u << ((bit) % 8)))
#define BITMAP_CLEAR(map, bit) ((map)[(bit) / 8] &= ~(1u << ((bit) % 8)))
#define BITMAP_TEST(map, bit) (((map)[(bit) / 8] >> ((bit) % 8)) & 1u)

size_t bitmap_find_next_free(uint8_t *map, size_t bit_offset, size_t total_bits);
