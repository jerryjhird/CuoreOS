#include <stdint.h>
#include "disk/partition.h"
#include "fs/cuorefs.h"
#include "devices.h"
#include "drivers/PS2.h"
#include "disk/mbr.h"
#include "disk/gpt.h"
#include "disk/guid_list.h"
#include "kstate.h"

void installer_show_menu(void) {
	wants_to_install_prompt:
	dev_puts(&flanterm_dev, "\x1b[H\x1b[J\nCuoreFS has not been detected\nWould you Like to Install CuoreFS? (y/n): ");

	char wants_to_install = ps2_getc_blocking();

	switch(wants_to_install) {
		case 'y': dev_puts(&flanterm_dev, "\x1b[H\x1b[J"); break;
		case 'n': dev_puts(&flanterm_dev, "\x1b[H\x1b[J"); return;
		default: goto wants_to_install_prompt;
	}

	disk_selection_prompt:
	dev_puts(&flanterm_dev, "\n--- SELECT DISK ---\n");

	if (disk_devices_c == 0) {
		dev_puts(&flanterm_dev, "No disks found!\n");
		return;
	}

	for (size_t i = 0; i < disk_devices_c; i++) {
		dev_puts(&flanterm_dev, "Disk ");
		dev_putint(&flanterm_dev, i + 1);
		dev_puts(&flanterm_dev, (disk_devices[i]->model[0]) ? " (" : " (Unknown Disk)\n");
		if (disk_devices[i]->model[0]) {
			dev_puts(&flanterm_dev, disk_devices[i]->model);
			dev_puts(&flanterm_dev, ")\n");
		}
	}

	dev_puts(&flanterm_dev, "Select Disk (1-");
	dev_putint(&flanterm_dev, disk_devices_c);
	dev_puts(&flanterm_dev, "): ");

	kernel_disk_dev_t* chosen_disk = NULL;
	while (!chosen_disk) {
		char c = ps2_getc_blocking();
		int idx = c - '1';
		if (idx >= 0 && idx < (int)disk_devices_c) {
			chosen_disk = disk_devices[idx];
			dev_puts(&flanterm_dev, (char[]){c, '\n', '\0'});
		}
	}

	boot_table_type_prompt:
	dev_puts(&flanterm_dev, "\nSelect table (m=MBR, g=GPT): ");
	char type = ps2_getc_blocking();
	dev_puts(&flanterm_dev, (char[]){type, '\n', '\0'});

	if (type == 'g') gpt_install(chosen_disk, "CUORE-FS", (uint8_t[])GUID_CUORE_BASIC_DATA);
	else if (type == 'm') mbr_install(chosen_disk, 0x7F);
	else goto boot_table_type_prompt;

	partition_refresh(chosen_disk);
	if (!chosen_disk->partitions) {
		dev_puts(&flanterm_dev, "No partitions created!\n");
		goto disk_selection_prompt;
	}

	dev_puts(&flanterm_dev, "Select partition (1-9): ");
	partition_t* selected_part = NULL;
	while (!selected_part) {
		char c = ps2_getc_blocking();
		int target = c - '0';
		int current = 0;
		for (partition_t* s = (partition_t*)chosen_disk->partitions; s; s = (partition_t*)s->next) {
			if (++current == target) {
				selected_part = s;
				dev_puts(&flanterm_dev, (char[]){c, '\n', '\0'});
				break;
			}
		}
	}

	dev_puts(&flanterm_dev, "\x1b[H\x1b[J[  FS  ] Formatting CuoreFS... ");
	if (cuorefs_format(selected_part, 128) == 0) {
		dev_puts(&flanterm_dev, "SUCCESS\n");
	} else {
		panic("INSTALLER", "Failed to install");
	}
}
