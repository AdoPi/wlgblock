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
#include "debug.h"
#include "memory.h"
#include "save.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LEN 4096
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define PATH_SEP '\\'
#define PATH_SEP_STR "\\"
#else
#define PATH_SEP '/'
#define PATH_SEP_STR "/"
#endif

static void get_save_basename(struct gbcc *gbc, char savename[MAX_NAME_LEN]);
static void strip_ext(char *fname);
static const char *gbcc_basename(const char *fname);
static bool read_v7_struct(struct gbcc_core *gbc, FILE *f);

void gbcc_save(struct gbcc *gbc)
{
	struct gbcc_core *core = &gbc->core;
	if (core->cart.ram_size == 0 && core->cart.mbc.type != MBC7) {
		return;
	}
	char *fname = malloc(MAX_NAME_LEN);
	char *tmp = malloc(MAX_NAME_LEN);
	get_save_basename(gbc, tmp);
	if (snprintf(fname, MAX_NAME_LEN, "%s.sav", tmp) >= MAX_NAME_LEN) {
		gbcc_log_error("Filename %s too long\n", fname);
		free(fname);
		return;
	}
	free(tmp);
	gbcc_log_info("Saving %s...\n", fname);

	FILE *sav = fopen(fname, "wb");
	if (sav == NULL) {
		gbcc_log_error("Can't open save file %s\n", fname);
		free(fname);
		return;
	}

	fwrite(core->cart.ram, 1, core->cart.ram_size, sav);
	if (core->cart.mbc.type == MBC7) {
		fwrite(core->cart.mbc.eeprom.data, 2, 128, sav);
	}
	if (core->cart.mbc.type == MBC3) {
		fprintf(sav, "\n%u:%u:%u:%u:%u:%u:%u:%ld:%ld\n",
				core->cart.mbc.rtc.seconds,
				core->cart.mbc.rtc.minutes,
				core->cart.mbc.rtc.hours,
				core->cart.mbc.rtc.day_low,
				core->cart.mbc.rtc.day_high,
				core->cart.mbc.rtc.latch,
				core->cart.mbc.rtc.cur_reg,
				core->cart.mbc.rtc.base_time.tv_sec,
				core->cart.mbc.rtc.base_time.tv_nsec
				);
	}
	fclose(sav);
	free(fname);
	gbcc_log_info("Saved.\n");
}

void gbcc_load(struct gbcc *gbc)
{
	struct gbcc_core *core = &gbc->core;
	char *fname = malloc(MAX_NAME_LEN);
	char *tmp = malloc(MAX_NAME_LEN);
	get_save_basename(gbc, tmp);
	if (snprintf(fname, MAX_NAME_LEN, "%s.sav", tmp) >= MAX_NAME_LEN) {
		gbcc_log_error("Filename %s too long\n", fname);
		free(tmp);
		free(fname);
		return;
	}
	free(tmp);
	FILE *sav = fopen(fname, "rb");
	if (sav == NULL) {
		for (size_t i = 0; i < core->cart.ram_size; i++) {
			core->cart.ram[i] = (uint8_t)rand();
		}
		if (core->cart.mbc.type == MBC3) {
			clock_gettime(CLOCK_REALTIME, &core->cart.mbc.rtc.base_time);
		}
		free(fname);
		return;
	}
	gbcc_log_info("Loading %s...\n", fname);
	if (core->cart.mbc.type == MBC7) {
		if (fread(core->cart.mbc.eeprom.data, 2, 128, sav) != 128) {
			gbcc_log_error("Failed to read eeprom data: %s\n", fname);
			fclose(sav);
			return;
		}
	} else {
		if (fread(core->cart.ram, 1, core->cart.ram_size, sav) != core->cart.ram_size) {
			gbcc_log_error("Failed to read save data: %s\n", fname);
			fclose(sav);
			return;
		}
	}
	if (core->cart.mbc.type == MBC3) {
		int matched;
		matched = fscanf(sav, "\n%" SCNu8 ":%" SCNu8 ":%" SCNu8 ":%"
				SCNu8 ":%" SCNu8 ":%" SCNu8 ":%" SCNu8 ":%ld:%ld",
				&core->cart.mbc.rtc.seconds,
				&core->cart.mbc.rtc.minutes,
				&core->cart.mbc.rtc.hours,
				&core->cart.mbc.rtc.day_low,
				&core->cart.mbc.rtc.day_high,
				&core->cart.mbc.rtc.latch,
				&core->cart.mbc.rtc.cur_reg,
				&core->cart.mbc.rtc.base_time.tv_sec,
				&core->cart.mbc.rtc.base_time.tv_nsec
				);
		if (matched < 9) {
			gbcc_log_warning("Couldn't read rtc data, "
					 "resetting base time to now.\n");
			clock_gettime(CLOCK_REALTIME, &core->cart.mbc.rtc.base_time);
		}
	}
	fclose(sav);
	free(fname);
}

