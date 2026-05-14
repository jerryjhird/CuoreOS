#pragma once

#include <stdint.h>
#include "devices.h"
#include "net/ip/ipv4.h"
#include "net/socket.h"

typedef struct {
	uint16_t src_port;
	uint16_t dest_port;
	uint32_t seq;
	uint32_t ack;
	uint8_t  res_off;
	uint8_t  flags;
	uint16_t window;
	uint16_t checksum;
	uint16_t urgent;
} __attribute__((packed)) tcp_header_t;

typedef struct {
	uint32_t src_ip;
	uint32_t dest_ip;
	uint8_t  zero;
	uint8_t  proto;
	uint16_t tcp_len;
} __attribute__((packed)) tcp_pseudo_header_t;

typedef enum {
	TCP_CLOSED,
	TCP_SYN_SENT,
	TCP_ESTABLISHED,
	TCP_FIN_WAIT
} tcp_state_t;

typedef void (*tcp_callback_t)(uint8_t* data, size_t len);

typedef struct {
	tcp_state_t state;
	uint32_t remote_ip;
	uint16_t remote_port;
	uint16_t local_port;
	uint32_t seq;
	uint32_t ack;
	volatile bool data_received;
	volatile bool connection_closed;
	tcp_callback_t on_data;
} tcp_connection_t;

extern volatile tcp_connection_t g_tcp;

void tcp_handle(kernel_net_dev_t* dev, ipv4_header_t* ip, void* data, size_t len);

volatile socket_t* tcp_connect(kernel_net_dev_t* dev, uint32_t ip, uint16_t port);
void tcp_disconnect(kernel_net_dev_t* dev, socket_t* s);
void tcp_write(kernel_net_dev_t* dev, socket_t* s, void* data, size_t len);
void tcp_send(kernel_net_dev_t* dev, volatile socket_t* s, uint8_t flags, void* payload, size_t payload_len);
