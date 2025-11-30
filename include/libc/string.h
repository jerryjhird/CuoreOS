#ifndef STRING_H
#define STRING_H

#ifdef __cplusplus
extern "C" {
#endif

size_t strlen(const char* s);
void *memcpy(void *dest, const void *src, size_t n);
void u32dec(char *buf, uint32_t val);

#ifdef __cplusplus
}
#endif

#endif // STRING_H