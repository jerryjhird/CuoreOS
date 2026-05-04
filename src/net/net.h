#pragma once

#include "devices.h"

#define IP_ADDR(a, b, c, d) ((a) | (b << 8) | (c << 16) | (d << 24))

void nping(kernel_net_dev_t* dev, uint32_t target_ip);
void QOTDSend(kernel_net_dev_t* dev, uint32_t dest_ip);
void telnet_client(kernel_net_dev_t* dev, uint32_t dest_ip);
