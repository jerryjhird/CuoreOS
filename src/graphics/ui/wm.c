#include "wm.h"
#include "graphics/fb.h"
#include "mem/heap.h"
#include <string.h>

static linear_framebuffer_t* target_fb;
static window_t* window_stack_bottom = NULL;
static uint32_t next_window_id = 1;

static uint32_t wm_process_color(uint8_t r, uint8_t g, uint8_t b) {
	// scale 8 bit color to hardware mask size
	uint32_t red   = (uint32_t)r * ((1 << target_fb->pixelf.red_size) - 1) / 255;
	uint32_t green = (uint32_t)g * ((1 << target_fb->pixelf.green_size) - 1) / 255;
	uint32_t blue  = (uint32_t)b * ((1 << target_fb->pixelf.blue_size) - 1) / 255;

	return (red   << target_fb->pixelf.red_shift)   |
		   (green << target_fb->pixelf.green_shift) |
		   (blue  << target_fb->pixelf.blue_shift);
}

static void wm_clear_screen(void) {
	uint32_t bg_color = wm_process_color(60, 60, 65);
	if (!target_fb || !target_fb->address) return;

	for (uint64_t y = 0; y < target_fb->height; y++) {
		uintptr_t row_addr = (uintptr_t)target_fb->address + (y * target_fb->pitch);
		uint32_t* row32 = (uint32_t*)row_addr;
		uint64_t x = 0;

		if (row_addr % 8 != 0 && target_fb->width > 0) {
			row32[x] = bg_color;
			x++;
		}

		uint64_t double_pixel = ((uint64_t)bg_color << 32) | bg_color;
		for (; x + 1 < target_fb->width; x += 2) {
			memcpy(&row32[x], &double_pixel, sizeof(uint64_t));
		}

		if (x < target_fb->width) {
			row32[x] = bg_color;
		}
	}
}

static void wm_draw_titlebar(window_t* win) {
	if (!win || !target_fb) return;

	uint32_t bar_color = wm_process_color(45, 45, 50);
	uint32_t border_color = wm_process_color(100, 100, 110);

	for (int i = 0; i < WM_TITLEBAR_HEIGHT; i++) {
		uint64_t actual_y = win->y + i;
		if (actual_y >= target_fb->height) break;

		uint32_t* row = (uint32_t*)((uintptr_t)target_fb->address + (actual_y * target_fb->pitch));

		if (win->x >= (int)target_fb->width) continue;

		row += win->x;

		uint64_t draw_width = win->buffer->width;
		if (win->x + draw_width > target_fb->width) {
			draw_width = target_fb->width - win->x;
		}

		uint32_t color = (i == 0) ? border_color : bar_color;
		for (uint64_t j = 0; j < draw_width; j++) {
			row[j] = color;
		}
	}
}

window_t* wm_create_window(int x, int y, int w, int h) {
	window_t* win = malloc(sizeof(window_t));
	if (!win) return NULL;

	linear_framebuffer_t* view = malloc(sizeof(linear_framebuffer_t));
	if (!view) { free(win); return NULL; }

	win->id = next_window_id++;
	win->x = x;
	win->y = y;
	win->buffer = view;

	view->width = w;
	view->height = h;
	view->bpp = target_fb->bpp;
	view->pixelf = target_fb->pixelf;
	view->memory_model = target_fb->memory_model;
	view->pitch = target_fb->pitch;

	uint32_t bpp_bytes = target_fb->bpp / 8;

	view->address = (uint8_t*)target_fb->address +
					((y + WM_TITLEBAR_HEIGHT) * target_fb->pitch) +
					(x * bpp_bytes);

	wm_draw_titlebar(win);

	win->next = NULL;
	win->prev = window_stack_bottom;
	if (window_stack_bottom) window_stack_bottom->next = win;
	window_stack_bottom = win;

	return win;
}

void wm_init(linear_framebuffer_t* gfb) {
	target_fb = gfb;
	wm_clear_screen();
}
