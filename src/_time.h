#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t time_t;

void time_init(void);
void time_sync(void);
time_t get_epoch(void);

// sleep
void msleep(uint64_t ms);
void usleep(uint64_t us);
