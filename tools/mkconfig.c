#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "src/kstate.h"

bool find_in_line(const char* start, size_t limit, const char* target) {
	size_t target_len = strlen(target);
	if (target_len > limit) return false;

	for (size_t i = 0; i <= limit - target_len; i++) {
		if (strncmp(&start[i], target, target_len) == 0) {
			return true;
		}
	}
	return false;
}

bool get_bool_val(const char* src, const char* key, bool default_val) {
	const char *line = src;
	size_t key_len = strlen(key);

	while ((line = strstr(line, key)) != NULL) {
		bool at_start = (line == src || line[-1] == '\n' || line[-1] == '\r');
		char next = line[key_len];
		bool exact_key = (next == ' ' || next == '=' || next == '\t');

		if (at_start && exact_key) {
			char *val_pos = strchr(line, '=');
			if (!val_pos) return default_val;

			char *end_of_line = strchr(val_pos, '\n');
			size_t limit = end_of_line ? (size_t)(end_of_line - val_pos) : strlen(val_pos);

			if (find_in_line(val_pos, limit, "true")) return true;
			if (find_in_line(val_pos, limit, "false")) return false;
		}
		line += key_len;
	}
	return default_val;
}

int main(int argc, char *argv[]) {
	FILE *f = fopen(argv[1], "r");
	if (!f) { perror("fopen input"); return 1; }

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);

	char *buffer = malloc(size + 1);
	fread(buffer, 1, size, f);
	buffer[size] = '\0';
	fclose(f);

	kernel_config_t config = {0};
	config.magic = CONFIG_MAGIC;
	config.epoch = (uint64_t)time(NULL);

	config.debug = get_bool_val(buffer, "debug", false);
	config.flanterm_is_debug_interface = get_bool_val(buffer, "flanterm_is_debug_interface", false);
	config.uart16550_is_debug_interface = get_bool_val(buffer, "uart16550_is_debug_interface", false);

	FILE *out = fopen(argv[2], "wb");
	if (!out) { perror("fopen output"); return 1; }

	fwrite(&config, sizeof(kernel_config_t), 1, out);

	printf("config baked: %s (%zu bytes)\n", argv[2], sizeof(kernel_config_t));

	fclose(out);
	free(buffer);
	return 0;
}
