#include <stdint.h>
#include "net/netbuf.h"
#include "ipv4.h"

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

typedef struct __attribute__((packed)) {
	uint8_t  type;
	uint8_t  code; // 0 for echo
	uint16_t checksum;
	uint16_t id;
	uint16_t sequence;
} icmp_header_t;

typedef void (*icmp_reply_callback_t)(uint32_t, uint16_t, uint8_t, size_t);
extern icmp_reply_callback_t g_icmp_reply_handler;

void icmp_handle(kernel_net_dev_t* dev, net_buf_t* buf, ipv4_header_t* ip_hdr);
void icmp_send(kernel_net_dev_t* dev, net_buf_t* buf, uint32_t dest_ip, uint8_t type, uint8_t code);
