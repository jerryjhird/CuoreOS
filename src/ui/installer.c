#include <stdint.h>
#include "disk/partition.h"
#include "fs/cuorefs.h"
#include "devices.h"
#include "drivers/PS2.h"
#include "disk/mbr.h"
#include "disk/gpt.h"
#include "disk/guid_list.h"
#include "kstate.h"
#include "logbuf.h"

void installer_show_menu(void) {
	dev_puts(&flanterm_dev, "\n--- CUORE OS INSTALLER ---\n");

	dev_puts(&flanterm_dev, "Select table: (m=MBR, g=GPT): ");
	char type = 0;
	while (type != 'm' && type != 'g') {
		type = ps2_getc();
	}
	dev_puts(&flanterm_dev, (char[]){type, '\n', '\0'});

	if (type == 'g') {
		gpt_install(&active_disk_device, "CUORE-FS", (uint8_t[])GUID_CUORE_BASIC_DATA);
		logbuf_write("[ GPT  ] GPT installed\n");
		partition_refresh(&active_disk_device);
	} else if (type == 'm') {
		mbr_install(&active_disk_device, 0x7F);
		logbuf_write("[ MBR  ] MBR installed\n");
		partition_refresh(&active_disk_device);
	}

	partition_t* p_iter = head;
	int count = 0;
	while (p_iter != NULL) {
		if (p_iter->disk == &active_disk_device) { count++; }
		p_iter = (partition_t*)p_iter->next;
	}

	if (count == 0) {
		dev_puts(&flanterm_dev, "Error: No partitions found.\n");
		return;
	}

	dev_puts(&flanterm_dev, "Select partition (1-9): ");
	partition_t* selected = NULL;
	while (!selected) {
		char c = ps2_getc();
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

	dev_puts(&flanterm_dev, "[  FS  ] Formatting...");
	if (cuorefs_format(selected, 128) == 0) {
		dev_puts(&flanterm_dev, " SUCCESS\n");
	} else {
		dev_puts(&flanterm_dev, " FAILED\n");
		panic("INSTALLER", "Failed to install");
	}
}
