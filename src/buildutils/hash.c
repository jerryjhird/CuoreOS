#include <stdio.h>
#include <stdlib.h>

unsigned int hash(const char *s) {
    unsigned int h = 0;
    while (*s) {
        h = h * 31 + (unsigned char)(*s++);
    }
    return h;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        unsigned int h = hash(argv[i]);
        printf("\"%s\" = 0x%08X\n", argv[i], h);
    }

    return 0;
}
