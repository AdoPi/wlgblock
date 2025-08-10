/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "core.h"
#include "bit_utils.h"
#include "colour.h"
#include "debug.h"
#include "gbcc.h"
#include "memory.h"
#include "palettes.h"
#include "ppu.h"
#include <stdio.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define BACKGROUND_MAP_BANK_1 0x9800u
#define BACKGROUND_MAP_BANK_2 0x9C00u

/* 
 * Guide to line buffer attribute bits:
 * 0 - is this pixel being drawn?
 * 1 - is this colour 0?
 * 2 - priority bit
 */

#define ATTR_DRAWN (bit(0))
#define ATTR_COLOUR0 (bit(1))
#define ATTR_PRIORITY (bit(2))

enum palette_flag { BACKGROUND, SPRITE_1, SPRITE_2 };

static void draw_background_pixel(struct gbcc_core *gbc);
static void draw_window_pixel(struct gbcc_core *gbc);
static void draw_sprite_pixel(struct gbcc_core *gbc);
static void composite_line(struct gbcc_core *gbc);
static uint8_t get_video_mode(uint8_t stat);
static uint8_t set_video_mode(uint8_t stat, uint8_t mode);
static uint32_t get_palette_colour(struct gbcc_core *gbc, uint8_t palette, uint8_t n, enum palette_flag pf);
static void load_bg_tile(struct gbcc_core *gbc);
static void load_window_tile(struct gbcc_core *gbc);
static void load_sprite_tile(struct gbcc_core *gbc, int n);
static uint8_t get_tile_pixel(uint8_t hi, uint8_t lo, uint8_t x, bool flip);

void gbcc_disable_lcd(struct gbcc_core *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	if (gbc->mode == GBC) {
		memset(ppu->screen.sdl, 0xFFu, GBC_SCREEN_SIZE * sizeof(*ppu->screen.buffer_0));
	} else {
		uint32_t colour = get_palette_colour(gbc, 0, 0, BACKGROUND);
		for (int y = 0; y < GBC_SCREEN_HEIGHT; y++) {
			for (int x = 0; x < GBC_SCREEN_WIDTH; x++) {
				ppu->screen.sdl[y * GBC_SCREEN_WIDTH + x] = colour;
			}
		}
		for (int y = 0; y < GBC_SCREEN_HEIGHT; y++) {
			for (int x = 0; x < GBC_SCREEN_WIDTH; x++) {
				ppu->screen.gbc[y * GBC_SCREEN_WIDTH + x] = colour;
			}
		}
	}
	ppu->lcd_disable = true;
	ppu->ly = 0;
	gbcc_memory_write_force(gbc, LY, 0);
	
	uint8_t stat = gbcc_memory_read_force(gbc, STAT);
	if (get_video_mode(stat) != GBC_LCD_MODE_VBLANK) {
		gbcc_log_debug("LCD disabled outside of VBLANK.\n");
	}
	stat = set_video_mode(stat, GBC_LCD_MODE_HBLANK);
	gbcc_memory_write_force(gbc, STAT, stat);
}

void gbcc_enable_lcd(struct gbcc_core *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	if (!ppu->lcd_disable) {
		return;
	}
	ppu->lcd_disable = false;
	ppu->clock = 0;
	/* 
	 * TODO: Mooneye's test roms seem to imply that when enabling the
	 * screen, it starts in HBLANK? Is this true? 
	 * (Currently, it skips the first line of the first frame)
	 */
	ppu->clock = 248;
}

