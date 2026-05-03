#include "udp.h"

#include "devices.h"
#include "net/link/eth.h"
#include "mem/mem.h"
#include "net/netbuf.h"
#include "net/protocol/ntp.h"

void udp_send(kernel_net_dev_t* dev, uint32_t dest_ip, uint16_t src, uint16_t dest, void* data, size_t len) {
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
	udp_header_t* udp = (udp_header_t*)transport_data;

	uint16_t src_port = NTOHS(udp->src_port);
	uint16_t dest_port = NTOHS(udp->dest_port);
	uint16_t udp_len = NTOHS(udp->length);

	if (udp_len > len || udp_len < sizeof(udp_header_t)) return;

	void* payload = (void*)(udp + 1);
	size_t payload_len = udp_len - sizeof(udp_header_t);

	switch (dest_port) {
		case 68:
			// dhcp_handle(dev, payload, payload_len);
			break;

		case 69: {
			// tftp_handle(dev, payload, payload_len);
			break;
		}

		case 53:
			// dns_handle(something);
			break;

		case 123:
			ntp_handle(payload, payload_len);
			break;

		default:
			break;
	}
}