void gbcc_save_state(struct gbcc *gbc)
{
	struct gbcc_core *core = &gbc->core;
	char *fname = malloc(MAX_NAME_LEN);
	char *tmp = malloc(MAX_NAME_LEN);
	get_save_basename(gbc, tmp);
	if (snprintf(fname, MAX_NAME_LEN, "%s.s%d", tmp, gbc->save_state) >= MAX_NAME_LEN) {
		gbcc_log_error("Filename %s too long\n", fname);
		free(tmp);
		free(fname);
		return;
	}
	FILE *sav = fopen(fname, "wb");
	if (!sav) {
		gbcc_log_error("Error opening %s: %s\n", fname, strerror(errno));
		gbc->save_state = 0;
		gbc->load_state = 0;
		free(tmp);
		free(fname);
		return;
	}
	fwrite(core, sizeof(struct gbcc_core), 1, sav);
	if (core->cart.ram_size > 0) {
		fwrite(core->cart.ram, 1, core->cart.ram_size, sav);
	}
	fclose(sav);
	snprintf(tmp, MAX_NAME_LEN, "Saved state %d", gbc->save_state);
	gbcc_log_info("Saved state %s\n", fname);
	gbcc_window_show_message(gbc, tmp, 2, true);
	gbc->save_state = 0;
	gbc->load_state = 0;
	free(tmp);
	free(fname);
}

void gbcc_load_state(struct gbcc *gbc)
{
	struct gbcc_core *core = &gbc->core;

	char *fname = malloc(MAX_NAME_LEN);
	char *tmp = malloc(MAX_NAME_LEN);
	get_save_basename(gbc, tmp);
	if (snprintf(fname, MAX_NAME_LEN, "%s.s%d", tmp, gbc->load_state) >= MAX_NAME_LEN) {
		gbcc_log_error("Filename %s too long\n", fname);
		free(tmp);
		free(fname);
		return;
	}
	FILE *sav = fopen(fname, "rb");
	if (!sav) {
		if (errno == ENOENT) {
			snprintf(tmp, MAX_NAME_LEN, "State %d not found", gbc->load_state);
			gbcc_window_show_message(gbc, tmp, 2, true);
		}
		gbcc_log_error("Error opening %s: %s\n", fname, strerror(errno));
		gbc->save_state = 0;
		gbc->load_state = 0;
		free(tmp);
		free(fname);
		return;
	}
	uint32_t old_version = 0;
	if (fread(&old_version, 4, 1, sav) != 1) {
		gbcc_log_error("Couldn't read save state version.\n");
		fclose(sav);
		free(tmp);
		free(fname);
		return;
	}
	rewind(sav);
	/* Hardcoded check, should be updated when updating the core version */
	if (old_version != 7 && old_version != core->version) {
		gbcc_log_error("Save state %d version mismatch, tried "
				"to load v%u (current version is v%u).\n",
				gbc->load_state,
				old_version,
				core->version);
		snprintf(tmp, MAX_NAME_LEN, "Save state %d version "
				"mismatch:\n have v%u, loaded v%u",
				gbc->load_state,
				core->version,
				old_version);
		gbcc_window_show_message(gbc, tmp, 2, true);
		gbc->save_state = 0;
		gbc->load_state = 0;
		fclose(sav);
		free(tmp);
		free(fname);
		return;
	}

	struct gbcc_core *tmp_core = calloc(1, sizeof(*tmp_core));
	bool read_success = false;

	/* Hardcoded check, should be updated when updating the core version */
	if (old_version == 7 && gbc->core.version == 8) {
		read_success = read_v7_struct(tmp_core, sav);
	} else {
		read_success = (fread(tmp_core, sizeof(struct gbcc_core), 1, sav) == 1);
	}
	if (!read_success) {
		gbcc_log_error("Error reading %s: %s\n", fname, strerror(errno));
		free(tmp_core);
		fclose(sav);
		gbc->save_state = 0;
		gbc->load_state = 0;
		free(tmp);
		free(fname);
		return;
	}

	/*
	 * Load the sram data from the savestate if there is any, and then we
	 * can safely close the file
	 * */
	if (core->cart.ram_size > 0) {
		if (fread(core->cart.ram, 1, core->cart.ram_size, sav) != core->cart.ram_size) {
			gbcc_log_error("Error reading %s: %s\n", fname, strerror(errno));
			free(tmp_core);
			fclose(sav);
			gbc->save_state = 0;
			gbc->load_state = 0;
			free(tmp);
			free(fname);
			return;
		}
	}
	fclose(sav);

	/*
	 * Now that we've loaded the struct, we need to make sure all pointers
	 * are updated to be correct. This is done roughly in order of
	 * definition within "core.h"
	 */

	/* cpu */
	/* No pointers */

	/* apu */
	/* No pointers */

	/* ppu */
	tmp_core->ppu.screen.buffer_0 = core->ppu.screen.buffer_0;
	tmp_core->ppu.screen.buffer_1 = core->ppu.screen.buffer_1;
	tmp_core->ppu.screen.gbc = core->ppu.screen.gbc;
	tmp_core->ppu.screen.sdl = core->ppu.screen.sdl;

	/* cart */
	/* No pointers in the mbc */
	tmp_core->cart.filename = core->cart.filename;
	tmp_core->cart.rom = core->cart.rom;
	tmp_core->cart.ram = core->cart.ram;

	/* memory */
	/*
	 * We first need to work out which banks the wram & vram are set to.
	 * This can safely be done before the rest of memory is initialised as
	 * the io registers are an array in the struct, so we've already loaded
	 * them.
	 */
	uint8_t wram_bank;
	uint8_t vram_bank;
	switch (tmp_core->mode) {
		case DMG:
			wram_bank = 1;
			vram_bank = 0;
			break;
		case GBC:
			wram_bank = gbcc_memory_read(tmp_core, SVBK) & 0x07u;
			vram_bank = gbcc_memory_read(tmp_core, VBK) & 0x01u;
			break;
	}

	tmp_core->memory.rom0 = core->cart.rom;
	tmp_core->memory.romx = core->cart.rom + tmp_core->cart.mbc.romx_bank * ROMX_SIZE;
	tmp_core->memory.vram = core->memory.vram_bank[vram_bank];
	if (tmp_core->cart.ram != NULL) {
		tmp_core->memory.sram = core->cart.ram + tmp_core->cart.mbc.sram_bank * SRAM_SIZE;
	} else {
		tmp_core->memory.sram = NULL;
	}
	tmp_core->memory.wram0 = core->memory.wram_bank[0];
	tmp_core->memory.wramx = core->memory.wram_bank[wram_bank];
	tmp_core->memory.echo = core->memory.wram0;

	/* printer */
	/* No pointers */

	/* Reset some things that shouldn't be saved */
	memset(&tmp_core->keys, 0, sizeof(tmp_core->keys));
	tmp_core->sync_to_video = core->sync_to_video;
	tmp_core->error_msg = NULL;

	/* Perform the actual switch */
	*core = *tmp_core;
	free(tmp_core);

	snprintf(tmp, MAX_NAME_LEN, "Loaded state %d", gbc->load_state);
	gbcc_window_show_message(gbc, tmp, 2, true);
	gbcc_log_info("Loaded state %s\n", fname);

	gbc->save_state = 0;
	gbc->load_state = 0;
	free(tmp);
	free(fname);
}