ANDROID_INLINE
void gbcc_ppu_clock(struct gbcc_core *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	if (ppu->lcd_disable) {
		return;
	}
	uint8_t stat = gbcc_memory_read_force(gbc, STAT);

	/* Start of a new scanline */
	if (ppu->clock == 0 && get_video_mode(stat) != GBC_LCD_MODE_VBLANK) {
		/* Clear our buffers */
		memset(ppu->bg_line.attr, 0, sizeof(ppu->bg_line.attr));
		memset(ppu->window_line.attr, 0, sizeof(ppu->window_line.attr));
		memset(ppu->sprite_line.attr, 0, sizeof(ppu->sprite_line.attr));

		/* 
		 * The ppu ignores writes to some variables during a scanline,
		 * so we make copies of them here, and again just before
		 * rendering starts.
		 */
		ppu->scy = gbcc_memory_read_force(gbc, SCY);
		ppu->scx = gbcc_memory_read_force(gbc, SCX);
		ppu->lyc = gbcc_memory_read_force(gbc, LYC);
		ppu->wy = gbcc_memory_read_force(gbc, WY);
		ppu->wx = gbcc_memory_read_force(gbc, WX);
		/*
		 * The ppu doesn't really ignore writes to LCDC during a
		 * scanline, however the behaviour is not well described
		 * anywhere, and various games seem to rely on being able to
		 * enable the window mid-scanline for the next line.
		 */
		ppu->lcdc = gbcc_memory_read_force(gbc, LCDC);

		/*
		 * First off, we enter OAM scanning mode for 80 cycles. As the
		 * OAM is inaccessable by the CPU during this time, we can just
		 * do all the calculation at once and then wait for 79 cycles.
		 */
		stat = set_video_mode(stat, GBC_LCD_MODE_OAM_READ);

		ppu->n_sprites = 0;
		bool double_size = check_bit(ppu->lcdc, 2); /* 8x8 or 8x16 tiles */
		for (uint16_t addr = OAM_START; addr < OAM_END; addr += 4) {
			uint8_t y = gbcc_memory_read_force(gbc, addr);
			uint8_t x = gbcc_memory_read_force(gbc, addr + 1);
			if (ppu->ly >= y || ppu->ly + 16 < y || (!double_size && ppu->ly + 8 >= y)) {
				/* Sprite isn't on this line */
				continue;
			}
			ppu->sprites[ppu->n_sprites].y = y;
			ppu->sprites[ppu->n_sprites].x = x;
			ppu->sprites[ppu->n_sprites].address = addr;
			ppu->sprites[ppu->n_sprites].loaded = false;
			ppu->n_sprites++;
			/* GameBoy can only draw 10 sprites per line */
			if (ppu->n_sprites == 10) {
				break;
			}
		}
	}
	if (ppu->clock == 81 && get_video_mode(stat) != GBC_LCD_MODE_VBLANK) {
		/* Final value of these variables for the rest of this line */
		ppu->scy = gbcc_memory_read_force(gbc, SCY);
		ppu->scx = gbcc_memory_read_force(gbc, SCX);
		ppu->lyc = gbcc_memory_read_force(gbc, LYC);
		ppu->wy = gbcc_memory_read_force(gbc, WY);
		ppu->wx = gbcc_memory_read_force(gbc, WX);
		ppu->lcdc = gbcc_memory_read_force(gbc, LCDC);

		/* Start the actual rendering of this line */
		stat = set_video_mode(stat, GBC_LCD_MODE_OAM_VRAM_READ);
		ppu->x = 0;
		load_bg_tile(gbc);
		/*
		 * Rendering at the beginning of a scanline pauses if
		 * SCX % 8 != 0, while the ppu discards offscreen pixels
		 */
		/*
		 * TODO: As far as I can tell, the magic number here should be
		 * 89, as mode 3 begins at 81 dots, with an 8 dot pause while
		 * the first background tile is fetched. This breaks Mooneye's
		 * intr_2_mode0 test however, which suggests that mode 3 is not
		 * taking long enough.
		 */
		ppu->next_dot = 94 + ppu->scx % 8;
		ppu->bg_tile.x = ppu->scx % 8;
		ppu->window_tile.x = 0;
	}
	if (get_video_mode(stat) == GBC_LCD_MODE_OAM_VRAM_READ) {
		if (ppu->x == 160) {
			stat = set_video_mode(stat, GBC_LCD_MODE_HBLANK);
			composite_line(gbc);
			if (gbc->hdma.hblank && gbc->hdma.length > 0) {
				gbc->hdma.to_copy = 0x10u;
			}
		} else if (ppu->clock == ppu->next_dot) {
			draw_background_pixel(gbc);
			draw_window_pixel(gbc);
			draw_sprite_pixel(gbc);
			ppu->x++;
			ppu->next_dot++;
		}
	}
	
	/* LCDSTAT interrupt */
	//printf("LYC: %u\n", gbcc_memory_read_force(gbc, LYC));
	//printf("Clock = %u\tMode = %u\n", ppu->clock, get_video_mode(stat));
	ppu->clock++;
	if (ppu->clock == 456) {
		ppu->ly++;
		ppu->clock = 0;
	}
	if (ppu->ly == 153 && ppu->clock == 10) {
		/*
		 * Some strangeness here - after a few clocks on line 153, the
		 * gameboy goes back to ly=0, and spends the rest of the
		 * scanline there in VBLANK mode, before resetting.
		 */
		ppu->ly = 0;
	}
	if (ppu->ly == 1 && get_video_mode(stat) == GBC_LCD_MODE_VBLANK) {
		ppu->ly = 0;
		stat = set_video_mode(stat, GBC_LCD_MODE_OAM_READ);
	}

	stat = cond_bit(stat, 2, ppu->ly == ppu->lyc);
	
	/* VBLANK interrupt flag */
	if (ppu->ly == 144 && ppu->clock == 0) {
		gbcc_memory_set_bit(gbc, IF, 0);
		stat = set_video_mode(stat, GBC_LCD_MODE_VBLANK);

		if (gbc->sync_to_video && !gbc->keys.turbo) {
			sem_wait(&ppu->vsync_semaphore);
		}

		uint32_t *tmp = ppu->screen.gbc;
		ppu->screen.gbc = ppu->screen.sdl;
		ppu->screen.sdl = tmp;

		ppu->frame++;

		/*
		 * Apparently, the window "remembers" how many lines it drew
		 * during the frame, so e.g. drawing some window at the top of
		 * the screen, then disabling it, then re-enabling it further
		 * down without changing WY will resume drawing where it left
		 * off, **NOT** from where you'd expect from (LY - WY). This
		 * doesn't seem to be mentioned in any documentation I can
		 * find, but is emulated correctly in many other emulators.
		 *
		 * TODO: Antonio's TCAGBD suggests that there is an entirely
		 * separate background state machine for the window, which
		 * would make sense here, and is worth remembering whenever the
		 * ppu gets rewritten.
		 */
		ppu->window_ly = 0xFFu;
	} 
	/* Check STAT interrupt */
	/* 
	 * The last_stat flag is used as the STAT interrupt is only triggered
	 * when this condition goes from 0->1, so switching between two
	 * different conditions being satisfied in a single cycle will *NOT*
	 * trigger the interrupt.
	 *
	 * N.B. this convoluted logic is for performance reasons - apparently
	 * all this bit-checking is expensive, and this set of checks used to
	 * account for ~4% of *all* of gbcc_emulate_cycle!
	 *
	 * This should be equivalent to the following switch statement:
	 *
	 * bool mode_interrupt = false;
	 * switch (get_video_mode(stat)) {
	 * 	case GBC_LCD_MODE_HBLANK:
	 * 		mode_interrupt = check_bit(stat, 3);
	 * 		break;
	 * 	case GBC_LCD_MODE_VBLANK:
	 * 		mode_interrupt = check_bit(stat, 4);
	 * 		break;
	 * 	case GBC_LCD_MODE_OAM_READ:
	 * 		mode_interrupt = check_bit(stat, 5);
	 * 		break;
	 * }
	 */
	bool mode_interrupt = check_bit(stat, (stat & 0x03u) + 3)
			& ~(check_bit(stat, 0) & check_bit(stat, 1));
	if (mode_interrupt || (check_bit(stat, 2) && check_bit(stat, 6)) /* LY = LYC */) {
		if (!ppu->last_stat) {
			ppu->last_stat = true;
			gbcc_memory_set_bit(gbc, IF, 1);
		}
	} else {
		ppu->last_stat = false;
	}
	gbcc_memory_write_force(gbc, LY, ppu->ly);
	gbcc_memory_write_force(gbc, STAT, stat);
}

