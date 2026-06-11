#pragma once

#include "limine.h"

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_mp_request mp_request;
extern volatile struct limine_executable_address_request executable_request;
extern volatile struct limine_executable_cmdline_request cmdline_request;
extern volatile struct limine_smbios_request smbios_request;

extern uint64_t kernel_pml4_phys;
