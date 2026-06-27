#pragma once

#include "graphics/fb.h"
#include "devices/char.h"

#define ANSI_MAX_PARAMS 16
#define CUTERM_CHAR_WIDTH 8
#define CUTERM_CHAR_HEIGHT 16

typedef enum {
	ANSI_STATE_NORMAL,
	ANSI_STATE_ESC,
	ANSI_STATE_CSI
} ansi_state_t;

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} rgb_t;

typedef struct {
	linear_framebuffer_t fb_info;
	int cursor_x;
	int cursor_y;

	ansi_state_t ansi_state;
	int ansi_params[ANSI_MAX_PARAMS];
	int ansi_param_count;
	int ansi_cur_param;
	bool ansi_has_digit;

	uint8_t cur_fg_r;
	uint8_t cur_fg_g;
	uint8_t cur_fg_b;

	uint8_t cur_bg_r;
	uint8_t cur_bg_g;
	uint8_t cur_bg_b;
} cuterm_state_t;

void cuterm_init(linear_framebuffer_t fb);
void cuterm_putc(char c);

extern kernel_char_dev_t cuterm_dev;
