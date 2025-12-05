#ifndef SERIAL_H
#define SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#define SERIAL_COM1 0x3F8

void serial_init(void);
void serial_write(const char *msg, size_t len);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_H