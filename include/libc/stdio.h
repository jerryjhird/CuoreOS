#ifndef STDIO_H
#define STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#define BUF_SIZE 128

typedef struct {
    void* ft_ctx;
    char buf[BUF_SIZE];
    size_t len;
} buffer;

void write(void* ft_ctx, const char* msg, size_t len);
void flush(buffer* ctx);
void bwrite(buffer* ctx, void* ft_ctx, const char* msg);

#ifdef __cplusplus
}
#endif

#endif // STDIO_H