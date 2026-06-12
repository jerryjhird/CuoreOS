#include "cedt.h"
#include "logbuf.h"

static cedt_ret_t g_cxl_info = {0};

bool cedt_get_info(cedt_ret_t* out_info) {
	if (!g_cxl_info.found) return false;
	*out_info = g_cxl_info;
	return true;
}

void cedt_init(struct acpi_sdt_header* table) {
	uint8_t* ptr = (uint8_t*)table + sizeof(struct acpi_sdt_header);
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

				bool is_valid_window = (cfmws->target_restrict == 0) || (cfmws->target_restrict & (ACPI_CEDT_CFMWS_RESTRICT_TYPE3 | ACPI_CEDT_CFMWS_RESTRICT_CXL_RAM));

				if (is_valid_window) {
					uint32_t num_targets = 1 << cfmws->interconnect_element_ways;
					for (uint32_t i = 0; i < num_targets; i++) {
						if (!has_uid || cfmws->target_windows[i] == active_uid) {
							g_cxl_info.phys_base = cfmws->window_base;
							g_cxl_info.size = cfmws->window_size;
							g_cxl_info.host_bridge_uid = cfmws->target_windows[i];
							g_cxl_info.found = true;

							logbuf_ok("[ ACPI ] CEDT: found CXL window at 0x%lx (Size: 0x%lx)\n", g_cxl_info.phys_base, g_cxl_info.size);
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
