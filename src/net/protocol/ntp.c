#include "ntp.h"

#include "net/transport/udp.h"
#include "stdio.h"
#include "mem/mem.h"

ntp_callback_t ntp_update_cb = NULL; // when NTP response is received this will be called

#ifdef DEBUG
static void ntp_debug_callback(ntp_info_t* info) {
	dev_puts(&uart16550_dev, "\n( NTP DEBUG HANDLER )\n");

	dev_puts(&uart16550_dev, "unix TS: ");
	dev_putint(&uart16550_dev, info->utc_timestamp);
	dev_puts(&uart16550_dev, "\n");

	dev_puts(&uart16550_dev, "stratum: ");
	dev_putint(&uart16550_dev, (uint64_t)info->stratum);
	dev_puts(&uart16550_dev, "\n");

	dev_puts(&uart16550_dev,  "ver: ");
	dev_putint(&uart16550_dev, (uint64_t)info->version);
	dev_puts(&uart16550_dev,  "\n");

	dev_puts(&uart16550_dev, "delay: ");
	dev_putint(&uart16550_dev, (uint64_t)info->root_delay);
	dev_puts(&uart16550_dev, " units\n");

	dev_puts(&uart16550_dev, "Leap(S): ");
	dev_puts(&uart16550_dev, info->leap_warning ? "Warning\n" : "None\n");
}
#endif

void ntp_send_request(kernel_net_dev_t* dev,  uint32_t target_ip) {
	ntp_packet_t packet;

	#ifdef DEBUG
		if (ntp_update_cb == NULL) { ntp_update_cb = ntp_debug_callback; }
	#endif

	memset(&packet, 0, sizeof(ntp_packet_t));
	packet.flags = 0x1B;
	udp_send_nosock(dev, target_ip, 123, 123, &packet, sizeof(ntp_packet_t));
}

void ntp_handle(void* data) {
	ntp_packet_t* packet = (ntp_packet_t*)data;
	uint32_t ntp_s = NTOHL(packet->trans_ts_s);

	if (ntp_s == 0) return;

	ntp_info_t info;
	info.utc_timestamp = (uint64_t)ntp_s - NTP_OFFSET;
	info.version	  = (packet->flags >> 3) & 0x07;
	info.leap_warning = (packet->flags >> 6) == 0x01;
	info.stratum	  = packet->stratum;
	info.root_delay   = NTOHL(packet->root_delay);
	if (packet->precision < 0) {
		info.precision_ms = 1000 >> (-packet->precision);
	} else {
		info.precision_ms = 1000 << packet->precision;
	}

	if (ntp_update_cb != NULL) {
		ntp_update_cb(&info);
	}
}
