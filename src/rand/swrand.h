#pragma once

#include <stdint.h>

// LCG constants
#define LCG_MULT 6364136223846793005ULL
#define LCG_INC  1442695040888963407ULL

uint64_t swrand64(uint64_t *seed);
uint32_t swrand32(uint64_t *seed);
uint16_t swrand16(uint64_t *seed);
uint8_t swrand8(uint64_t *seed);
uint32_t randint(uint64_t *seed, uint32_t start, uint32_t end);
