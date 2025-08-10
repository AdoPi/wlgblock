/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_PALETTE_H
#define GBCC_PALETTE_H

#include <stdbool.h>
#include <stdint.h>

#define GBCC_NUM_PALETTES 14
#define GBCC_MAX_PALETTE_NAME_LEN 16

struct palette {
	char name[GBCC_MAX_PALETTE_NAME_LEN];
	uint32_t background[4];
	uint32_t sprite1[4];
	uint32_t sprite2[4];
};

extern const struct palette gbcc_palettes[GBCC_NUM_PALETTES];

struct palette gbcc_get_palette(const char *name);
struct palette gbcc_get_palette_by_index(unsigned int index);
unsigned int gbcc_get_palette_index(const char *name);

#endif /* GBCC_PALETTE_H */
