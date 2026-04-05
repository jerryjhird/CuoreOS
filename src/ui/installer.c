#include <stdint.h>
#include "disk/partition.h"
#include "fs/cuorefs.h"
#include "devices.h"
#include "drivers/PS2.h"
#include "disk/mbr.h"
#include "disk/gpt.h"
#include "disk/guid_list.h"
#include "kstate.h"

#include "pci/drivers/ide.h"

void installer_show_menu(void) {
	wants_to_install_prompt:
	dev_puts(&flanterm_dev, "\x1b[H\x1b[J");
	dev_puts(&flanterm_dev,"\nCuoreFS has not been detected\n");
	dev_puts(&flanterm_dev,"Would you Like to Install CuoreFS? (y/n): ");

	char wants_to_install = ps2_getc_blocking();

	switch(wants_to_install) {
		case 'y': dev_puts(&flanterm_dev, "\x1b[H\x1b[J"); break;
		case 'n': dev_puts(&flanterm_dev, "\x1b[H\x1b[J"); return;
		default: goto wants_to_install_prompt;
	}

	disk_selection_prompt:
	dev_puts(&flanterm_dev, "\n--- SELECT DISK ---\n");

	kernel_dev_t* disks[MAX_DEVICES];
	int disk_count = 0;

	for (size_t i = 0; i < output_devices_c; i++) {
		if (DEV_CAP_HAS(output_devices[i], CAP_IS_DISK)) {
			disks[disk_count] = output_devices[i];

			ide_ctx_t* internal_ctx = (ide_ctx_t*)disks[disk_count]->ctx;

			dev_puts(&flanterm_dev, "Disk ");
			dev_putint(&flanterm_dev, disk_count + 1);
			dev_puts(&flanterm_dev, " (");

			if (internal_ctx && internal_ctx->model[0] != '\0') {
				dev_puts(&flanterm_dev, internal_ctx->model);
			} else {
				dev_puts(&flanterm_dev, "Unknown Disk");
			}

			dev_puts(&flanterm_dev, ")\n");
			disk_count++;
		}
	}

	if (disk_count == 0) {
		dev_puts(&flanterm_dev, "No disks found!\n");
		return;
	}

	dev_puts(&flanterm_dev, "Select Disk (1-");
	dev_putint(&flanterm_dev, disk_count);
	dev_puts(&flanterm_dev, "): ");

	kernel_dev_t* chosen_disk = NULL;
	while (!chosen_disk) {
		char c = ps2_getc_blocking();
		if (c >= '1' && c <= ('0' + disk_count)) {
			chosen_disk = disks[c - '1'];
			dev_puts(&flanterm_dev, (char[]){c, '\n', '\0'});
		}
	}

	// Boot table selection
	boot_table_type_prompt:
	dev_puts(&flanterm_dev, "\nSelect table (m=MBR, g=GPT): ");
	char type = ps2_getc_blocking();
	dev_puts(&flanterm_dev, (char[]){type, '\n', '\0'});

	switch (type) {
		case 'g': gpt_install(chosen_disk, "CUORE-FS", (uint8_t[])GUID_CUORE_BASIC_DATA); break;
		case 'm': mbr_install(chosen_disk, 0x7F); break;
		default: goto boot_table_type_prompt;
	}

	// Partition selection
	partition_refresh(chosen_disk);

	partition_t* p_iter = (partition_t*)chosen_disk->partitions;
	int p_count = 0;
	while (p_iter != NULL) {
		p_count++;
		p_iter = (partition_t*)p_iter->next;
	}

	if (p_count == 0) {
		dev_puts(&flanterm_dev, "No partitions created!\n");
		goto disk_selection_prompt;
	}

	dev_puts(&flanterm_dev, "Select partition (1-");
	dev_putint(&flanterm_dev, p_count);
	dev_puts(&flanterm_dev, "): ");

	partition_t* selected_part = NULL;
	while (!selected_part) {
		char c = ps2_getc_blocking();
		if (c >= '1' && c <= ('0' + p_count)) {
			int target = c - '0';
			partition_t* s = (partition_t*)chosen_disk->partitions;
			int found = 0;
			while (s != NULL) {
				if (++found == target) {
					selected_part = s;
					break;
				}
				s = (partition_t*)s->next;
			}
			dev_puts(&flanterm_dev, (char[]){c, '\n', '\0'});
		}
	}

	dev_puts(&flanterm_dev, "\x1b[H\x1b[J[  FS  ] Formatting CuoreFS... ");
	if (cuorefs_format(selected_part, 128) == 0) {
		dev_puts(&flanterm_dev, "SUCCESS\n");
	} else {
		dev_puts(&flanterm_dev, "FAILED\n");
		panic("INSTALLER", "Failed to install");
	}
}
