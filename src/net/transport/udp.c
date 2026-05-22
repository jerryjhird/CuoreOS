#include "udp.h"

#include "net/link/eth.h"
#include "mem/mem.h"
#include "net/netbuf.h"
#include "net/protocol/ntp.h"
#include "net/protocol/dns.h"
#include "net/socket.h"
#include "abs.h"

void udp_send(kernel_net_dev_t* dev, volatile socket_t* s, void* data, size_t len) {
	if (!s || s->type != SOCKET_TYPE_UDP) return;

	size_t total_udp_len = sizeof(udp_header_t) + len;
	net_buf_t* buf = net_buf_alloc(total_udp_len);

	udp_header_t* udp = (udp_header_t*)buf->data;
	udp->src_port = HTONS(s->local_port);
	udp->dest_port = HTONS(s->remote_port);
	udp->length = HTONS((uint16_t)total_udp_len);
	udp->checksum = 0;

	memcpy(buf->data + sizeof(udp_header_t), data, len);

	ipv4_send(dev, buf, s->remote_ip, 17);
}

// exists to maintain compatibility with old code (will be removed later, along with the reserved switch case in udp_handle)
void udp_send_nosock(kernel_net_dev_t* dev, uint32_t dest_ip, uint16_t src, uint16_t dest, void* data, size_t len) {
	size_t total_needed = sizeof(eth_header_t) + sizeof(ipv4_header_t) + sizeof(udp_header_t) + len;
	net_buf_t* buf = net_buf_alloc(total_needed);

	net_buf_reserve(buf, total_needed);

	uint8_t* payload = net_buf_push(buf, len);
	memcpy(payload, data, len);

	udp_header_t* udp = (udp_header_t*)net_buf_push(buf, sizeof(udp_header_t));
	udp->src_port = HTONS(src);
	udp->dest_port = HTONS(dest);
	udp->length = HTONS(len + sizeof(udp_header_t));
	udp->checksum = 0;

	ipv4_send(dev, buf, dest_ip, 17);
}

void udp_handle(kernel_net_dev_t* dev, ipv4_header_t* ip_hdr, void* transport_data, size_t len) {
	UNUSED(dev);
	UNUSED(len);

	udp_header_t* udp = (udp_header_t*)transport_data;
	uint16_t local_port = HTONS(udp->dest_port);
	uint16_t remote_port = HTONS(udp->src_port);
	void* payload = (uint8_t*)transport_data + sizeof(udp_header_t);
	size_t payload_len = HTONS(udp->length) - sizeof(udp_header_t);

	// system reserved (0 -> 1024)
	if (local_port < 1025) {
		switch (local_port) {
			case 53:
				dns_handle(payload);
				break;

			case 123:
				ntp_handle(payload);
				break;

			default:
				break;
		}
		return;
	}

	// check for sockets
	volatile socket_t* s = find_socket(local_port, remote_port, ip_hdr->src);

	if (s && s->type == SOCKET_TYPE_UDP && s->on_data) {
		s->on_data(payload, payload_len);
		return;
	}
}
