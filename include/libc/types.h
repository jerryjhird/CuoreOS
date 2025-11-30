#ifndef TYPES_H
#define TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "types.h"

typedef struct {
    uint8_t sec, min, hour;
    uint8_t day, month;
    uint16_t year;
} datetime_st;

#ifdef __cplusplus
}
#endif

#endif // TYPES_H