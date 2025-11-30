#ifndef MEM_H
#define MEM_H

#ifdef __cplusplus
extern "C" {
#endif

void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

#ifndef ptrhex
void *ptrhex(char *buf, void *ptr); // ptr to hex
#endif

#ifdef __cplusplus
}
#endif

#endif // MEM_H