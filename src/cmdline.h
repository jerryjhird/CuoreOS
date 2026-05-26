#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "math/fnv1a.h"

#ifndef CMDLINE_MAX_ARGS // allow override from cflags
#define CMDLINE_MAX_ARGS 128
#endif

typedef struct {
	char key[32];
	char value[64];
	fnv1a_hash_t hash;
	bool found;
} arg_entry_t;

void cmdline_init(void);
const char* cmdline_get_string(const char* key);
uint64_t cmdline_get_uint(const char* key, uint64_t default_val);
bool cmdline_has_key(const char* key);
