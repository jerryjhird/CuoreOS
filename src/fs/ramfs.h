#include <stdint.h>
#include <stddef.h>

#define RAMFS_MAGIC 0x494E495452414D21ULL

// Baked into the archive

typedef struct  {
	uint64_t magic;
	uint32_t file_count;
	uint32_t reserved;
} __attribute__((packed)) ramfs_header_t;

typedef struct {
	char name[64];
	uint64_t offset;
	uint64_t size;
} __attribute__((packed)) ramfs_entry_t;

// used at runtime by kernel

typedef struct {
	ramfs_header_t *header;
	ramfs_entry_t  *entries;
	void		   *base;
} ramfs_handle_t;

typedef struct {
	void *data;
	uint64_t size;
} ramfs_file_t;

ramfs_handle_t ramfs_init(void *ptr);
ramfs_file_t ramfs_get_file(ramfs_handle_t *handle, const char *name);
