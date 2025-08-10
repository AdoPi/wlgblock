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
#include "args.h"
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
	struct gbcc gbc = {0};

	if (!gbcc_parse_args(&gbc, true, argc, argv)) {
		exit(EXIT_FAILURE);
	}

	gbc.core.keys.turbo = true;
	gbc.has_focus = true;

	for (size_t cycles = 10000; cycles > 0; cycles--) {
		gbcc_emulate_cycle(&gbc.core);
	}

	exit(EXIT_SUCCESS);
}
