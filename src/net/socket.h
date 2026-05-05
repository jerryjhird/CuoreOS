#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
	SOCKET_TYPE_NONE = 0,
	SOCKET_TYPE_TCP,
	SOCKET_TYPE_UDP
} socket_type_t;

typedef struct {
	uint16_t local_port;
	uint16_t remote_port;
	uint32_t remote_ip;
	socket_type_t type;
	bool is_active;

	void (*on_data)(uint8_t* data, size_t len);

	union {
		struct {
			uint32_t seq;
			uint32_t ack;
			uint8_t state;
		} tcp;

		struct {
			bool placeholder;
		} udp;
	};
} socket_t;

#define MAX_SOCKETS 16

extern volatile socket_t socket_table[MAX_SOCKETS];

volatile socket_t* find_socket(uint16_t local_port, uint16_t remote_port, uint32_t remote_ip);
volatile socket_t* alloc_socket(socket_type_t type);
