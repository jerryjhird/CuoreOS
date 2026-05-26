#pragma once

#include <stdint.h>

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

typedef uint64_t fnv1a_hash_t;

uint64_t hash_fnv1a(const char* key);
