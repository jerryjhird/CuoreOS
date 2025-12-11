#include "flanterm.h"
#include "string.h"
#include "stdio.h"
#include "stdint.h"

#include "arch/cwarch.h"
#include "drivers/ps2.h"
#include "time.h"

void write(struct writeout_t *wo, const char *msg, size_t len) {
    if (wo && wo->write) {
        wo->write(msg, len, wo->ctx);
    }
}

void flush(struct writeout_t *wo) {
    if (wo->len > 0 && wo->write) {
        wo->write(wo->buf, wo->len, wo->ctx);
        wo->len = 0;
        wo->buf[0] = '\0';
    }
}

void bwrite(struct writeout_t *wo, const char *msg) {
    if (!wo || !wo->write || !msg) return;

    size_t msg_len = strlen(msg);
    size_t offset = 0;

    while (offset < msg_len) {
        size_t space = BUF_SIZE - wo->len;
        size_t chunk = (msg_len - offset < space) ? (msg_len - offset) : space;

        memcpy(wo->buf + wo->len, msg + offset, chunk);
        wo->len += chunk;
        offset += chunk;

        if (wo->len == BUF_SIZE) {
            flush(wo);
        }
    }
}

void readline(struct writeout_t *wo, char *buf, size_t size) {
    if (size == 0) return;
    if (wo->len > 0) {
        flush(wo);
    }

    size_t pos = 0;
    while (1) {
        char c = ps2_getc();  // read one character

        // stop reading if enter
        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            write(wo, "\n", 1);
            return;
        }

        if ((c == '\b' || c == 127) && pos > 0) {
            pos--;
            const char bs_seq[] = "\b \b";
            write(wo, bs_seq, sizeof(bs_seq) - 1); // erase char
            continue;
        }

        if (pos < size - 1) {
            buf[pos++] = c;
            write(wo, &c, 1);  // echo typed char
        } else {
            // buffer full
            buf[pos] = '\0';
            return;
        }
    }
}

void write_epoch(struct writeout_t *wo) {
    char decbuf[12];

    datetime_t now = getdatetime();
    uint32_t epoch = dttepoch(now);

    u32dec(decbuf, epoch);

    bwrite(wo, "\x1b[35m[");
    bwrite(wo, decbuf);
    bwrite(wo, "]\x1b[0m ");
}

void panic(struct writeout_t *wo) {
    write_epoch(wo);
    bwrite(wo, "\x1b[31m[ FAIL ] KERNEL PANIC\x1b[0m");
    flush(wo);
    halt();
}
