#ifndef LIMINEABS_H
#define LIMINEABS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "limine.h"

uint64_t limine_module_count(void);
struct limine_file *limine_get_module(uint64_t index);

#ifdef __cplusplus
}
#endif

#endif // LIMINEABS_H