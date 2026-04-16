#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "limine.h"
#include "cpu/smp/init.h"

#define UNUSED(x) ((void)(x)) // helper macro

#ifdef SERIOUS
	#define MICROSOFT_STRING "Microsoft"
#else
	#define MICROSOFT_STRING "Microslop"
#endif

void panic(const char* header_msg, const char* msg);

extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_mp_request mp_request;
extern volatile struct limine_executable_address_request executable_request;

extern cpu_control_block_t* online_cpus[SMP_MAX_CORES];
extern volatile uint64_t online_cpu_index;
extern bool supported_disk_exists;
