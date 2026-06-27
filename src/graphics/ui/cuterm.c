#include "cuterm.h"
#include "cuterm_font.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "graphics/fb.h"
#include "mem/mem.h"

static const rgb_t ansi_basic_colors[16] = {
	{0, 0, 0},
	{205, 0, 0},
	{0, 205, 0},
	{205, 205, 0},
	{0, 0, 238},
	{205, 0, 205},
	{0, 205, 205},
	{229, 229, 229},
	{127, 127, 127},
	{255, 0, 0},
	{0, 255, 0},
	{255, 255, 0},
	{92, 92, 255},
	{255, 0, 255},
	{0, 255, 255},
	{255, 255, 255}
};

static const uint8_t ansi_cube_levels[6] = {0, 95, 135, 175, 215, 255};

static cuterm_state_t g_cuterm_state = {
	.ansi_state = ANSI_STATE_NORMAL,
	.cur_fg_r = 255,
	.cur_fg_g = 255,
	.cur_fg_b = 255,
	.cur_bg_r = 0,
	.cur_bg_g = 0,
	.cur_bg_b = 0,
};

kernel_char_dev_t cuterm_dev = {
	CHAR_IS_ERROR_HANDLER,
	cuterm_putc,
	false
};

void cuterm_init(linear_framebuffer_t fb) {
	g_cuterm_state.fb_info = fb;
}

static uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) {
	return ((r >> (8 - g_cuterm_state.fb_info.pixelf.red_size)) << g_cuterm_state.fb_info.pixelf.red_shift) | ((g >> (8 - g_cuterm_state.fb_info.pixelf.green_size)) << g_cuterm_state.fb_info.pixelf.green_shift) | ((b >> (8 - g_cuterm_state.fb_info.pixelf.blue_size)) << g_cuterm_state.fb_info.pixelf.blue_shift);
}

// TODO: optimize me :3
static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
	uint8_t* pixel_ptr = (uint8_t*)g_cuterm_state.fb_info.address + (y * g_cuterm_state.fb_info.pitch) + (x * (g_cuterm_state.fb_info.bpp / 8));
	uint32_t color = pack_color(r, g, b);

	if (g_cuterm_state.fb_info.bpp == 32) {
		memcpy(pixel_ptr, &color, sizeof(uint32_t));
	} else if (g_cuterm_state.fb_info.bpp == 24) {
		pixel_ptr[0] = (uint8_t)(color & 0xFF);
		pixel_ptr[1] = (uint8_t)((color >> 8) & 0xFF);
		pixel_ptr[2] = (uint8_t)((color >> 16) & 0xFF);
	}
}

static void clear_rows(int y_start, int height_px) {
	if (height_px <= 0) return;

	uint32_t color = pack_color(g_cuterm_state.cur_bg_r, g_cuterm_state.cur_bg_g, g_cuterm_state.cur_bg_b);
	uint8_t* base = (uint8_t*)g_cuterm_state.fb_info.address + (size_t)y_start * g_cuterm_state.fb_info.pitch;

	for (uint32_t y = 0; y < (uint32_t)height_px; y++) {
		uint8_t* row = base + (size_t)y * g_cuterm_state.fb_info.pitch;

		if (g_cuterm_state.fb_info.bpp == 32) {
			for (uint32_t x = 0; x < g_cuterm_state.fb_info.width; x++) {
				memcpy(row + (x * 4), &color, sizeof(uint32_t));
			}
		} else if (g_cuterm_state.fb_info.bpp == 24) {
			uint8_t b0 = color & 0xFF;
			uint8_t b1 = (color >> 8) & 0xFF;
			uint8_t b2 = (color >> 16) & 0xFF;
			for (uint32_t x = 0; x < g_cuterm_state.fb_info.width; x++) {
				row[x * 3 + 0] = b0;
				row[x * 3 + 1] = b1;
				row[x * 3 + 2] = b2;
			}
		}
	}
}

