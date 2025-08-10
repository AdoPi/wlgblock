/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "gbcc.h"
#include "debug.h"
#include "input.h"
#include "menu.h"
#include "nelem.h"
#include <stdio.h>
#include <string.h>

static void toggle_option(struct gbcc *gbc, enum gbcc_key key);
static const char *bool2str(bool b);
static char selected(struct gbcc_menu *menu, enum GBCC_MENU_ENTRY entry);
static int modulo(int x, int n);
static void turbo_index(struct gbcc *gbc, bool inc);

static const float turbo_speeds[] = {0, 0.25f, 0.5f, 1.5f, 2, 3, 4, 5, 10};

void gbcc_menu_init(struct gbcc *gbc)
{
	struct gbcc_menu *menu = &gbc->menu;
	*menu = (struct gbcc_menu){0};

	menu->save_state = 1;
	menu->load_state = 1;

	menu->width = GBC_SCREEN_WIDTH / gbc->window.font.tile_width;
	menu->height = GBC_SCREEN_HEIGHT / gbc->window.font.tile_height;
	menu->text = calloc(menu->width * menu->height + 1, 1);
	menu->initialised = true;
}

void gbcc_menu_destroy(struct gbcc *gbc)
{
	struct gbcc_menu *menu = &gbc->menu;
	if (!menu->initialised) {
		gbcc_log_error("Menu not initialised!\n");
		return;
	}
	free(menu->text);
	menu->initialised = false;
}

void gbcc_menu_update(struct gbcc *gbc)
{
	struct gbcc_menu *menu = &gbc->menu;
	if (!menu->initialised) {
		gbcc_log_error("Menu not initialised!\n");
		return;
	}
	if (!gbc->core.initialised) {
		return;
	}

	const char *link_text;
	switch (gbc->core.link_cable.state) {
		case GBCC_LINK_CABLE_STATE_DISCONNECTED:
			link_text = "Disconnected";
			break;
		case GBCC_LINK_CABLE_STATE_LOOPBACK:
			link_text = "Loopback";
			break;
		case GBCC_LINK_CABLE_STATE_PRINTER:
			link_text = "Printer";
			break;
		default:
			link_text = "";
			break;
	}

	char turbo_text[10];
	if (gbc->turbo_speed == 0) {
		strncpy(turbo_text, "Unlimited", sizeof(turbo_text));
	} else {
		snprintf(turbo_text, sizeof(turbo_text), "%gx", gbc->turbo_speed);
	}

	snprintf(menu->text, menu->width * menu->height + 1,
			"%cSave state:  1 2 3 4 5 6 7 8 9 "
			"%cLoad state:  1 2 3 4 5 6 7 8 9 "
			"%cAutosave:    %-18s"
			"%cFrameblend:  %-18s"
			"%cLink cable:  %-18s"
			"%cShader:      %-18s"
			"%cTurbo mult:  %-18s"
			"%cVsync:       %-18s"
			"%cInterlacing: %-18s"
			"%cFPS counter: %-18s"
			"%cCheats:      %-18s"
			"%cPalette:     %-18s",
			selected(menu, GBCC_MENU_ENTRY_SAVE_STATE),
			selected(menu, GBCC_MENU_ENTRY_LOAD_STATE),
			selected(menu, GBCC_MENU_ENTRY_AUTOSAVE),
			bool2str(gbc->autosave),
			selected(menu, GBCC_MENU_ENTRY_FRAMEBLEND),
			bool2str(gbc->frame_blending),
			selected(menu, GBCC_MENU_ENTRY_LINK_CABLE),
			link_text,
			selected(menu, GBCC_MENU_ENTRY_SHADER),
			gbc->window.gl.shaders[gbc->window.gl.cur_shader].name,
			selected(menu, GBCC_MENU_ENTRY_TURBO_MULT),
			turbo_text,
			selected(menu, GBCC_MENU_ENTRY_VSYNC),
			bool2str(gbc->core.sync_to_video),
			selected(menu, GBCC_MENU_ENTRY_INTERLACING),
			bool2str(gbc->interlacing),
			selected(menu, GBCC_MENU_ENTRY_FPS_COUNTER),
			bool2str(gbc->show_fps),
			selected(menu, GBCC_MENU_ENTRY_CHEATS),
			bool2str(gbc->core.cheats.enabled),
			selected(menu, GBCC_MENU_ENTRY_PALETTE),
			gbc->core.ppu.palette.name
	);
}