/* TODO: GBC BG-to-OAM Priority */
void draw_background_pixel(struct gbcc_core *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	struct tile *t = &ppu->bg_tile;
	if (t->x == 0) {
		load_bg_tile(gbc);
	}

	uint8_t colour = get_tile_pixel(t->hi, t->lo, t->x, check_bit(t->attr, 5));
	uint8_t palette;
	if (gbc->mode == DMG) {
		palette = gbcc_memory_read_force(gbc, BGP);
	} else {
		palette = t->attr & 0x07u;
	}
	ppu->bg_line.colour[ppu->x] = get_palette_colour(gbc, palette, colour, BACKGROUND);

	uint8_t attr = ATTR_DRAWN;
	if (colour == 0) {
		attr |= ATTR_COLOUR0;
	}
	if (check_bit(t->attr, 7)) {
		attr |= ATTR_PRIORITY;
	}
	ppu->bg_line.attr[ppu->x] = attr;
	t->x++;
	t->x %= 8;
}

void draw_window_pixel(struct gbcc_core *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	struct tile *t = &ppu->window_tile;

	if (ppu->ly < ppu->wy || !check_bit(ppu->lcdc, 5) || (gbc->mode == DMG && !check_bit(ppu->lcdc, 0))) {
		return;
	}
	if (ppu->x + 7 < ppu->wx) {
		return;
	}
	/* Should only be true the first window pixel of each line */
	if (ppu->x + MIN(7, ppu->wx) == ppu->wx) {
		ppu->window_ly++;
	}
	if (t->x == 0) {
		load_window_tile(gbc);
		if (ppu->x == 0) {
			/* Skip pixels to make wx=7 be at x=0 */
			t->x += (7 - ppu->wx);
		}
	}

	uint8_t colour = get_tile_pixel(t->hi, t->lo, t->x, check_bit(t->attr, 5));
	uint8_t palette;
	if (gbc->mode == DMG) {
		palette = gbcc_memory_read_force(gbc, BGP);
	} else {
		palette = t->attr & 0x07u;
	}
	ppu->window_line.colour[ppu->x] = get_palette_colour(gbc, palette, colour, BACKGROUND);
	uint8_t attr = ATTR_DRAWN;
	if (colour == 0) {
		attr |= ATTR_COLOUR0;
	}
	if (check_bit(t->attr, 7)) {
		attr |= ATTR_PRIORITY;
	}
	ppu->window_line.attr[ppu->x] = attr;
	t->x++;
	t->x %= 8;
}

