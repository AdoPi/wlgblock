/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 * All modifications made to the code to
 * transform this project into a locker 
 * are under the GPLv3 license.
 * Copyright (C) 2025 Adonis Najimi
 *
 * Licensed under the GPLv3 License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/license/gpl-3-0
 *
 */

#include "gbcc.h"
#include "debug.h"
#include "camera.h"
#include "save.h"
#include <stdio.h>

void *gbcc_emulation_loop(void *_gbc)
{
	struct gbcc *gbc = (struct gbcc *)_gbc;
	while (!gbc->quit) {
		for (int i = 1000; i > 0; i--) {
			/* Only check for savestates, pause etc.
			 * every 1000 cycles */
			gbcc_emulate_cycle(&gbc->core);
			if (gbc->core.error) {
				gbcc_log_error("Invalid opcode: 0x%02X\n", gbc->core.cpu.opcode);
				gbcc_print_registers(&gbc->core, false);
				gbc->quit = true;
				return 0;
			}
			gbcc_audio_update(gbc);
		}

	}
	return 0;
}
