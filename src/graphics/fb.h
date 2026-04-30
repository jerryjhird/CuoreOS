#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
	void* address; // base address of the buffer
	uint64_t width; // width in pixels
	uint64_t height; // height in pixels
	uint64_t pitch;
	uint16_t bpp; // bits per pixel

	struct {
		uint8_t red_size;
		uint8_t red_shift;
		uint8_t green_size;
		uint8_t green_shift;
		uint8_t blue_size;
		uint8_t blue_shift;
	} pixelf;

	uint8_t memory_model;
} linear_framebuffer_t;

#define GENERIC_FB_FROM_LIMINE_FB(gfb, lfb) \
	do { \
		(gfb)->address = (lfb)->address; \
		(gfb)->width   = (lfb)->width; \
		(gfb)->height  = (lfb)->height; \
		(gfb)->pitch   = (lfb)->pitch; \
		(gfb)->bpp	 = (lfb)->bpp; \
		\
		(gfb)->pixelf.red_size	= (lfb)->red_mask_size; \
		(gfb)->pixelf.red_shift   = (lfb)->red_mask_shift; \
		(gfb)->pixelf.green_size  = (lfb)->green_mask_size; \
		(gfb)->pixelf.green_shift = (lfb)->green_mask_shift;\
		(gfb)->pixelf.blue_size   = (lfb)->blue_mask_size; \
		(gfb)->pixelf.blue_shift  = (lfb)->blue_mask_shift; \
		\
		(gfb)->memory_model = (lfb)->memory_model; \
	} while (0)