void gbcc_menu_process_key(struct gbcc *gbc, enum gbcc_key key)
{
	struct gbcc_menu *menu = &gbc->menu;

	switch (key) {
		case GBCC_KEY_UP:
			if (menu->selection == 0) {
				menu->selection = GBCC_MENU_ENTRY_NUM_ENTRIES;
			}
			menu->selection--;
			break;
		case GBCC_KEY_DOWN:
			if (menu->selection == GBCC_MENU_ENTRY_NUM_ENTRIES - 1) {
				menu->selection = 0;
			} else {
				menu->selection++;
			}
			break;
		case GBCC_KEY_LEFT:
		case GBCC_KEY_RIGHT:
		case GBCC_KEY_A:
		case GBCC_KEY_START:
			toggle_option(gbc, key);
			break;
		case GBCC_KEY_MENU:
		case GBCC_KEY_B:
			menu->show = false;
			break;
		default:
			break;
	}
	gbcc_menu_update(gbc);
}

void toggle_option(struct gbcc *gbc, enum gbcc_key key)
{
	struct gbcc_menu *menu = &gbc->menu;

	switch (menu->selection) {
		case GBCC_MENU_ENTRY_SAVE_STATE:
			if (key == GBCC_KEY_LEFT) {
				menu->save_state--;
			} else if (key == GBCC_KEY_RIGHT) {
				menu->save_state++;
			} else {
				gbc->save_state = menu->save_state;
				menu->show = false;
			}
			menu->save_state = (int8_t)modulo(menu->save_state - 1, 9) + 1;
			break;
		case GBCC_MENU_ENTRY_LOAD_STATE:
			if (key == GBCC_KEY_LEFT) {
				menu->load_state--;
			} else if (key == GBCC_KEY_RIGHT) {
				menu->load_state++;
			} else {
				gbc->load_state = menu->load_state;
				menu->show = false;
			}
			menu->load_state = (int8_t)modulo(menu->load_state - 1, 9) + 1;
			break;
		case GBCC_MENU_ENTRY_AUTOSAVE:
			gbc->autosave ^= 1;
			break;
		case GBCC_MENU_ENTRY_FRAMEBLEND:
			gbc->frame_blending ^= 1;
			break;
		case GBCC_MENU_ENTRY_LINK_CABLE:
			if (key == GBCC_KEY_LEFT) {
				gbc->core.link_cable.state--;
			} else {
				gbc->core.link_cable.state++;
			}
			if (gbc->core.link_cable.state < 0) {
				gbc->core.link_cable.state += GBCC_LINK_CABLE_STATE_NUM_STATES;
			}
			gbc->core.link_cable.state %= GBCC_LINK_CABLE_STATE_NUM_STATES;
			break;
		case GBCC_MENU_ENTRY_SHADER:
			if (key == GBCC_KEY_LEFT) {
				gbc->window.gl.cur_shader--;
			} else {
				gbc->window.gl.cur_shader++;
			}
			gbc->window.gl.cur_shader = modulo(
					gbc->window.gl.cur_shader,
					N_ELEM(gbc->window.gl.shaders));
			break;
		case GBCC_MENU_ENTRY_TURBO_MULT:
			if (key == GBCC_KEY_LEFT) {
				turbo_index(gbc, false);
			} else {
				turbo_index(gbc, true);
			}
			break;
		case GBCC_MENU_ENTRY_VSYNC:
			gbc->core.sync_to_video ^= 1;
			break;
		case GBCC_MENU_ENTRY_INTERLACING:
			gbc->interlacing ^= 1;
			break;
		case GBCC_MENU_ENTRY_FPS_COUNTER:
			gbc->show_fps ^= 1;
			break;
		case GBCC_MENU_ENTRY_PALETTE:
			{
				unsigned int p_idx = gbcc_get_palette_index(gbc->core.ppu.palette.name);
				if (key == GBCC_KEY_LEFT) {
					p_idx = p_idx + GBCC_NUM_PALETTES - 1;
				} else {
					p_idx++;
				}
				gbc->core.ppu.palette = gbcc_get_palette_by_index(p_idx % GBCC_NUM_PALETTES);
			}
			break;
		case GBCC_MENU_ENTRY_CHEATS:
			gbc->core.cheats.enabled ^= 1;
			break;
		case GBCC_MENU_ENTRY_NUM_ENTRIES:
			break;
	}
}

const char *bool2str(bool b)
{
	return b ? "On" : "Off";
}

char selected(struct gbcc_menu *menu, enum GBCC_MENU_ENTRY entry)
{
	return menu->selection == entry ? '>' : ' ';
}

int modulo(int x, int n)
{
	return (x % n + abs(n)) % n;
}

void turbo_index(struct gbcc *gbc, bool inc)
{
	const float mult = gbc->turbo_speed;
	const int len = N_ELEM(turbo_speeds);
	int idx;
	if (inc) {
		for (idx = len - 1; idx > 0 && turbo_speeds[idx] > mult; idx--);
		idx++;
	} else {
		for (idx = 0; idx < len && turbo_speeds[idx] < mult; idx++);
		idx--;
	}
	gbc->turbo_speed = turbo_speeds[modulo(idx, len)];
}
