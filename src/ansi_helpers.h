#pragma once

#include "devicetypes.h"

#define ANSI_RESET "\033[0m"

// 24 bit true color
#define ANSI_24B_DEBUG "\033[1;37;48;2;40;40;40m"
#define ANSI_24B_PANIC "\033[1;91;48;2;40;40;40m"

// 8 bit color
#define ANSI_8B_DEBUG "\033[1;38;5;15;48;5;236m"
#define ANSI_8B_PANIC "\033[1;38;5;9;48;5;236m"

// 4 bit color
#define ANSI_4B_DEBUG "\033[1;37;100m"
#define ANSI_4B_PANIC "\033[1;91;100m"

#define GET_ANSI_STYLE(dev, cap4, cap8, cap24) \
	(DEV_CAP_HAS(dev, CAP_ANSI_24BIT) ? (cap24) : (DEV_CAP_HAS(dev, CAP_ANSI_8BIT) ? (cap8) : (DEV_CAP_HAS(dev, CAP_ANSI_4BIT) ? (cap4) : NULL)))
