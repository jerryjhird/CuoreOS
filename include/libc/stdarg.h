#ifndef STDARG_H
#define STDARG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef char* va_list;

#define va_start(ap, last) (ap = (va_list)&last + sizeof(last))
#define va_arg(ap, type) (*(type *)((ap += sizeof(type)) - sizeof(type)))
#define va_end(ap) (ap = (va_list)0)

#ifdef __cplusplus
}
#endif

#endif // STDARG_H