void draw_sprite_pixel(struct gbcc_core *gbc)
{
	struct ppu *ppu = &gbc->ppu;
	if (!check_bit(ppu->lcdc, 1)) {
		/* Sprites are disabled */
		return;
	}
	if (gbc->cpu.dma.running) {
		/* Sprites are disabled while DMA is running*/
		return;
	}
	for (int i = 0; i < ppu->n_sprites; i++) {
		struct sprite *s = &ppu->sprites[i];
		if (ppu->x + 8 < s->x || ppu->x >= s->x) {
			continue;
		}
		if (ppu->sprite_line.attr[ppu->x] & ATTR_DRAWN) {
			/* On the GBC, sprites earlier in OAM have priority */
			/* 
			 * TODO: On DMG, the sprite with the lowest x should
			 * have priority
			 */
			continue;
		}
		if (!s->loaded) {
			load_sprite_tile(gbc, i);
			/*
			 * Each new sprite causes a delay in rendering
			 * depending on its position over the background.
			 * TODO: This should take a different amount of time
			 * when drawing over the window - ppu->scx should be
			 * replaced with (255 - ppu->wx)
			 */
			ppu->next_dot += 11 - MIN(5, (ppu->x + ppu->scx) % 8);
		}
		uint8_t x = ppu->x + 8 - s->x;
		uint8_t colour = get_tile_pixel(s->tile.hi, s->tile.lo, x, check_bit(s->tile.attr, 5));
		/* Colour 0 is transparent */
		if (!colour) {
			continue;
		}
		uint8_t palette;
		enum palette_flag pf;
		if (gbc->mode == DMG) {
			if (check_bit(s->tile.attr, 4)) {
				palette = gbcc_memory_read_force(gbc, OBP1);
				pf = SPRITE_1;
			} else {
				palette = gbcc_memory_read_force(gbc, OBP0);
				pf = SPRITE_2;
			}
		} else {
			palette = s->tile.attr & 0x07u;
			pf = SPRITE_1;
		}
		ppu->sprite_line.colour[ppu->x] = get_palette_colour(gbc, palette, colour, pf);
		uint8_t attr = ATTR_DRAWN;
		if (check_bit(s->tile.attr, 7)) {
			attr |= ATTR_PRIORITY;
		}
		ppu->sprite_line.attr[ppu->x] = attr;
	}
}

