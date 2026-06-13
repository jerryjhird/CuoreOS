#include "cmdline.h"
#include "logbuf.h"
#include "panic.h"
#include "math/fnv1a.h"
#include <stddef.h>
#include "mem/mem.h"
#include "stdio.h"

static arg_entry_t arg_registry[CMDLINE_MAX_ARGS];
static int registry_count = 0;

static void cmdline_parse(const char* cmdline_str) {
	const char* cmd = cmdline_str;

	while (*cmd) {
		while (*cmd == ' ') cmd++;
		if (!*cmd) break;

		if (registry_count >= CMDLINE_MAX_ARGS) {
			panic("CMDLINE", "argument registry overflow! number of arguments exceeds CMDLINE_MAX_ARGS");
		}

		arg_entry_t* entry = &arg_registry[registry_count++];

		int k = 0;
		while (*cmd && *cmd != '=' && *cmd != ' ' && k < 31) entry->key[k++] = *cmd++;
		entry->key[k] = '\0';

		// precompute hash for fast lookup
		entry->hash = hash_fnv1a(entry->key);
		entry->found = true;

		if (*cmd == '=') {
			cmd++;
			int v = 0;
			while (*cmd && *cmd != ' ' && v < 63) entry->value[v++] = *cmd++;
			entry->value[v] = '\0';
		} else {
			entry->value[0] = '\0';
		}
	}
}

static int cmdline_find(const char* key) {
	fnv1a_hash_t target_hash = hash_fnv1a(key);

	for (int i = 0; i < registry_count; i++) {
		if (arg_registry[i].hash == target_hash) {
			if (strcmp(arg_registry[i].key, key) == 0) {
				return i;
			}
		}
	}
	return -1;
}

void cmdline_init(const char* cmdline_str) {
	if (cmdline_str == NULL || cmdline_str[0] == '\0') {
		logbuf_info("[ ARGS ] no command line arguments provided\n");
		return;
	}

	logbuf_info("[ ARGS ] cmdline arguments: \"%s\"\n", cmdline_str);
	cmdline_parse(cmdline_str);
}

const char* cmdline_get_string(const char* key) {
	int idx = cmdline_find(key);
	return (idx != -1) ? arg_registry[idx].value : NULL;
}

uint64_t cmdline_get_uint(const char* key, uint64_t default_val) {
	int idx = cmdline_find(key);
	if (idx != -1 && arg_registry[idx].value[0] != '\0') {
		return strtoull(arg_registry[idx].value);
	}
	return default_val;
}

bool cmdline_has_key(const char* key) {
	return (cmdline_find(key) != -1);
}
