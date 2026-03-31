#pragma once

#define MAX_DEVICES 9

#include "devicetypes.h"

extern kernel_dev_t uart16550_dev;
extern kernel_dev_t flanterm_dev;

extern kernel_dev_t *output_devices[MAX_DEVICES];
extern size_t output_devices_c;
