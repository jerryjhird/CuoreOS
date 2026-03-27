#pragma once
#include "devicetypes.h"

#define MAX_DEVICES 9

extern output_dev_t uart16550_dev;
extern output_dev_t flanterm_dev;

extern output_dev_t* output_devices[MAX_DEVICES];
extern size_t output_devices_c;