void composite_line(struct gbcc_core *gbc)
{
	/* 
	 * Composite the line according to various attributes. In order of
	 * increasing priority, these are:
	 * ob_attr & ATTR_PRIORITY: if set, draw sprites below background
	 * 			    colours 1-3
	 * (win|bg)_attr & ATTR_PRIORITY: same as above
	 * bit(ppu->lcdc, 0): TODO: different for DMG & GBC
	 */
	struct ppu *ppu = &gbc->ppu;
	uint8_t ly = ppu->ly;
	uint32_t *line = &ppu->screen.gbc[ly * GBC_SCREEN_WIDTH];

	if (!gbc->hide_background) {
		memcpy(line, ppu->bg_line.colour, GBC_SCREEN_WIDTH * sizeof(line[0]));
	} else {
		memset(line, 0xFFu, GBC_SCREEN_WIDTH * sizeof(line[0]));
	}
	for (uint8_t x = 0; x < GBC_SCREEN_WIDTH; x++) {
		uint8_t bg_attr = ppu->bg_line.attr[x];
		uint8_t win_attr = ppu->window_line.attr[x];
		uint8_t ob_attr = ppu->sprite_line.attr[x];

		if (win_attr & ATTR_DRAWN && !gbc->hide_window) {
			line[x] = ppu->window_line.colour[x];
			if (win_attr & ATTR_PRIORITY && !(win_attr & ATTR_COLOUR0)) {
				continue;
			}
		}

		if (ob_attr & ATTR_DRAWN) {
			if ((win_attr & ATTR_DRAWN) && !(win_attr & ATTR_COLOUR0)) {
				if (win_attr & ATTR_PRIORITY) {
					continue;
				}
				if (ob_attr & ATTR_PRIORITY) {
					continue;
				}
			}
			if ((bg_attr & ATTR_DRAWN) && !(bg_attr & ATTR_COLOUR0)) {
				if (bg_attr & ATTR_PRIORITY) {
					continue;
				}
				if (ob_attr & ATTR_PRIORITY) {
					continue;
				}
			}
			if (gbc->hide_sprites) {
				continue;
			}
			line[x] = ppu->sprite_line.colour[x];
		}
	}
}

uint8_t get_video_mode(uint8_t stat)
{
	return stat & 0x03u;
}

uint8_t set_video_mode(uint8_t stat, uint8_t mode)
{
	stat &= 0xFCu;
	stat |= mode;
	return stat;
}

