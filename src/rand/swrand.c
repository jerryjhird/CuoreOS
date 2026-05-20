#include "swrand.h"

static inline uint32_t step_lcg_32(uint64_t *seed) {
	*seed = (*seed) * LCG_MULT + LCG_INC;
	uint32_t xorshifted = (uint32_t)((((*seed) >> 18u) ^ (*seed)) >> 27u);
	uint32_t rot = (uint32_t)((*seed) >> 59u);
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

uint64_t swrand64(uint64_t *seed) { return ((uint64_t)step_lcg_32(seed) << 32) | step_lcg_32(seed); }
uint32_t swrand32(uint64_t *seed) { return step_lcg_32(seed); }
uint16_t swrand16(uint64_t *seed) { return (uint16_t)(step_lcg_32(seed) >> 16); }
uint8_t swrand8(uint64_t *seed)  { return (uint8_t)(step_lcg_32(seed) >> 24); }

uint32_t randint(uint64_t *seed, uint32_t start, uint32_t end) {
	if (start > end) {
		uint32_t temp = start;
		start = end;
		end = temp;
	}

	uint64_t range = (uint64_t)(end - start) + 1;
	uint32_t x = swrand32(seed);

	uint64_t m = (uint64_t)x * range;
	uint32_t l = (uint32_t)m;
	if (l < range) {
		uint32_t t = -range;
		if (t >= range) {
			t -= range;
			if (t >= range) t %= range;
		}
		while (l < t) {
			x = swrand32(seed);
			m = (uint64_t)x * range;
			l = (uint32_t)m;
		}
	}

	return (uint32_t)(start + (m >> 32));
}
