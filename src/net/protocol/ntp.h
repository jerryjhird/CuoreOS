#pragma once

#include <stdint.h>
#include <stddef.h>
#include "device/types.h"

#define NTP_OFFSET 2208988800ULL

typedef struct {
	uint8_t flags;
	uint8_t stratum;
	uint8_t poll;
	uint8_t precision;
	uint32_t root_delay;
	uint32_t root_dispersion;
	uint32_t ref_id;
	uint32_t ref_ts_s;
	uint32_t ref_ts_f;
	uint32_t orig_ts_s;
	uint32_t orig_ts_f;
	uint32_t recv_ts_s;
	uint32_t recv_ts_f;
	uint32_t trans_ts_s;
	uint32_t trans_ts_f;
} __attribute__((packed)) ntp_packet_t;

typedef struct {
	uint64_t utc_timestamp;
	uint32_t precision_ms;
	uint32_t root_delay;
	uint8_t  stratum;
	uint8_t  version;
	bool	 leap_warning;
} ntp_info_t;

typedef void (*ntp_callback_t)(ntp_info_t*);

extern ntp_callback_t g_ntp_callback;
#define NTP_SET_CALLBACK(func) (g_ntp_callback = (ntp_callback_t)(func))

void ntp_send_request(kernel_net_dev_t* dev, uint32_t server_ip);
void ntp_handle(void* data);
