#include "icmp.h"

icmp_reply_callback_t g_icmp_reply_handler  = NULL;

void icmp_send(kernel_net_dev_t* dev, net_buf_t* buf, uint32_t dest_ip, uint8_t type, uint8_t code) {
	icmp_header_t* icmp = (icmp_header_t*)net_buf_push(buf, sizeof(icmp_header_t));

	icmp->type = type;
	icmp->code = code;
	icmp->checksum = 0;
	icmp->checksum = ipv4_checksum(icmp, buf->len);

	ipv4_send(dev, buf, dest_ip, 1);
}

void icmp_handle(kernel_net_dev_t* dev, net_buf_t* buf, ipv4_header_t* ip_hdr) {
	if (buf->len < sizeof(icmp_header_t)) return;

	icmp_header_t* icmp = (icmp_header_t*)buf->data;

	if (icmp->type == ICMP_TYPE_ECHO_REPLY) {
		if (g_icmp_reply_handler) {
			g_icmp_reply_handler(ip_hdr->src, icmp->sequence, 64, buf->len);
		}
	}

	if (icmp->type == ICMP_TYPE_ECHO_REQUEST) {
		uint16_t id = icmp->id;
		uint16_t seq = icmp->sequence;

		net_buf_pull(buf, sizeof(icmp_header_t));

		icmp_header_t* reply = (icmp_header_t*)net_buf_push(buf, sizeof(icmp_header_t));
		reply->type = ICMP_TYPE_ECHO_REPLY;
		reply->code = 0;
		reply->id = id;
		reply->sequence = seq;
		reply->checksum = 0;
		reply->checksum = ipv4_checksum(reply, buf->len);

		ipv4_send(dev, buf, ip_hdr->src, 1);
	}
}
