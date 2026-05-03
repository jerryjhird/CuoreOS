#include "eth.h"
#include "net/arp.h"
#include "net/ip/ipv4.h"
#include "mem/mem.h"

void eth_receive(kernel_net_dev_t* dev, void* data, size_t len) {
	net_buf_t* buf = net_buf_alloc(len);
	if (!buf) return;

	memcpy(buf->data, data, len);
	buf->len = len;

	if (buf->len < sizeof(eth_header_t)) {
		net_buf_free(buf);
		return;
	}

	eth_header_t* eth = (eth_header_t*)net_buf_pull(buf, sizeof(eth_header_t));
	uint16_t type = NTOHS(eth->type);

	if (type == ETH_TYPE_ARP) {
		arp_handle(dev, buf);
	} else if (type == ETH_TYPE_IPV4) {
		ipv4_handle(dev, buf);
	}

	net_buf_free(buf);
}

void eth_send(kernel_net_dev_t* dev, net_buf_t* buf, uint8_t* dest_mac, uint16_t type) {
	eth_header_t* header = (eth_header_t*)net_buf_push(buf, sizeof(eth_header_t));

	memcpy(header->dest, dest_mac, 6);
	memcpy(header->src, dev->mac, 6);
	header->type = HTONS(type);

	dev->send_packet(dev, buf->data, buf->len);
}

void eth_init(kernel_net_dev_t* dev) {
	dev->on_received = eth_receive;
}
