#include <stdint.h>
#include "logbuf.h"
#include "ansi_helpers.h"

char logbuf_buffer[LOGBUF_SIZE];

static uint32_t write_pos = 0;
static uint32_t read_pos = 0;
static bool wrapped = false;

void logbuf_putc(char c) {
    logbuf_buffer[write_pos] = c;
    write_pos = (write_pos + 1) % LOGBUF_SIZE;

    if (write_pos == read_pos) {
        read_pos = (read_pos + 1) % LOGBUF_SIZE;
        wrapped = true;
    }
}

void logbuf_write(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        logbuf_putc(str[i]);
    }
}

void logbuf_vwrite(char level, const char *str) {
    logbuf_putc('\033');
    logbuf_putc(level);
    logbuf_write(str);
    logbuf_putc('\034');
}

void logbuf_vputhex(char level, uint64_t val) {
    logbuf_putc('\033');
    logbuf_putc(level);
    logbuf_puthex(val);
    logbuf_putc('\034');
}

void logbuf_puthex(uint64_t val) {
    const char *hex_chars = "0123456789ABCDEF";
    logbuf_write("0x");
    for (int i = 15; i >= 0; i--) {
        logbuf_putc(hex_chars[(val >> (i * 4)) & 0xF]);
    }
}

void logbuf_flush(output_dev_t *target) {
    if (!target || !target->putc) return;

    uint32_t curr = read_pos;
    bool skipping = false;
    const char *active_style = NULL;

    while (curr != write_pos) {
        char c = logbuf_buffer[curr];

        switch (c) {
            case '\033': { // metadata start
                curr = (curr + 1) % LOGBUF_SIZE;
                char level = logbuf_buffer[curr];
                
                bool is_debug = (level == '0');
                skipping = (is_debug && !DEV_CAP_HAS(target, CAP_ON_DEBUG));

                if (!skipping && is_debug) {
                    active_style = GET_ANSI_STYLE(target, ANSI_4B_DEBUG, ANSI_8B_DEBUG, ANSI_24B_DEBUG);
                    if (active_style) {
                        const char *s = active_style;
                        while (*s) target->putc(*s++);
                    }
                }
                break;
            }

            case '\034': // metadata end
                if (active_style) {
                    const char *r = ANSI_RESET;
                    while (*r) target->putc(*r++);
                    active_style = NULL;
                }
                skipping = false;
                break;

            default:
                if (!skipping) {
                    target->putc(c);
                }
                break;
        }

        curr = (curr + 1) % LOGBUF_SIZE;
    }
}

void logbuf_clear(void) {
    read_pos = write_pos;
}