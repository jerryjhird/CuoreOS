#include "net.h"
#include "ip/icmp.h"
#include "devices.h"
#include "arp.h"
#include "logbuf.h"
#include "_time.h"
#include "mem/mem.h"
#include "transport/tcp.h"

static void ping_recv_handler(uint32_t ip, uint16_t seq, uint8_t ttl, size_t len) {
	logbuf_printf("%d bytes from %I: icmp_seq=%d ttl=%d\n", len, &ip, seq, ttl);
}

static void qotd_callback(uint8_t* data, size_t len) { for (size_t i = 0; i < len; i++) { uart16550_dev.putc((char)data[i]); } dev_puts(&uart16550_dev, "\n"); }
void QOTDSend(kernel_net_dev_t* dev, uint32_t dest_ip) {
	g_tcp.on_data = qotd_callback;
	tcp_connect(dev, dest_ip, 17);
}

static void telnet_callback(uint8_t* data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (data[i] == 0xFF) {
			i += 2;
			continue;
		}

		uart16550_dev.putc(data[i]);
	}
}

void telnet_client(kernel_net_dev_t* dev, uint32_t dest_ip) {
	dev_puts(&uart16550_dev, "\033[2J\033[H");
	g_tcp.on_data = telnet_callback;
	tcp_connect(dev, dest_ip, 23);
}

void nping(kernel_net_dev_t* dev, uint32_t target_ip) {
	if (!dev) return;

	bool is_local = ((target_ip & dev->subnet_mask) == (dev->ip_addr & dev->subnet_mask));
	uint32_t arp_target = is_local ? target_ip : dev->gateway;

	g_icmp_reply_handler = ping_recv_handler;
	uint8_t* dest_mac = arp_lookup(dev, arp_target);

	if (!dest_mac) {
		int timeout = 1000;
		while (!(dest_mac = arp_silent_lookup(arp_target)) && timeout > 0) {
			msleep(1);
			timeout--;
		}

		if (!dest_mac) {
			logbuf_printf("PING %I: dest unreachable (ARP timeout)\n", &target_ip);
			return;
		}
	}

	net_buf_t* pb = net_buf_alloc(32);
	memset(pb->data, 0, 32);
	pb->len = 32;

	logbuf_printf("PING %I: 32 bytes \n", &target_ip, &arp_target);

	icmp_send(dev, pb, target_ip, 8, 0);
}
