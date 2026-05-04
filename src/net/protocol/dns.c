#include "dns.h"

#include "net/transport/udp.h"
#include "mem/mem.h"
#include "_time.h"

uint16_t query_id_counter = 0xbeef;
volatile uint32_t dns_response = 0;

static uint8_t* dns_encode_name(uint8_t* dest, const char* src) {
	uint8_t* len_ptr = dest++;
	uint8_t len = 0;

	while (*src) {
		if (*src == '.') {
			*len_ptr = len;
			len = 0;
			len_ptr = dest++;
		} else {
			*dest++ = *src;
			len++;
		}
		src++;
	}
	*len_ptr = len;
	*dest++ = 0;
	return dest;
}

static uint8_t* skip_name(uint8_t* reader) {
	while (*reader) {
		if ((*reader & 0xC0) == 0xC0) {
			return reader + 2;
		}
		reader += *reader + 1;
	}
	return reader + 1;
}

void dns_query(kernel_net_dev_t* dev, uint32_t dns_server, const char* hostname) {
	dns_response = 0;
	uint8_t packet[512];
	dns_header_t* dns = (dns_header_t*)packet;

	query_id_counter++;
	dns->id = HTONS(query_id_counter);
	dns->flags = HTONS(0x0100);
	dns->q_count = HTONS(1);
	dns->ans_count = 0;
	dns->auth_count = 0;
	dns->add_count = 0;

	uint8_t* qname_ptr = packet + sizeof(dns_header_t);
	uint8_t* end_ptr = dns_encode_name(qname_ptr, hostname);

	*(uint16_t*)(void*)end_ptr = HTONS(1); end_ptr += 2;
	*(uint16_t*)(void*)end_ptr = HTONS(1); end_ptr += 2;

	size_t packet_len = (size_t)(end_ptr - packet);
	udp_send(dev, dns_server, 53, 53, packet, packet_len);
}

uint32_t dns_query_blocking(kernel_net_dev_t* dev, uint32_t dns_server, const char* hostname) {
	dns_query(dev, dns_server, hostname);

	uint32_t timeout_ms = 5000;
	uint32_t elapsed = 0;

	while (dns_response == 0) {
		if (elapsed >= timeout_ms) {
			return 0;
		}

		msleep(1);
		elapsed++;
	}

	return dns_response;
}

void dns_handle(void* data) {
	dns_header_t* dns = (dns_header_t*)data;
	if (HTONS(dns->id) != query_id_counter) return;
	if (!(HTONS(dns->flags) & 0x8000)) return;
	if (HTONS(dns->ans_count) == 0) return;

	uint8_t* reader = (uint8_t*)data + sizeof(dns_header_t);

	for (int i = 0; i < HTONS(dns->q_count); i++) {
		reader = skip_name(reader);
		reader += 4;
	}

	for (int i = 0; i < HTONS(dns->ans_count); i++) {
		reader = skip_name(reader);

		uint16_t type;
		memcpy(&type, reader, 2);
		type = HTONS(type);
		reader += 2;

		uint16_t class;
		memcpy(&class, reader, 2);
		class = HTONS(class);
		reader += 2;

		reader += 4;
		uint16_t dlen = HTONS(*(uint16_t*)(void*)reader); reader += 2;

		if (type == 1 && dlen == 4) {
			memcpy((void*)&dns_response, reader, 4);
			return;
		}
		reader += dlen;
	}
}