uint32_t get_palette_colour(struct gbcc_core *gbc, uint8_t palette, uint8_t n, enum palette_flag pf)
{
	struct ppu *ppu = &gbc->ppu;
	if (gbc->mode == DMG) {
		uint8_t colours[4] = {
			(palette & 0x03u) >> 0u,
			(palette & 0x0Cu) >> 2u,
			(palette & 0x30u) >> 4u,
			(palette & 0xC0u) >> 6u
		};
		switch (pf) {
			case BACKGROUND:
				return ppu->palette.background[colours[n]];
			case SPRITE_1:
				return ppu->palette.sprite1[colours[n]];
			case SPRITE_2:
				return ppu->palette.sprite2[colours[n]];
		}
	}
	uint8_t index = palette * 8;
	uint8_t lo;
	uint8_t hi;
	if (pf == BACKGROUND) {
		lo = ppu->bgp[index + 2 * n];
		hi = ppu->bgp[index + 2 * n + 1];
	} else {
		lo = ppu->obp[index + 2 * n];
		hi = ppu->obp[index + 2 * n + 1];
	} 
	uint8_t r = lo & 0x1Fu;
	uint8_t g = ((lo & 0xE0u) >> 5u) | (uint8_t)((hi & 0x03u) << 3u);
	uint8_t b = (hi & 0x7Cu) >> 2u;
	uint32_t res = 0;
	res |= ((uint32_t)r << 27u);
	res |= ((uint32_t)g << 19u);
	res |= ((uint32_t)b << 11u);
	res |= 0xFFu;

	return res;
}

void load_bg_tile(struct gbcc_core *gbc)
{
	struct ppu *ppu = &gbc->ppu;

	ppu->bg_tile.x = 0;
	uint8_t tx = ((ppu->scx + ppu->x) / 8u) % 32u;
	uint8_t ty = ((ppu->scy + ppu->ly) / 8u) % 32u;
	uint16_t line_offset = 2 * ((ppu->scy + ppu->ly) % 8u); /* 2 bytes per line */
	uint16_t map;
	if (check_bit(ppu->lcdc, 3)) {
		map = BACKGROUND_MAP_BANK_2;
	} else {
		map = BACKGROUND_MAP_BANK_1;
	}

	if (gbc->mode == DMG) {
		uint8_t tile = gbcc_memory_read_force(gbc, map + 32 * ty + tx);
		uint16_t tile_addr;
		if (check_bit(ppu->lcdc, 4)) {
			tile_addr = VRAM_START + 16 * tile;
		} else {
			tile_addr = (uint16_t)(0x9000 + 16 * (int8_t)tile);
		}
		ppu->bg_tile.lo = gbcc_memory_read_force(gbc, tile_addr + line_offset);
		ppu->bg_tile.hi = gbcc_memory_read_force(gbc, tile_addr + line_offset + 1);
		ppu->bg_tile.attr = 0;
	} else {
		uint8_t tile = gbc->memory.vram_bank[0][map + 32 * ty + tx - VRAM_START];
		ppu->bg_tile.attr = gbc->memory.vram_bank[1][map + 32 * ty + tx - VRAM_START];
		uint8_t *vbk;
		uint16_t tile_addr;
		if (check_bit(ppu->bg_tile.attr, 3)) {
			vbk = gbc->memory.vram_bank[1];
		} else {
			vbk = gbc->memory.vram_bank[0];
		}
		if (check_bit(ppu->lcdc, 4)) {
			tile_addr = 16 * tile;
		} else {
			tile_addr = (uint16_t)(0x1000 + 16 * (int8_t)tile);
		}
		/* Check for Y-flip */
		if (check_bit(ppu->bg_tile.attr, 6)) {
			ppu->bg_tile.lo = vbk[tile_addr + (14 - line_offset)];
			ppu->bg_tile.hi = vbk[tile_addr + (14 - line_offset) + 1];
		} else {
			ppu->bg_tile.lo = vbk[tile_addr + line_offset];
			ppu->bg_tile.hi = vbk[tile_addr + line_offset + 1];
		}
	}
}

