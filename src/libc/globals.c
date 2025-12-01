#include "global.h"
#include "x86.h"

struct flanterm_context *g_ft_ctx = NULL;
char g_writebuf[BUF_SIZE];
size_t g_writebuf_len = 0;