bool gbcc_check_savestate(struct gbcc *gbc, int state)
{
	struct gbcc_core *core = &gbc->core;

	char *fname = malloc(MAX_NAME_LEN);
	char *tmp = malloc(MAX_NAME_LEN);
	FILE *sav;
	if (snprintf(tmp, MAX_NAME_LEN, "%s", core->cart.filename) >= MAX_NAME_LEN) {
		gbcc_log_error("Filename %s too long\n", tmp);
		free(tmp);
		free(fname);
		return false;
	}
	strip_ext(tmp);
	if (snprintf(fname, MAX_NAME_LEN, "%s.s%d", tmp, state) >= MAX_NAME_LEN) {
		gbcc_log_error("Filename %s too long\n", fname);
		free(tmp);
		free(fname);
		return false;
	}
	sav = fopen(fname, "rb");
	free(tmp);
	free(fname);
	if (!sav) {
		return false;
	}
	fclose(sav);
	return true;
}

void get_save_basename(struct gbcc *gbc, char savename[MAX_NAME_LEN]) {
	int written  = 0;
	size_t len = strlen(gbc->save_directory);
	if (len) {
		bool print_path_sep = (gbc->save_directory[len - 1] != PATH_SEP);
		written = snprintf(
				savename,
				MAX_NAME_LEN,
				"%s%s%s",
				gbc->save_directory,
				print_path_sep ? PATH_SEP_STR : "",
				gbcc_basename(gbc->core.cart.filename));
		if (written >= MAX_NAME_LEN) {
			gbcc_log_error("Filename %s too long\n", savename);
		}
	} else {
		written = snprintf(savename, MAX_NAME_LEN, "%s", gbc->core.cart.filename);
		if (written >= MAX_NAME_LEN) {
			gbcc_log_error("Filename %s too long\n", savename);
		}
	}
	strip_ext(savename);
}

void strip_ext(char *fname)
{
	char *end = fname + strlen(fname);
	while (end > fname && *end != '.') {
		--end;
	}

	if (end > fname) {
		*end = '\0';
	}
}

const char *gbcc_basename(const char *fname)
{
	const char *ret = strrchr(fname, PATH_SEP);
	return ret ? ret + 1 : fname;
}

/*
 * In-place conversion from the previous core struct version to this one.
 * This is a dirty hack, but the alternative is trusting users to not rely on
 * savestates when upgrading.
 * Proper serialisation of the core struct would be nice....
 * WARNING: This needs to be updated when changing the core version.
 */
bool read_v7_struct(struct gbcc_core *gbc, FILE *f)
{
	/*
	 * v8 added cheats - luckily we can just ignore the lost values this
	 * time, as they don't matter, and re-zero the cheats struct.
	 */
	size_t old_size = sizeof(*gbc) - sizeof(gbc->cheats);
	bool success = fread(gbc, old_size, 1, f) == 1;
	memset(&gbc->cheats, 0, sizeof(gbc->cheats));
	gbc->version = GBCC_SAVE_STATE_VERSION;
	gbc->initialised = true;
	return success;
}
