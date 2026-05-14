#include "tcp.h"

#include "devices.h"
#include "mem/mem.h"
#include "net/ip/ipv4.h"
#include "net/protocol/dns.h"
#include "net/socket.h"
#include "abs.h"

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

void tcp_send(kernel_net_dev_t* dev, volatile socket_t* s, uint8_t flags, void* payload, size_t payload_len) {
	size_t total_tcp_len = sizeof(tcp_header_t) + payload_len;
	net_buf_t* buf = net_buf_alloc(total_tcp_len);

	memset(buf->data, 0, total_tcp_len);
	buf->len = total_tcp_len;

	tcp_header_t* tcp = (tcp_header_t*)buf->data;

	tcp->src_port = HTONS(s->local_port);
	tcp->dest_port = HTONS(s->remote_port);
	tcp->seq = HTONL(s->tcp.seq);
	tcp->ack = HTONL(s->tcp.ack);
	tcp->res_off = (5 << 4);
	tcp->flags = flags;
	tcp->window = HTONS(65535);
	tcp->checksum = 0;
	tcp->urgent = 0;

	if (payload && payload_len > 0) {
		memcpy(buf->data + sizeof(tcp_header_t), payload, payload_len);
	}

	tcp_pseudo_header_t ph;
	ph.src_ip = dev->ip_addr;
	ph.dest_ip = s->remote_ip;
	ph.zero = 0;
	ph.proto = 6; // TCP
	ph.tcp_len = HTONS((uint16_t)total_tcp_len);

	uint32_t sum = checksum_accumulate(&ph, sizeof(tcp_pseudo_header_t));
	sum += checksum_accumulate(tcp, total_tcp_len);
	tcp->checksum = checksum_fin(sum);

	ipv4_send(dev, buf, s->remote_ip, 6);
}

void tcp_handle(kernel_net_dev_t* dev, ipv4_header_t* ip, void* data, size_t len) {
	UNUSED(len);
	tcp_header_t* tcp = (tcp_header_t*)data;

	uint16_t local_port = HTONS(tcp->dest_port);
	uint16_t remote_port = HTONS(tcp->src_port);

	volatile socket_t* s = find_socket(local_port, remote_port, ip->src);
	if (!s) return;

	uint16_t ip_total_len = HTONS(ip->len);
	uint8_t ip_header_len = (ip->version_ihl & 0x0F) * 4;
	uint16_t tcp_segment_len = ip_total_len - ip_header_len;
	uint8_t tcp_header_len = (tcp->res_off >> 4) * 4;

	uint32_t remote_seq = HTONL(tcp->seq);
	uint32_t remote_ack = HTONL(tcp->ack);
	uint32_t payload_len = (tcp_segment_len > tcp_header_len) ? (tcp_segment_len - tcp_header_len) : 0;

	if (tcp->flags & 0x04) { // RST
		s->tcp.state = TCP_CLOSED;
		s->is_active = false;
		return;
	}

	switch (s->tcp.state) {
		case TCP_SYN_SENT:
			if ((tcp->flags & 0x12) == 0x12) { // SYN-ACK
				s->tcp.ack = remote_seq + 1;
				s->tcp.seq = remote_ack;
				s->tcp.state = TCP_ESTABLISHED;
				tcp_send(dev, s, 0x10, NULL, 0); // ACK
			}
			break;

		case TCP_ESTABLISHED:
			if (remote_seq != s->tcp.ack) {
				tcp_send(dev, s, 0x10, NULL, 0); // Re-ACK
				return;
			}

			if (payload_len > 0 || (tcp->flags & 0x01)) {
				s->tcp.ack += payload_len;

				if (tcp->flags & 0x01) { // FIN
					s->tcp.ack += 1;
					tcp_send(dev, s, 0x11, NULL, 0); // FIN-ACK
					s->tcp.state = TCP_CLOSED;
					s->is_active = false;
				} else {
					tcp_send(dev, s, 0x10, NULL, 0); // ACK
				}

				if (payload_len > 0 && s->on_data) {
					s->on_data((uint8_t*)data + tcp_header_len, payload_len);
				}
			}
			break;
	}
}

volatile socket_t* tcp_connect(kernel_net_dev_t* dev, uint32_t ip, uint16_t port) {
	volatile socket_t* s = alloc_socket(SOCKET_TYPE_TCP);

	s->local_port = 49152 + (query_id_counter++ % 1000);
	s->remote_port = port;
	s->remote_ip = ip;
	s->tcp.state = TCP_SYN_SENT;
	s->tcp.seq = 0;
	s->tcp.ack = 0;

	tcp_send(dev, s, 0x02, NULL, 0); // SYN
	s->tcp.seq += 1;
	return s;
}

void tcp_disconnect(kernel_net_dev_t* dev, socket_t* s) {
	if (!s || s->tcp.state == TCP_CLOSED || s->tcp.state == TCP_FIN_WAIT) {
		return;
	}

	tcp_send(dev, s, 0x11, NULL, 0);

	s->tcp.seq += 1;
	s->tcp.state = TCP_FIN_WAIT;
}

void tcp_write(kernel_net_dev_t* dev, socket_t* s, void* data, size_t len) {
	if (!s || s->tcp.state != TCP_ESTABLISHED) return;
	tcp_send(dev, s, 0x18, data, len); // PSH-ACK
	s->tcp.seq += len;
}
