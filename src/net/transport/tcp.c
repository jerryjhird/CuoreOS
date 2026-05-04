#include "tcp.h"
#include "devices.h"
#include "mem/mem.h"
#include "net/ip/ipv4.h"
#include "net/protocol/dns.h"

volatile tcp_connection_t g_tcp = { .state = TCP_CLOSED };

static uint32_t checksum_accumulate(void* data, size_t length) {
	uint32_t sum = 0;
	uint16_t* ptr = (uint16_t*)data;

	while (length > 1) {
		sum += *ptr++;
		length -= 2;
	}

	if (length > 0) {
		sum += *(uint8_t*)ptr;
	}

	return sum;
}

static uint16_t checksum_fin(uint32_t sum) {
	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	return (uint16_t)~sum;
}

void tcp_send(kernel_net_dev_t* dev, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, uint8_t flags, void* payload, size_t payload_len) {
	size_t total_tcp_len = sizeof(tcp_header_t) + payload_len;
	net_buf_t* buf = net_buf_alloc(total_tcp_len);

	memset(buf->data, 0, total_tcp_len);
	buf->len = total_tcp_len;

	tcp_header_t* tcp = (tcp_header_t*)buf->data;

	tcp->src_port = HTONS(src_port);
	tcp->dest_port = HTONS(dest_port);
	tcp->seq = HTONL(g_tcp.seq);
	tcp->ack = HTONL(g_tcp.ack);
	tcp->res_off = (5 << 4); // 20 byte header, no options
	tcp->flags = flags;
	tcp->window	= HTONS(65535);
	tcp->checksum = 0;
	tcp->urgent	= 0;

	if (payload && payload_len > 0) {
		memcpy(buf->data + sizeof(tcp_header_t), payload, payload_len);
	}

	tcp_pseudo_header_t ph;
	ph.src_ip = dev->ip_addr;
	ph.dest_ip = dest_ip;
	ph.zero	= 0;
	ph.proto = 6; // TCP
	ph.tcp_len = HTONS((uint16_t)total_tcp_len);

	uint32_t sum = checksum_accumulate(&ph, sizeof(tcp_pseudo_header_t));
	sum += checksum_accumulate(tcp, total_tcp_len);
	tcp->checksum = checksum_fin(sum);

	ipv4_send(dev, buf, dest_ip, 6);
}

void tcp_handle(kernel_net_dev_t* dev, ipv4_header_t* ip, void* data, size_t len) {
	tcp_header_t* tcp = (tcp_header_t*)data;

	uint16_t ip_total_len = HTONS(ip->len);
	uint8_t ip_header_len = (ip->version_ihl & 0x0F) * 4;

	uint16_t tcp_segment_len = ip_total_len - ip_header_len;

	uint32_t remote_seq = HTONL(tcp->seq);
	uint32_t remote_ack = HTONL(tcp->ack);
	uint16_t remote_src_port = HTONS(tcp->src_port);

	uint8_t tcp_header_len = (tcp->res_off >> 4) * 4;

	uint32_t payload_len = 0;
	if (tcp_segment_len > tcp_header_len) {
		payload_len = tcp_segment_len - tcp_header_len;
	}

	if (tcp->flags & 0x04) {
		g_tcp.state = TCP_CLOSED;
		g_tcp.connection_closed = true;
		return;
	}

	switch (g_tcp.state) {
		case TCP_SYN_SENT:
			if ((tcp->flags & 0x12) == 0x12) {
				g_tcp.ack = remote_seq + 1;

				g_tcp.seq = remote_ack;
				g_tcp.state = TCP_ESTABLISHED;

				// ACK
				tcp_send(dev, ip->src, g_tcp.local_port, remote_src_port, 0x10, NULL, 0);
			}
			break;

		case TCP_ESTABLISHED: {
			if (remote_seq != g_tcp.ack) {
				tcp_send(dev, ip->src, g_tcp.local_port, remote_src_port, 0x10, NULL, 0);
				return;
			}

			if (payload_len > 0 || (tcp->flags & 0x01)) {
				g_tcp.ack = remote_seq + payload_len;

				if (tcp->flags & 0x01) {
					g_tcp.ack += 1;

					// reply FIN-ACK
					tcp_send(dev, ip->src, g_tcp.local_port, remote_src_port, 0x11, NULL, 0);
					g_tcp.seq += 1;

					g_tcp.state = TCP_CLOSED;
					g_tcp.connection_closed = true;
				} else {
					// 0x10
					tcp_send(dev, ip->src, g_tcp.local_port, remote_src_port, 0x10, NULL, 0);
				}

				if (payload_len > 0 && g_tcp.on_data) {
					g_tcp.on_data((uint8_t*)data + tcp_header_len, payload_len);
				}
			}
			break;
		}

		case TCP_FIN_WAIT:
			if (tcp->flags & 0x01) {
				g_tcp.ack = remote_seq + 1;
				tcp_send(dev, ip->src, g_tcp.local_port, remote_src_port, 0x10, NULL, 0);
				g_tcp.state = TCP_CLOSED;
				g_tcp.connection_closed = true;
			}
			break;
	}
}

void tcp_connect(kernel_net_dev_t* dev, uint32_t ip, uint16_t port) {
	g_tcp.local_port = 49152 + (query_id_counter % 1000);
	g_tcp.remote_port = port;
	g_tcp.remote_ip  = ip;
	g_tcp.state   = TCP_SYN_SENT;

	g_tcp.seq = 0;

	// send SYN
	tcp_send(dev, ip, g_tcp.local_port, port, 0x02, NULL, 0);
	g_tcp.seq += 1;
}

void tcp_disconnect(kernel_net_dev_t* dev) {
	if (g_tcp.state == TCP_CLOSED || g_tcp.state == TCP_FIN_WAIT) return;

	tcp_send(dev, g_tcp.remote_ip, g_tcp.local_port, g_tcp.remote_port, 0x11, NULL, 0);

	g_tcp.seq += 1;
	g_tcp.state = TCP_FIN_WAIT;
}

void tcp_write(kernel_net_dev_t* dev, void* data, size_t len) {
	if (g_tcp.state != TCP_ESTABLISHED) return;

	tcp_send(dev, g_tcp.remote_ip, g_tcp.local_port, g_tcp.remote_port, 0x18, data, len);
	g_tcp.seq += len;
}
