#ifndef KIO_H
#define KIO_H

#ifdef __cplusplus
extern "C" {
#endif

void klog(buffer *buf_ctx, struct flanterm_context *ft_ctx, const char *msg);

#ifdef __cplusplus
}
#endif

#endif // KIO_H