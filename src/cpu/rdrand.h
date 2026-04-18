#pragma once

#include <stdint.h>
#include <stdbool.h>

// true == supported | false == unsupported
bool rdrand_supported(void);
bool rdseed_supported(void);

// true == success | false == failed
bool rdrand64(uint64_t *output);
bool rdseed64(uint64_t *output);
