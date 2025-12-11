// cwarch // current working arch // resolves includes to the arch
// being used to compile the code

#ifndef CWARCH_H
#define CWARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__x86_64__) || defined(_M_X64)
    // x86-64
    #include "arch/x86_64.h"

#elif defined(__riscv) && __riscv_xlen == 64
    // riscv64
#elif defined(__aarch64__)
    // arm64
#elif defined(__loongarch_lp64)
    // loongarch64 (not sure what this is but limine supports it?)
#endif

#ifdef __cplusplus
}
#endif

#endif // CWARCH_H