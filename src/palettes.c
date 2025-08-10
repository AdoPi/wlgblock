/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "colour.h"
#include "debug.h"
#include "nelem.h"
#include "palettes.h"
#include <strings.h>

const struct palette gbcc_palettes[GBCC_NUM_PALETTES] = {
	{ /* Default */
		.name = "Default",
		.background = {0xc4cfa100u, 0x8b956d00u, 0x6b735300u, 0x00000000u},
		.sprite1    = {0xc4cfa100u, 0x8b956d00u, 0x6b735300u, 0x00000000u},
		.sprite2    = {0xc4cfa100u, 0x8b956d00u, 0x6b735300u, 0x00000000u}
	},
	{ /* Monochrome */
		.name = "Monochrome",
		.background = {0xffffff00u, 0xaaaaaa00u, 0x55555500u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0xaaaaaa00u, 0x55555500u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0xaaaaaa00u, 0x55555500u, 0x00000000u}
	},
	{ /* brown: up */
		.name = "Brown",
		.background = {0xffffff00u, 0xffad6300u, 0x83310000u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0xffad6300u, 0x83310000u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0xffad6300u, 0x83310000u, 0x00000000u}
	},
	{ /* red: up a */
		.name = "Red",
		.background = {0xffffff00u, 0xff858400u, 0x943a3a00u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0x7bff3000u, 0x00830000u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0x65a49b00u, 0x0000fe00u, 0x00000000u}
	},
	{ /* darkbrown: up b */
		.name = "Dark-Brown",
		.background = {0xffe7c500u, 0xce9c8500u, 0x846b2900u, 0x5b310900u},
		.sprite1    = {0xffffff00u, 0xffad6300u, 0x83310000u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0xffad6300u, 0x83310000u, 0x00000000u}
	},
	{ /* pastel: down */
		.name = "Pastel",
		.background = {0xffffa500u, 0xfe949400u, 0x9394fe00u, 0x00000000u},
		.sprite1    = {0xffffa500u, 0xfe949400u, 0x9394fe00u, 0x00000000u},
		.sprite2    = {0xffffa500u, 0xfe949400u, 0x9394fe00u, 0x00000000u}
	},
	{ /* orange: down a */
		.name = "Orange",
		.background = {0xffffff00u, 0xffff0000u, 0xfe000000u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0xffff0000u, 0xfe000000u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0xffff0000u, 0xfe000000u, 0x00000000u}
	},
	{ /* yellow: down b */
		.name = "Yellow",
		.background = {0xffffff00u, 0xffff0000u, 0x7d490000u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0x65a49b00u, 0x0000fe00u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0x7bff3000u, 0x00830000u, 0x00000000u}
	},
	{ /* blue: left */
		.name = "Blue",
		.background = {0xffffff00u, 0x65a49b00u, 0x0000fe00u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0xff858400u, 0x83310000u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0x7bff3000u, 0x00830000u, 0x00000000u}
	},
	{ /* darkblue: left a */
		.name = "Dark-Blue",
		.background = {0xffffff00u, 0x8b8cde00u, 0x53528c00u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0xff858400u, 0x943a3a00u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0xffad6300u, 0x83310000u, 0x00000000u}
	},
	{ /* grey: left b */
		.name = "Grey",
		.background = {0xffffff00u, 0xa5a5a500u, 0x52525200u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0xa5a5a500u, 0x52525200u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0xa5a5a500u, 0x52525200u, 0x00000000u}
	},
	{ /* green: right */
		.name = "Green",
		.background = {0xffffff00u, 0x51ff0000u, 0xff420000u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0x51ff0000u, 0xff420000u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0x51ff0000u, 0xff420000u, 0x00000000u}
	},
	{ /* darkgreen: right a */
		.name = "Dark-Green",
		.background = {0xffffff00u, 0x7bff3000u, 0x0163c600u, 0x00000000u},
		.sprite1    = {0xffffff00u, 0xff858400u, 0x943a3a00u, 0x00000000u},
		.sprite2    = {0xffffff00u, 0xff858400u, 0x943a3a00u, 0x00000000u}
	},
	{ /* invert: right b */
		.name = "Invert",
		.background = {0x00000000u, 0x00848600u, 0xffde0000u, 0xffffff00u},
		.sprite1    = {0x00000000u, 0x00848600u, 0xffde0000u, 0xffffff00u},
		.sprite2    = {0x00000000u, 0x00848600u, 0xffde0000u, 0xffffff00u}
	}
};

struct palette gbcc_get_palette(const char *name)
{
	return gbcc_palettes[gbcc_get_palette_index(name)];
}

struct palette gbcc_get_palette_by_index(unsigned int index)
{
	if (index >= N_ELEM(gbcc_palettes)) {
		gbcc_log_error("Invalid palette index \"%u\"\n", index);
		index = 0;
	}
	return gbcc_palettes[index];
}

unsigned int gbcc_get_palette_index(const char *name)
{
	unsigned int p;
	for (p = 0; p < N_ELEM(gbcc_palettes); p++) {
		if (strcasecmp(name, gbcc_palettes[p].name) == 0) {
			break;
		}
	}
	if (p >= N_ELEM(gbcc_palettes)) {
		gbcc_log_error("Invalid palette \"%s\"\n", name);
		p = 0;
	}
	return p;
}
