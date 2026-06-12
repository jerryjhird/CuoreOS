#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "acpi.h"

typedef struct acpi_fadt {
	acpi_sdt_header_t header;
	uint32_t firmware_ctrl; // physical address of facs
	uint32_t dsdt; // physical address of dsdt
	uint8_t reserved; // field no longer used
	uint8_t preferred_pm_profile;
	uint16_t sci_int; // system control interrupt
	uint32_t smi_cmd; // system management interrupt command port
	uint8_t acpi_enable; // value to write to smi_cmd to enable acpi
	uint8_t acpi_disable; // value to write to smi_cmd to disable acpi
	uint8_t s4bios_req; // value to write to smi_cmd to enter s4bios state
	uint8_t pstate_cnt; // processor performance state control
	uint32_t pm1a_evt_blk; // port address of pm1a event reg. block
	uint32_t pm1b_evt_blk; // port address of pm1b event reg. block
	uint32_t pm1a_cnt_blk; // port address of pm1a control reg. block
	uint32_t pm1b_cnt_blk; // port address of pm1b control reg. block
	uint32_t pm2_cnt_blk; // port address of pm2 control reg. block
	uint32_t pm_tmr_blk; // port address of pm timer control reg. block
	uint32_t gpe0_blk; // port address of general purpose event 0 reg. block
	uint32_t gpe1_blk; // port address of general purpose event 1 reg. block
	uint8_t pm1_evt_len; // length of pm1 event reg. block
	uint8_t pm1_cnt_len; // length of pm1 control reg. block
	uint8_t pm2_cnt_len; // length of pm2 control reg. block
	uint8_t pm_tmr_len; // length of pm timer control reg. block
	uint8_t gpe0_len; // length of gpe0 reg. block
	uint8_t gpe1_len; // length of gpe1 reg. block
	uint8_t gpe1_base; // offset in gpe number space where gpe1 begins
	uint8_t cstate_cnt; // value to write to smi_cmd to support _cst
	uint16_t flush_size; // size of cache to flush
	uint16_t flush_stride; // stride of cache flush
	uint8_t duty_offset; // processor duty cycle starting bit
	uint8_t duty_width; // processor duty cycle bit width
	uint8_t day_alrm; // index to rtc day alarm
	uint8_t mon_alrm; // index to rtc month alarm
	uint8_t century; // index to rtc century
	uint16_t iapc_boot_arch; // ia-pc boot architecture flags
	uint8_t reserved2; // reserved
	uint32_t flags; // fixed feature flags
	acpi_gas_t reset_reg; // hardware reset register
	uint8_t reset_value; // value to write to reset_reg to reset system
	uint8_t reserved3[3]; // reserved
	uint64_t x_firmware_ctrl; // 64-bit facs pointer
	uint64_t x_dsdt; // 64-bit dsdt pointer
	acpi_gas_t x_pm1a_evt_blk; // 64-bit pm1a event register block
	acpi_gas_t x_pm1b_evt_blk; // 64-bit pm1b event register block
	acpi_gas_t x_pm1a_cnt_blk; // 64-bit pm1a control register block
	acpi_gas_t x_pm1b_cnt_blk; // 64-bit pm1b control register block
	acpi_gas_t x_pm2_cnt_blk; // 64-bit pm2 control register block
	acpi_gas_t x_pm_tmr_blk; // 64-bit pm timer control register block
	acpi_gas_t x_gpe0_blk; // 64-bit gpe0 register block
	acpi_gas_t x_gpe1_blk; // 64-bit gpe1 register block
} __attribute__((packed)) acpi_fadt_t;

extern acpi_fadt_t* fadt;

void fadt_init(struct acpi_sdt_header* acpi_tab);
uint16_t fadt_get_s5_types(void);
uintptr_t fadt_get_pm1a_cnt(void);
bool fadt_is_pm1a_mmio(void);
