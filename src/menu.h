/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_MENU_H
#define GBCC_MENU_H

#include "constants.h"
#include "input.h"
#include <stdbool.h>
#include <stdint.h>

struct gbcc;

enum GBCC_MENU_ENTRY {
	GBCC_MENU_ENTRY_SAVE_STATE,
	GBCC_MENU_ENTRY_LOAD_STATE,
	GBCC_MENU_ENTRY_AUTOSAVE,
	GBCC_MENU_ENTRY_FRAMEBLEND,
	GBCC_MENU_ENTRY_LINK_CABLE,
	GBCC_MENU_ENTRY_SHADER,
	GBCC_MENU_ENTRY_TURBO_MULT,
	GBCC_MENU_ENTRY_VSYNC,
	GBCC_MENU_ENTRY_INTERLACING,
	GBCC_MENU_ENTRY_FPS_COUNTER,
	GBCC_MENU_ENTRY_CHEATS,
	GBCC_MENU_ENTRY_PALETTE,
	GBCC_MENU_ENTRY_NUM_ENTRIES
};

struct gbcc_menu {
	bool initialised;
	bool show;
	int8_t save_state;
	int8_t load_state;
	unsigned int width;
	unsigned int height;
	enum GBCC_MENU_ENTRY selection;
	char *text;
};

void gbcc_menu_init(struct gbcc *gbc);
void gbcc_menu_destroy(struct gbcc *gbc);
void gbcc_menu_update(struct gbcc *gbc);
void gbcc_menu_process_key(struct gbcc *gbc, enum gbcc_key key);

#endif /* GBCC_MENU_H */
