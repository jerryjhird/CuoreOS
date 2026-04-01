#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "src/fs/ramfs.h"

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("Usage: %s <out> <files...>\n", argv[0]);
		return 1;
	}

	FILE *out = fopen(argv[1], "wb");
	if (!out) return 1;

	uint32_t num_files = argc - 2;
	ramfs_header_t header = {RAMFS_MAGIC, num_files, 0};
	fwrite(&header, sizeof(ramfs_header_t), 1, out);

	ramfs_entry_t *table = calloc(num_files, sizeof(ramfs_entry_t));
	uint64_t current_off = sizeof(ramfs_header_t) + (num_files * sizeof(ramfs_entry_t));

	for (int i = 0; i < num_files; i++) {
		const char *full_path = argv[i+2];
		FILE *in = fopen(full_path, "rb");
		if (!in) {
			fprintf(stderr, "Could not open %s\n", full_path);
			continue;
		}

		const char *filename = strrchr(full_path, '/');
		if (filename) {
			filename++;
		} else {
			filename = full_path;
		}

		fseek(in, 0, SEEK_END);
		table[i].size = ftell(in);
		table[i].offset = current_off;

		memset(table[i].name, 0, 64);
		strncpy(table[i].name, filename, 63);

		current_off += table[i].size;
		fclose(in);
	}

	fwrite(table, sizeof(ramfs_entry_t), num_files, out);

	for (int i = 0; i < num_files; i++) {
		FILE *in = fopen(argv[i+2], "rb");
		if (!in) continue;
		char *buf = malloc(table[i].size);
		fread(buf, 1, table[i].size, in);
		fwrite(buf, 1, table[i].size, out);
		free(buf);
		fclose(in);
	}

	fclose(out);
	free(table);
	return 0;
}
