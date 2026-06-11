#pragma once

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define UNUSED(x) ((void)(x))
#define ALIGN(alignment, size) (((size) + ((alignment) - 1)) & ~((alignment) - 1))
#define GET_CONTAINER(ptr, type, member) ((type *)((uintptr_t)(ptr) - offsetof(type, member)))
#define SMAX(a, b) ((a) > (b) ? (a) : (b))
#define IP_ADDR(a, b, c, d) ((a) | (b << 8) | (c << 16) | (d << 24))

#define BIT_SET(var, mask) ((var) |=  (mask))
#define BIT_CLEAR(var, mask) ((var) &= ~(mask))
#define BIT_CHECK(var, mask) (((var) & (mask)) == (mask))
#define BIT_TOGGLE(var, mask) ((var) ^=  (mask))

// unit conversion
#define BYTES_TO_KB(bytes) ((bytes) / 1024UL)
#define BYTES_TO_MB(bytes) ((bytes) / (1024UL * 1024UL))
#define BYTES_TO_GB(bytes) ((bytes) / (1024UL * 1024UL * 1024UL))
#define KB_TO_BYTES(x) ((x) * 1024UL)
#define MB_TO_BYTES(x) ((x) * 1024UL * 1024UL)
#define GB_TO_BYTES(x) ((x) * 1024UL * 1024UL * 1024UL)

// ANSI (based on https://gist.github.com/JBlond/2fea43a3049b38287e5e9cefc87b2124)
#define ESC "\033"
#define ANSI_RESET ESC "[0m"

// regular
#define ANSI_REG_BLACK ESC "[0;30m"
#define ANSI_REG_RED ESC "[0;31m"
#define ANSI_REG_GREEN ESC "[0;32m"
#define ANSI_REG_YELLOW ESC "[0;33m"
#define ANSI_REG_BLUE ESC "[0;34m"
#define ANSI_REG_PURPLE ESC "[0;35m"
#define ANSI_REG_CYAN ESC "[0;36m"
#define ANSI_REG_WHITE ESC "[0;37m"

// bold
#define ANSI_BOLD_BLACK ESC "[1;30m"
#define ANSI_BOLD_RED ESC "[1;31m"
#define ANSI_BOLD_GREEN ESC "[1;32m"
#define ANSI_BOLD_YELLOW ESC "[1;33m"
#define ANSI_BOLD_BLUE ESC "[1;34m"
#define ANSI_BOLD_PURPLE ESC "[1;35m"
#define ANSI_BOLD_CYAN ESC "[1;36m"
#define ANSI_BOLD_WHITE ESC "[1;37m"

// underline
#define ANSI_UND_BLACK ESC "[4;30m"
#define ANSI_UND_RED ESC "[4;31m"
#define ANSI_UND_GREEN ESC "[4;32m"
#define ANSI_UND_YELLOW ESC "[4;33m"
#define ANSI_UND_BLUE ESC "[4;34m"
#define ANSI_UND_PURPLE ESC "[4;35m"
#define ANSI_UND_CYAN ESC "[4;36m"
#define ANSI_UND_WHITE ESC "[4;37m"

// background
#define ANSI_BG_BLACK ESC "[40m"
#define ANSI_BG_RED ESC "[41m"
#define ANSI_BG_GREEN ESC "[42m"
#define ANSI_BG_YELLOW ESC "[43m"
#define ANSI_BG_BLUE ESC "[44m"
#define ANSI_BG_PURPLE ESC "[45m"
#define ANSI_BG_CYAN ESC "[46m"
#define ANSI_BG_WHITE ESC "[47m"

// high intensity
#define ANSI_HI_BLACK ESC "[0;90m"
#define ANSI_HI_RED ESC "[0;91m"
#define ANSI_HI_GREEN ESC "[0;92m"
#define ANSI_HI_YELLOW ESC "[0;93m"
#define ANSI_HI_BLUE ESC "[0;94m"
#define ANSI_HI_PURPLE ESC "[0;95m"
#define ANSI_HI_CYAN ESC "[0;96m"
#define ANSI_HI_WHITE ESC "[0;97m"

// bold high intensity
#define ANSI_BHI_BLACK ESC "[1;90m"
#define ANSI_BHI_RED ESC "[1;91m"
#define ANSI_BHI_GREEN ESC "[1;92m"
#define ANSI_BHI_YELLOW ESC "[1;93m"
#define ANSI_BHI_BLUE ESC "[1;94m"
#define ANSI_BHI_PURPLE ESC "[1;95m"
#define ANSI_BHI_CYAN ESC "[1;96m"
#define ANSI_BHI_WHITE ESC "[1;97m"

// high intensity backgrounds
#define ANSI_BHG_BLACK ESC "[0;100m"
#define ANSI_BHG_RED ESC "[0;101m"
#define ANSI_BHG_GREEN ESC "[0;102m"
#define ANSI_BHG_YELLOW ESC "[0;103m"
#define ANSI_BHG_BLUE ESC "[0;104m"
#define ANSI_BHG_PURPLE ESC "[0;105m"
#define ANSI_BHG_CYAN ESC "[0;106m"
#define ANSI_BHG_WHITE ESC "[0;107m"
