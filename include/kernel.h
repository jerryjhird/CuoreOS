// header file reserved for kernel specific functions
// that do not fall into the categories of the other
// header files

#ifndef KERNEL_H
#define KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"

// kernel tests
void memory_test(struct writeout_t *wo);

#ifdef __cplusplus
}
#endif

#endif // KERNEL_H