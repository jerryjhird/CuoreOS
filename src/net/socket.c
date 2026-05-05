#include "socket.h"
#include "mem/mem.h"

socket_t volatile socket_table[MAX_SOCKETS];
static uint16_t port_c = 1025;

volatile socket_t* find_socket(uint16_t local_port, uint16_t remote_port, uint32_t remote_ip) {
	for (int i = 0; i < MAX_SOCKETS; i++) {
		if (!socket_table[i].is_active) continue;

		// exact match
		if (socket_table[i].local_port == local_port &&
			socket_table[i].remote_port == remote_port &&
			socket_table[i].remote_ip == remote_ip) {
			return &socket_table[i];
		}

		// port match but wildcard IP
		if (socket_table[i].local_port == local_port &&
			(socket_table[i].remote_ip == 0 || socket_table[i].remote_ip == 0xFFFFFFFF)) {
			return &socket_table[i];
		}
	}

	return NULL;
}

volatile socket_t* alloc_socket(socket_type_t type) {
	for (int i = 0; i < MAX_SOCKETS; i++) {
		if (!socket_table[i].is_active) {
			memset((void*)&socket_table[i], 0, sizeof(socket_t));
			socket_table[i].is_active = true;
			socket_table[i].type = type;
			socket_table[i].local_port = port_c++;

			if (port_c == 0) { // if 16 bit wrap around
				port_c = 1025;
			}

			return &socket_table[i];
		}
	}
	return NULL;
}
