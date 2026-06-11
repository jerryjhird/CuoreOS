#include "cedt.h"

#include <stdint.h>
#include "logbuf.h"

void parse_cedt(acpi_sdt_header_t* table, cedt_ret_t* match) {
	uint8_t* ptr = (uint8_t*)table + sizeof(acpi_sdt_header_t);
	uint8_t* end = (uint8_t*)table + table->length;

	uint32_t active_uid = 0;
	bool has_uid = false;

	while (ptr < end) {
		acpi_cedt_header_t* sub = (acpi_cedt_header_t*)ptr;
		if (sub->length == 0) break;

		switch (sub->type) {
			case ACPI_CEDT_TYPE_CHBS: {
				acpi_cedt_chbs_t* chbs = (acpi_cedt_chbs_t*)ptr;
				active_uid = chbs->local_uid;
				has_uid = true;
				break;
			}
			case ACPI_CEDT_TYPE_CFMWS: {
				acpi_cedt_cfmws_t* cfmws = (acpi_cedt_cfmws_t*)ptr;

				// fallback
				bool is_valid_window = (cfmws->target_restrict == 0) ||
									(cfmws->target_restrict & (ACPI_CEDT_CFMWS_RESTRICT_TYPE3 | ACPI_CEDT_CFMWS_RESTRICT_CXL_RAM));

				if (is_valid_window) {
					uint32_t num_targets = 1 << cfmws->interconnect_element_ways;

					for (uint32_t i = 0; i < num_targets; i++) {
						if (!has_uid || cfmws->target_windows[i] == active_uid) {
							match->phys_base = cfmws->window_base;
							match->size = cfmws->window_size;
							match->host_bridge_uid = cfmws->target_windows[i];
							match->found = true;
							return;
						}
					}
				}
				break;
			}
			default:
				break;
		}
		ptr += sub->length;
	}
}
