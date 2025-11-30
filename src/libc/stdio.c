#include "flanterm.h"
#include "string.h"
#include "stdio.h"
#include "stdint.h"
#include "x86.h"
#include "types.h"

void write(void* ft_ctx, const char* msg, size_t len) {
    flanterm_write(ft_ctx, msg, len);
}

void flush(buffer* ctx) {
    if (ctx->len > 0) {
        write(ctx->ft_ctx, ctx->buf, ctx->len);
        ctx->len = 0;
    }
}

void bwrite(buffer* ctx, void* ft_ctx, const char* msg) {
    if (ctx->ft_ctx == NULL) {
        ctx->ft_ctx = ft_ctx;
        ctx->len = 0;
    }

    size_t msg_len = strlen(msg);
    size_t offset = 0;

    // Handle message in chunks so buffer is never overflowed
    while (offset < msg_len) {
        size_t space = BUF_SIZE - ctx->len;
        size_t chunk = (msg_len - offset < space) ? (msg_len - offset) : space;

        memcpy(ctx->buf + ctx->len, msg + offset, chunk);
        ctx->len += chunk;
        offset += chunk;

        if (ctx->len == BUF_SIZE) {
            flush(ctx);
        }
    }
}

void klog(buffer *buf_ctx, struct flanterm_context *ft_ctx, const char *msg) {
    char decbuf[12];
    char s1[] = "[";
    char s2[] = "] ";
    char s3[] = "\n";

    datetime_st now = get_datetime();
    uint32_t epoch = datetime_to_epoch(now);

    u32dec(decbuf, epoch);

    bwrite(buf_ctx, ft_ctx, s1);
    bwrite(buf_ctx, ft_ctx, decbuf);
    bwrite(buf_ctx, ft_ctx, s2);
    bwrite(buf_ctx, ft_ctx, msg);
    bwrite(buf_ctx, ft_ctx, s3);
}
