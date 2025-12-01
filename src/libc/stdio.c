#include "flanterm.h"
#include "string.h"
#include "stdio.h"
#include "stdint.h"
#include "stdarg.h"

#include "x86.h"
#include "drivers/ps2.h"
#include "time.h"

void write(const char* msg, size_t len) {
    flanterm_write(g_ft_ctx, msg, len);
}

void flush(void) {
    if (g_writebuf_len > 0) {
        write(g_writebuf, g_writebuf_len);
        g_writebuf_len = 0;
    }
}

void bwrite(const char *msg) {
    if (!g_ft_ctx) return;

    size_t msg_len = strlen(msg);
    size_t offset = 0;

    while (offset < msg_len) {
        size_t space = BUF_SIZE - g_writebuf_len;
        size_t chunk = (msg_len - offset < space) ? (msg_len - offset) : space;

        memcpy((uint8_t*)g_writebuf + g_writebuf_len, (const uint8_t*)msg + offset, chunk);
        g_writebuf_len += chunk;
        offset += chunk;

        if (g_writebuf_len == BUF_SIZE) {
            flush();
        }
    }
}

void readline(char *buf, size_t size) {
    if (size == 0) return;
    if (g_writebuf_len > 0) {
        flush();
    }

    size_t pos = 0;
    while (1) {
        char c = getc();  // read one character

        // stop reading if enter
        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            write("\n", 1);
            return;
        }

        if ((c == '\b' || c == 127) && pos > 0) {
            pos--;
            const char bs_seq[] = "\b \b";
            write(bs_seq, sizeof(bs_seq) - 1); // erase char
            continue;
        }

        if (pos < size - 1) {
            buf[pos++] = c;
            write(&c, 1);  // echo typed char
        } else {
            // buffer full
            buf[pos] = '\0';
            return;
        }
    }
}

void klog(void) {
    char decbuf[12];

    datetime_st now = get_datetime();
    uint32_t epoch = datetime_to_epoch(now);

    u32dec(decbuf, epoch);

    bwrite("\x1b[35m[");
    bwrite(decbuf);
    bwrite("]\x1b[0m ");
}

void kpanic(void) {
    klog();
    bwrite("\x1b[31m[ FAIL ] KERNEL PANIC\x1b[0m");
    flush();
    for (;;) __asm__ volatile("hlt");
}
