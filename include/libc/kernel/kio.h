#ifndef KIO_H
#define KIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "flanterm.h"

void klog(struct writeout_t *wo);
void kpanic(struct writeout_t *wo);

#ifdef __cplusplus
}
#endif

#endif // KIO_H