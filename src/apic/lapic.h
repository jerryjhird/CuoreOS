#pragma once

#include <stdint.h>

void lapic_init(uintptr_t base_addr);
void lapic_eoi(void);
