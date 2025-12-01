#ifndef KIO_H
#define KIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "flanterm.h"

void klog(void);
void kpanic(void);

#ifdef __cplusplus
}
#endif

#endif // KIO_H