static void cuterm_scroll(void) {
	size_t row_bytes = (size_t)g_cuterm_state.fb_info.pitch * CUTERM_CHAR_HEIGHT;
	size_t total_bytes = (size_t)g_cuterm_state.fb_info.pitch * g_cuterm_state.fb_info.height;
	uint8_t* base = (uint8_t*)g_cuterm_state.fb_info.address;
	memmove(base, base + row_bytes, total_bytes - row_bytes);
	clear_rows((int)g_cuterm_state.fb_info.height - CUTERM_CHAR_HEIGHT, CUTERM_CHAR_HEIGHT);
}

static void cuterm_newline(void) {
	g_cuterm_state.cursor_y++;
	int rows = g_cuterm_state.fb_info.height / CUTERM_CHAR_HEIGHT;

	if (g_cuterm_state.cursor_y >= rows) {
		cuterm_scroll();
		g_cuterm_state.cursor_y = rows - 1;
	}
}

static rgb_t ansi_256_to_rgb(uint8_t code) {
	if (code < 16) {
		return ansi_basic_colors[code];
	}

	if (code >= 232) {
		uint8_t v = (uint8_t)(8 + (code - 232) * 10);
		return (rgb_t){v, v, v};
	}

	uint8_t idx = (uint8_t)(code - 16);
	rgb_t color;

	color.r = ansi_cube_levels[idx / 36];
	color.g = ansi_cube_levels[(idx / 6) % 6];
	color.b = ansi_cube_levels[idx % 6];

	return color;
}

static void ansi_reset_colors(void) {
	g_cuterm_state.cur_fg_r = 200;
	g_cuterm_state.cur_fg_g = 200;
	g_cuterm_state.cur_fg_b = 200;

	g_cuterm_state.cur_bg_r = 0;
	g_cuterm_state.cur_bg_g = 0;
	g_cuterm_state.cur_bg_b = 0;
}

static void ansi_apply_sgr(void) {
	if (g_cuterm_state.ansi_param_count == 0) {
		ansi_reset_colors();
		return;
	}

	for (int i = 0; i < g_cuterm_state.ansi_param_count; i++) {
		int p = g_cuterm_state.ansi_params[i];
		if (p == 0) {
			ansi_reset_colors();
		} else if (p >= 30 && p <= 37) {
			rgb_t c = ansi_basic_colors[p - 30];
			g_cuterm_state.cur_fg_r = c.r;
			g_cuterm_state.cur_fg_g = c.g;
			g_cuterm_state.cur_fg_b = c.b;
		} else if (p == 38 && i + 2 < g_cuterm_state.ansi_param_count && g_cuterm_state.ansi_params[i + 1] == 5) {
			// 256 color fg
			rgb_t c = ansi_256_to_rgb((uint8_t)g_cuterm_state.ansi_params[i + 2]);
			g_cuterm_state.cur_fg_r = c.r;
			g_cuterm_state.cur_fg_g = c.g;
			g_cuterm_state.cur_fg_b = c.b;
			i += 2;
		} else if (p == 39) {
			g_cuterm_state.cur_fg_r = 255;
			g_cuterm_state.cur_fg_g = 255;
			g_cuterm_state.cur_fg_b = 255;
		} else if (p >= 40 && p <= 47) {
			rgb_t c = ansi_basic_colors[p - 40];
			g_cuterm_state.cur_bg_r = c.r;
			g_cuterm_state.cur_bg_g = c.g;
			g_cuterm_state.cur_bg_b = c.b;
		} else if (p == 48 && i + 2 < g_cuterm_state.ansi_param_count && g_cuterm_state.ansi_params[i + 1] == 5) {
			// 256 color bg
			rgb_t c = ansi_256_to_rgb((uint8_t)g_cuterm_state.ansi_params[i + 2]);
			g_cuterm_state.cur_bg_r = c.r;
			g_cuterm_state.cur_bg_g = c.g;
			g_cuterm_state.cur_bg_b = c.b;
			i += 2;
		} else if (p == 49) {
			g_cuterm_state.cur_bg_r = 0;
			g_cuterm_state.cur_bg_g = 0;
			g_cuterm_state.cur_bg_b = 0;
		} else if (p >= 90 && p <= 97) {
			rgb_t c = ansi_basic_colors[p - 90 + 8];
			g_cuterm_state.cur_fg_r = c.r;
			g_cuterm_state.cur_fg_g = c.g;
			g_cuterm_state.cur_fg_b = c.b;
		} else if (p >= 100 && p <= 107) {
			rgb_t c = ansi_basic_colors[p - 100 + 8];
			g_cuterm_state.cur_bg_r = c.r;
			g_cuterm_state.cur_bg_g = c.g;
			g_cuterm_state.cur_bg_b = c.b;
		}
	}
}

