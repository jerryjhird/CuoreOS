#pragma once

#include "graphics/fb.h"
#include <stdbool.h>

typedef struct window {
	int x, y;
	linear_framebuffer_t* buffer;
	uint32_t id;
	struct window* next;
	struct window* prev;
} window_t;

#define WM_TITLEBAR_HEIGHT 15

void wm_init(linear_framebuffer_t* gfb);
window_t* wm_create_window(int x, int y, int w, int h);
