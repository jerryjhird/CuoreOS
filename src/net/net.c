#include "net.h"
#include "ip/icmp.h"
#include "devices.h"
#include "arp.h"
#include "logbuf.h"
#include "_time.h"
#include "mem/mem.h"

static void ping_recv_handler(uint32_t ip, uint16_t seq, uint8_t ttl, size_t len) {
	logbuf_printf("%d bytes from %I: icmp_seq=%d ttl=%d\n", len, &ip, seq, ttl);
}

void nping(kernel_net_dev_t* dev, uint32_t target_ip) {
	if (!dev) return;

	uint8_t* dest_mac = arp_lookup(dev, target_ip);
	g_icmp_reply_handler = ping_recv_handler;

	if (!dest_mac) {
		int timeout = 200;
		while (!(dest_mac = arp_silent_lookup(target_ip)) && timeout > 0) {
			msleep(1);
			timeout--;
		}
	}

	if (!dest_mac) {
		logbuf_printf("PING %I: dest unreachable (ARP timeout)\n", &target_ip);
		return;
	}

	net_buf_t* pb = net_buf_alloc(32);
	memset(pb->data, 0, 32);
	pb->len = 32;

	logbuf_printf("PING %I: 32 bytes\n", &target_ip);

	icmp_send(dev, pb, target_ip, 8, 0);
}
