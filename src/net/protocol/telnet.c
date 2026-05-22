#include "telnet.h"

#include <stdint.h>
#include "device/types.h"
#include "device/char.h"
#include "net/transport/tcp.h"

volatile socket_t* telnet_socket = NULL;

static void telnet_callback(uint8_t* data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (data[i] == 0xFF) {
			i += 2;
			continue;
		}
		uart16550_dev.putc(data[i]);
	}
}

void telnet_client(kernel_net_dev_t* dev, uint32_t server_ip, uint16_t server_port) {
	dev_puts(&uart16550_dev, "\033[2J\033[H");

	telnet_socket = tcp_connect(dev, server_ip, server_port);
	if (telnet_socket) {
		telnet_socket->on_data = telnet_callback;
	}
}
