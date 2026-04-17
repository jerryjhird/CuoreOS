#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t time_t;

void time_init(void);
void time_sync(void);
time_t get_epoch(void);
