#include "arch/limineabs.h"

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
    .internal_module_count = 0,
    .internal_modules = 0,
    .response = 0
};

uint64_t limine_module_count(void) {
    struct limine_module_response *resp = module_request.response;
    if (resp == NULL) {
        return 0;
    }
    return resp->module_count;
}

struct limine_file *limine_get_module(uint64_t index) {
    struct limine_module_response *resp = module_request.response;
    if (resp == NULL) {
        return NULL;
    }
    if (index >= resp->module_count) {
        return NULL;
    }

    return resp->modules[index];
}