static void ansi_push_param(void) {
	if (g_cuterm_state.ansi_param_count < ANSI_MAX_PARAMS) {
		g_cuterm_state.ansi_params[g_cuterm_state.ansi_param_count] = g_cuterm_state.ansi_has_digit ? g_cuterm_state.ansi_cur_param : 0;
		g_cuterm_state.ansi_param_count++;
	}

	g_cuterm_state.ansi_cur_param = 0;
	g_cuterm_state.ansi_has_digit = false;
}

static bool ansi_feed(char c) {
	if (g_cuterm_state.ansi_state == ANSI_STATE_NORMAL) {
		if (c == 0x1b) {
			g_cuterm_state.ansi_state = ANSI_STATE_ESC;
			return true;
		}
		return false;
	}

	if (g_cuterm_state.ansi_state == ANSI_STATE_ESC) {
		if (c == '[') {
			g_cuterm_state.ansi_state = ANSI_STATE_CSI;
			g_cuterm_state.ansi_param_count = 0;
			g_cuterm_state.ansi_cur_param = 0;
			g_cuterm_state.ansi_has_digit = false;
		} else {
			g_cuterm_state.ansi_state = ANSI_STATE_NORMAL;
		}
		return true;
	}

	if (c >= '0' && c <= '9') {
		g_cuterm_state.ansi_cur_param = g_cuterm_state.ansi_cur_param * 10 + (c - '0');
		g_cuterm_state.ansi_has_digit = true;
		return true;
	}

	if (c == ';') {
		ansi_push_param();
		return true;
	}

	ansi_push_param();
	if (c == 'm') {
		ansi_apply_sgr();
	}

	g_cuterm_state.ansi_state = ANSI_STATE_NORMAL;
	return true;
}

static void draw_char(char c) {
	int font_idx = (unsigned char)c * CUTERM_CHAR_HEIGHT;
	int pos_x = g_cuterm_state.cursor_x * CUTERM_CHAR_WIDTH;
	int pos_y = g_cuterm_state.cursor_y * CUTERM_CHAR_HEIGHT;

	for (int y = 0; y < CUTERM_CHAR_HEIGHT; y++) {
		uint8_t row = builtin_font[font_idx + y];
		for (int x = 0; x < CUTERM_CHAR_WIDTH; x++) {
			if ((row >> (7 - x)) & 1) {
				set_pixel(pos_x + x, pos_y + y, g_cuterm_state.cur_fg_r, g_cuterm_state.cur_fg_g, g_cuterm_state.cur_fg_b);
			} else {
				set_pixel(pos_x + x, pos_y + y, g_cuterm_state.cur_bg_r, g_cuterm_state.cur_bg_g, g_cuterm_state.cur_bg_b);
			}
		}
	}
}

void cuterm_putc(char c) {
	if (ansi_feed(c)) { return; }

	if (c == '\n') {
		g_cuterm_state.cursor_x = 0;
		cuterm_newline();
	} else {
		draw_char(c);
		g_cuterm_state.cursor_x++;

		if ((unsigned int)g_cuterm_state.cursor_x >= (g_cuterm_state.fb_info.width / CUTERM_CHAR_WIDTH)) {
			g_cuterm_state.cursor_x = 0;
			cuterm_newline();
		}
	}
}
