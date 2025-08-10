/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_PPU_H
#define GBCC_PPU_H

#include "constants.h"
#include "palettes.h"
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>

struct gbcc_core;

struct line_buffer {
	uint32_t colour[GBC_SCREEN_WIDTH];
	uint8_t attr[GBC_SCREEN_WIDTH];
};

struct tile {
	uint8_t hi;
	uint8_t lo;
	uint8_t x;
	uint8_t attr;
};

struct sprite {
	uint8_t x;
	uint8_t y;
	uint16_t address;
	struct tile tile;
	bool loaded;
};

struct ppu {
	uint64_t frame;
	uint32_t clock;
	bool lcd_disable;
	uint8_t bgp[64]; 	/* 8 x 8-byte palettes */
	uint8_t obp[64]; 	/* 8 x 8-byte palettes */
	struct palette palette;
	struct line_buffer bg_line;
	struct line_buffer window_line;
	struct line_buffer sprite_line;
	struct {
		uint32_t *buffer_0;
		uint32_t *buffer_1;
		uint32_t *gbc;
		uint32_t *sdl;
	} screen;
	sem_t vsync_semaphore;

	/* Copies of IOREG data */
	uint8_t scy;
	uint8_t scx;
	uint8_t ly;
	uint8_t lyc;
	uint8_t wy;
	uint8_t wx;
	uint8_t lcdc;

	/* Internal variables */
	bool last_stat;
	uint8_t x;
	uint8_t window_ly;
	uint16_t next_dot;
	uint8_t n_sprites;
	struct sprite sprites[10];
	struct tile bg_tile;
	struct tile window_tile;
};

void gbcc_ppu_clock(struct gbcc_core *gbc);
void gbcc_disable_lcd(struct gbcc_core *gbc);
void gbcc_enable_lcd(struct gbcc_core *gbc);

#endif /* GBCC_PPU_H */
