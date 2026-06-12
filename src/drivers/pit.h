#include <stdint.h>

#define PIT_CHANNEL_0 0x40
#define PIT_CHANNEL_1 0x41
#define PIT_CHANNEL_2 0x42
#define PIT_COMMAND 0x43

#define PIT_CMD_CHANNEL0 0x00
#define PIT_CMD_CHANNEL1 0x40
#define PIT_CMD_CHANNEL2 0x80
#define PIT_CMD_ACCESS_LO_HI 0x30
#define PIT_CMD_MODE_SQUARE 0x06

#define PIT_FREQ 1193182

// system control ports
#define PORT_SYSTEM_CONTROL_A 0x92
#define PORT_SYSTEM_CONTROL_B 0x61

void pit_init(void);