void load_window_tile(struct gbcc_core *gbc)
{
	struct ppu *ppu = &gbc->ppu;

	ppu->window_tile.x = 0;
	uint8_t tx = ((ppu->x - ppu->wx + 7) / 8u) % 32u;
	uint8_t ty = (ppu->window_ly / 8u) % 32u;
	uint16_t line_offset = 2 * (ppu->window_ly % 8u); /* 2 bytes per line */
	uint16_t map;
	if (check_bit(ppu->lcdc, 6)) {
		map = BACKGROUND_MAP_BANK_2;
	} else {
		map = BACKGROUND_MAP_BANK_1;
	}

	if (gbc->mode == DMG) {
		uint8_t tile = gbcc_memory_read_force(gbc, map + 32 * ty + tx);
		uint16_t tile_addr;
		if (check_bit(ppu->lcdc, 4)) {
			tile_addr = VRAM_START + 16 * tile;
		} else {
			tile_addr = (uint16_t)(0x9000 + 16 * (int8_t)tile);
		}
		ppu->window_tile.lo = gbcc_memory_read_force(gbc, tile_addr + line_offset);
		ppu->window_tile.hi = gbcc_memory_read_force(gbc, tile_addr + line_offset + 1);
		ppu->window_tile.attr = 0;
	} else {
		uint8_t tile = gbc->memory.vram_bank[0][map + 32 * ty + tx - VRAM_START];
		ppu->window_tile.attr = gbc->memory.vram_bank[1][map + 32 * ty + tx - VRAM_START];
		uint8_t *vbk;
		uint16_t tile_addr;
		if (check_bit(ppu->window_tile.attr, 3)) {
			vbk = gbc->memory.vram_bank[1];
		} else {
			vbk = gbc->memory.vram_bank[0];
		}
		if (check_bit(ppu->lcdc, 4)) {
			tile_addr = 16 * tile;
		} else {
			tile_addr = (uint16_t)(0x1000 + 16 * (int8_t)tile);
		}
		/* Check for Y-flip */
		if (check_bit(ppu->window_tile.attr, 6)) {
			ppu->window_tile.lo = vbk[tile_addr + (14 - line_offset)];
			ppu->window_tile.hi = vbk[tile_addr + (14 - line_offset) + 1];
		} else {
			ppu->window_tile.lo = vbk[tile_addr + line_offset];
			ppu->window_tile.hi = vbk[tile_addr + line_offset + 1];
		}
	}
}

void load_sprite_tile(struct gbcc_core *gbc, int n)
{
	struct ppu *ppu = &gbc->ppu;
	uint8_t ly = ppu->ly;
	bool double_size = check_bit(ppu->lcdc, 2); /* 1 or 2 8x8 tiles */
	struct tile *t = &ppu->sprites[n].tile;
	/* 
	 * Together SY & SX define the bottom right corner of the
	 * sprite. X=1, Y=1 is the top left corner, so each of these
	 * are offset by 1 for array values.
	 * TODO: Is this off-by-one true?
	 */
	uint8_t sy = ppu->sprites[n].y;
	uint8_t tile = gbcc_memory_read_force(gbc, ppu->sprites[n].address + 2);
	t->attr = gbcc_memory_read_force(gbc, ppu->sprites[n].address + 3);
	bool yflip = check_bit(t->attr, 6);
	uint8_t sprite_line = sy - ly;
	uint8_t *vram_bank;
	if (gbc->mode == DMG) {
		vram_bank = gbc->memory.vram_bank[0];
	} else {
		vram_bank = gbc->memory.vram_bank[check_bit(t->attr, 3)];
	}
	if (double_size) {
		/* 
		 * In 8x16 mode, the lower bit of the tile number is
		 * ignored, and the sprite is stored as upper tile then
		 * lower tile.
		 */
		if (ly + 8 < sy) {
			/* We are drawing the top tile */
			tile = cond_bit(tile, 0, yflip);
		} else {
			/* We are drawing the bottom tile */
			tile = cond_bit(tile, 0, !yflip);
			sprite_line += 8;
		}
	}
	if (yflip) {
		sprite_line -= 9;
	} else {
		sprite_line = 16 - sprite_line;
	}
	t->lo = vram_bank[16 * tile + 2 * sprite_line];
	t->hi = vram_bank[16 * tile + 2 * sprite_line + 1];
	t->x = 0;
	ppu->sprites[n].loaded = true;
}

uint8_t get_tile_pixel(uint8_t hi, uint8_t lo, uint8_t x, bool flip)
{
	if (!flip) {
		x = 7 - x;
	}
	return (uint8_t)(check_bit(hi, x) << 1u) | check_bit(lo, x);
}
