#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t time_t;

void time_init(bool sync_now);
void time_sync(void *data);
time_t get_epoch();
