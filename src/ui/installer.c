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
	dev_puts(&flanterm_dev,"\nCuoreFS has not been detected\n");
	dev_puts(&flanterm_dev,"Would you Like to Install CuoreFS? (y/n): ");

	char wants_to_install = ps2_getc_blocking();

	switch(wants_to_install) {
		case 'y':
			dev_puts(&flanterm_dev, "\x1b[H\x1b[J"); // clear screen
			break;
		case 'n':
			dev_puts(&flanterm_dev, "\x1b[H\x1b[J"); // clear screen
			return;
		default: // account for hooman error
			dev_puts(&flanterm_dev, "\x1b[H\x1b[J"); // clear screen
			dev_puts(&flanterm_dev, "Invalid selection. Please try again.\n");
			goto wants_to_install_prompt;
	}

	boot_table_type_prompt:
	dev_puts(&flanterm_dev, "\n--- CUORE OS INSTALLER ---\n");
	dev_puts(&flanterm_dev, "Select table: (m=MBR, g=GPT): ");

	char type = ps2_getc_blocking();

	dev_puts(&flanterm_dev, (char[]){type, '\n', '\0'});

	switch (type) {
		case 'g':
			gpt_install(&active_disk_device, "CUORE-FS", (uint8_t[])GUID_CUORE_BASIC_DATA);
			break;
		case 'm':
			mbr_install(&active_disk_device, 0x7F);
			break;
		default: // account for hooman error
			dev_puts(&flanterm_dev, "\x1b[H\x1b[J"); // clear screen
			dev_puts(&flanterm_dev, "Invalid selection. Please try again.\n\n");
			goto boot_table_type_prompt;
	}

	partition_refresh(&active_disk_device);

	partition_t* p_iter = head;
	int count = 0;
	while (p_iter != NULL) {
		if (p_iter->disk == &active_disk_device) { count++; }
		p_iter = (partition_t*)p_iter->next;
	}

	dev_puts(&flanterm_dev, "Select partition (1-");
	dev_putint(&flanterm_dev, count);
	dev_puts(&flanterm_dev, "): ");

	partition_t* selected = NULL;
	while (!selected) {
		char c = ps2_getc_blocking();
		if (c >= '1' && c <= ('0' + count)) {
			int target = c - '0';
			partition_t* s = head;
			int found = 0;
			while (s != NULL) {
				if (s->disk == &active_disk_device) {
					found++;
					if (found == target) {
						selected = s;
						break;
					}
				}
				s = (partition_t*)s->next;
			}
			dev_puts(&flanterm_dev, (char[]){c, '\n', '\0'});
		}
	}

	dev_puts(&flanterm_dev, "\x1b[H\x1b[J"); // clear screen

	dev_puts(&flanterm_dev, "[  FS  ] Formatting CuoreFS... ");
	if (cuorefs_format(selected, 128) == 0) {
		dev_puts(&flanterm_dev, "SUCCESS\n");
	} else {
		dev_puts(&flanterm_dev, "FAILED\n");
		panic("INSTALLER", "Failed to install");
	}
}
