#include <stdint.h>
#include <stddef.h>
#include "devices.h"

#define DNS_CACHE_SIZE 16

typedef struct {
	uint16_t id;
	uint16_t flags;
	uint16_t q_count;
	uint16_t ans_count;
	uint16_t auth_count;
	uint16_t add_count;
} __attribute__((packed)) dns_header_t;

typedef struct {
	char hostname[64];
	uint32_t ip;
	uint64_t expires;
	bool active;
} dns_entry_t;

extern volatile uint32_t dns_response;
extern uint16_t query_id_counter;

void dns_init(void);
uint32_t dns_lookup(const char* hostname);
void dns_query(kernel_net_dev_t* dev, uint32_t dns_server, const char* hostname);
uint32_t dns_query_blocking(kernel_net_dev_t* dev, uint32_t dns_server, const char* hostname);
void dns_handle(void* data);
