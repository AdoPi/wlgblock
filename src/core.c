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
#include "apu.h"
#include "bit_utils.h"
#include "constants.h"
#include "debug.h"
#include "memory.h"
#include "nelem.h"
#include "palettes.h"
#include "save.h"
#include <errno.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const uint8_t nintendo_logo[CART_LOGO_SIZE] = {
	0xCEu, 0xEDu, 0x66u, 0x66u, 0xCCu, 0x0Du, 0x00u, 0x0Bu,
	0x03u, 0x73u, 0x00u, 0x83u, 0x00u, 0x0Cu, 0x00u, 0x0Du,
	0x00u, 0x08u, 0x11u, 0x1Fu, 0x88u, 0x89u, 0x00u, 0x0Eu,
	0xDCu, 0xCCu, 0x6Eu, 0xE6u, 0xDDu, 0xDDu, 0xD9u, 0x99u,
	0xBBu, 0xBBu, 0x67u, 0x63u, 0x6Eu, 0x0Eu, 0xECu, 0xCCu,
	0xDDu, 0xDCu, 0x99u, 0x9Fu, 0xBBu, 0xB9u, 0x33u, 0x3Eu
};

static void load_rom(struct gbcc_core *gbc, const char *filename);
static void parse_header(struct gbcc_core *gbc);
static bool verify_cartridge(struct gbcc_core *gbc, bool print);
static void load_title(struct gbcc_core *gbc);
static void init_mode(struct gbcc_core *gbc);
static void print_licensee_code(struct gbcc_core *gbc);
static void get_cartridge_hardware(struct gbcc_core *gbc);
static void init_ram(struct gbcc_core *gbc);
static void print_destination_code(struct gbcc_core *gbc);
static void init_registers(struct gbcc_core *gbc);
static void init_mmap(struct gbcc_core *gbc);
static void init_ioreg(struct gbcc_core *gbc);

void gbcc_initialise(struct gbcc_core *gbc, const char *filename)
{
	*gbc = (const struct gbcc_core){0};
	gbc->error_msg = NULL;
	gbc->version = GBCC_SAVE_STATE_VERSION;
	gbc->cart.filename = filename;
	gbc->cart.mbc.type = NONE;
	gbc->cart.mbc.romx_bank = 0x01u;
	gbc->cart.mbc.sram_bank = 0x00u;
	gbc->cart.mbc.romb0 = 0x01u;
	gbc->cart.mbc.accelerometer.real_x = 0x81D0u;
	gbc->cart.mbc.accelerometer.real_y = 0x81D0u;
	gbc->cpu.ime = false;
	gbc->ppu.clock = 0;
	gbc->ppu.palette = gbcc_get_palette("default");
	gbc->ppu.screen.buffer_0 = calloc(GBC_SCREEN_SIZE, sizeof(uint32_t));
	gbc->ppu.screen.buffer_1 = calloc(GBC_SCREEN_SIZE, sizeof(uint32_t));
	gbc->ppu.screen.gbc = gbc->ppu.screen.buffer_0;
	gbc->ppu.screen.sdl = gbc->ppu.screen.buffer_1;
	load_rom(gbc, filename);
	if (gbc->error) {
		return;
	}
	parse_header(gbc);
	if (gbc->error) {
		return;
	}
	init_mmap(gbc);
	init_ioreg(gbc);
	gbcc_apu_init(gbc);

	for (size_t i = 0; i < N_ELEM(gbc->memory.wram_bank); i++) {
		for (size_t j = 0; j < N_ELEM(gbc->memory.wram_bank[i]); j++) {
			gbc->memory.wram_bank[i][j] = (uint8_t)rand();
		}
	}
	for (size_t i = 0; i < N_ELEM(gbc->memory.hram); i++) {
		gbc->memory.hram[i] = (uint8_t)rand();
	}

	sem_init(&gbc->ppu.vsync_semaphore, 0, 0);
	gbc->initialised = true;
}

void gbcc_free(struct gbcc_core *gbc)
{
	if (!gbc->initialised) {
		return;
	}
	gbc->initialised = false;
	sem_destroy(&gbc->ppu.vsync_semaphore);
	free(gbc->cart.rom);
	if (gbc->cart.ram_size > 0) {
		free(gbc->cart.ram);
	}
	free(gbc->ppu.screen.buffer_0);
	free(gbc->ppu.screen.buffer_1);
	*gbc = (const struct gbcc_core){0};
}

void load_rom(struct gbcc_core *gbc, const char *filename)
{
	gbcc_log_info("Loading %s...\n", filename);

	FILE *rom = fopen(filename, "rb");
	if (rom == NULL)
	{
		gbcc_log_error("Error opening file %s: %s\n", filename, strerror(errno));
		gbc->error = true;
		gbc->error_msg = "Couldn't read ROM file.\n";
		return;
	}

	fseek(rom, 0, SEEK_END);
	if (ferror(rom)) {
		gbcc_log_error("Error seeking in file %s: %s\n", filename, strerror(errno));
		fclose(rom);
		gbc->error = true;
		gbc->error_msg = "Couldn't read ROM file.\n";
		return;
	}

	long pos = ftell(rom);
	if (pos <= 0) {
		gbcc_log_error("Error seeking in file %s: %s\n", filename, strerror(errno));
		fclose(rom);
		gbc->error = true;
		gbc->error_msg = "Couldn't read ROM file.\n";
		return;
	}
	gbc->cart.rom_size = (size_t)pos;

	gbc->cart.rom_banks = gbc->cart.rom_size / ROM0_SIZE;
	gbcc_log_info("\tCartridge size: 0x%zX bytes (%zu banks)\n", gbc->cart.rom_size, gbc->cart.rom_banks);

	if (gbc->cart.rom_banks < 2) {
		gbcc_log_warning("ROM smaller than minimum size of 2 banks\n");
		gbc->cart.rom = (uint8_t *) calloc(ROMX_END, 1);
		gbc->cart.rom_banks = 2;
	} else {
		gbc->cart.rom = (uint8_t *) calloc(gbc->cart.rom_size, 1);
	}
	if (gbc->cart.rom == NULL) {
		gbcc_log_error("Error allocating ROM.\n");
		fclose(rom);
		gbc->error = true;
		gbc->error_msg = "Couldn't read ROM file.\n";
		return;
	}


	if (fseek(rom, 0, SEEK_SET) != 0) {
		gbcc_log_error("Error seeking in file %s: %s\n", filename, strerror(errno));
		fclose(rom);
		gbc->error = true;
		gbc->error_msg = "Couldn't read ROM file.\n";
		return;
	}

	size_t read = fread(gbc->cart.rom, 1, gbc->cart.rom_size, rom);
	if (read == 0) {
		gbcc_log_error("Error reading from file %s: %s\n", filename, strerror(errno));
		fclose(rom);
		gbc->error = true;
		gbc->error_msg = "Couldn't read ROM file.\n";
		return;
	}

	fclose(rom);
	gbcc_log_info("\tROM loaded.\n");
}

void parse_header(struct gbcc_core *gbc)
{
	gbcc_log_info("Parsing header...\n");
	/* Check for MMM01, which has its header at the end of the file */
	size_t last_bank = (0x1FEu * 0x4000) % gbc->cart.rom_size;
	gbc->memory.rom0 = gbc->cart.rom + last_bank;
	gbc->cart.mbc.rom0_bank = (uint16_t)(last_bank / 0x4000u);
	if (!verify_cartridge(gbc, false)) {
		gbc->memory.rom0 = gbc->cart.rom;
		gbc->cart.mbc.rom0_bank = 0;
	}
	verify_cartridge(gbc, true);
	uint8_t rom_size_flag = gbc->memory.rom0[CART_ROM_SIZE_FLAG];
	size_t rom_size = 0;
	if (rom_size_flag <= 0x08u) {
		rom_size = 0x8000u << rom_size_flag; /* 32KB shl N */
	} else if (rom_size_flag == 0x52u) {
		rom_size = 0x120000u;
	} else if (rom_size_flag == 0x53u) {
		rom_size = 0x140000u;
	} else if (rom_size_flag == 0x54u) {
		rom_size = 0x180000u;
	} else {
		gbcc_log_error("Unknown ROM size flag: 0x%02X\n", rom_size_flag);
	}
	if (rom_size && rom_size != gbc->cart.rom_size) {
		gbcc_log_warning("ROM size flag does not match file size\n");
	}
	load_title(gbc);
	print_licensee_code(gbc);
	init_mode(gbc);
	get_cartridge_hardware(gbc);
	if (gbc->error) {
		return;
	}
	init_ram(gbc);
	if (gbc->error) {
		return;
	}
	print_destination_code(gbc);
	if (gbc->error) {
		return;
	}
	init_registers(gbc);
	gbcc_log_info("\tHeader parsed.\n");
}

bool verify_cartridge(struct gbcc_core *gbc, bool print)
{
	for (uint16_t i = CART_LOGO_START; i < CART_LOGO_GBC_CHECK_END; i++) {
		if (gbc->memory.rom0[i] != nintendo_logo[i - CART_LOGO_START]) {
			if (print) {
				gbcc_log_error("Cartridge logo check failed on byte %04X\n", i);
			}
			return false;
		}
	}
	if (print) {
		gbcc_log_info("\tCartridge logo check passed.\n");
	}
	uint16_t sum = 0;
	for (size_t i = CART_HEADER_CHECKSUM_START; i < CART_HEADER_CHECKSUM_START + CART_HEADER_CHECKSUM_SIZE + 1; i++) {
		sum += gbc->memory.rom0[i];
	}
	sum = low_byte(sum + 25u);
	if (sum) {
		if (print) {
			gbcc_log_error("Cartridge checksum failed with value %04X.\n", sum);
		}
		return false;
	}
	if (print) {
		gbcc_log_info("\tCartridge checksum passed.\n");
	}
	return true;
}

void load_title(struct gbcc_core *gbc)
{
	uint8_t *title = gbc->memory.rom0 + CART_TITLE_START;
	memcpy(gbc->cart.title, title, CART_TITLE_SIZE);
	gbc->cart.title[CART_TITLE_SIZE] = '\0';
	gbcc_log_info("\tTitle: %s\n", gbc->cart.title);
}

void print_licensee_code(struct gbcc_core *gbc)
{
	uint8_t old_code = gbc->memory.rom0[CART_OLD_LICENSEE_CODE];
	char buffer[CART_NEW_LICENSEE_CODE_SIZE + 21];
	size_t len = N_ELEM(buffer);
	if (old_code != 0x33u) {
		snprintf(buffer, len, "\tLicensee code: 0x%02X\n", old_code);
	} else {
		unsigned char new_code[CART_NEW_LICENSEE_CODE_SIZE + 1];
		for (size_t i = CART_NEW_LICENSEE_CODE_START; i < CART_NEW_LICENSEE_CODE_END; i++) {
			new_code[i - CART_NEW_LICENSEE_CODE_START] = gbc->memory.rom0[i];
		}
		new_code[CART_NEW_LICENSEE_CODE_SIZE] = '\0';
		snprintf(buffer, len, "\tLicensee code: %s\n", new_code);
	}
	gbcc_log_info("%s", buffer);
}

void init_mode(struct gbcc_core *gbc)
{
	uint8_t mode_flag;

	mode_flag = gbc->memory.rom0[CART_GBC_FLAG];
	if (mode_flag == 0x80u || mode_flag == 0xC0u) {
		gbc->mode = GBC;
	} else {
		gbc->mode = DMG;
	}
	if (check_bit(mode_flag, 7) && (mode_flag & 0x0Cu)) {
		gbcc_log_debug("Colorised DMG mode unsupported\n");
	}
	switch (gbc->mode) {
		case DMG:
			gbcc_log_info("\tMode: DMG\n");
			break;
		case GBC:
			gbcc_log_info("\tMode: GBC\n");
			break;
	}
}

void get_cartridge_hardware(struct gbcc_core *gbc)
{
	switch (gbc->memory.rom0[CART_TYPE]) {
		case 0x00u:	/* ROM ONLY */
			gbc->cart.mbc.type = NONE;
			gbcc_log_info("\tHardware: ROM only\n");
			break;
		case 0x01u:	/* MBC1 */
			gbc->cart.mbc.type = MBC1;
			gbcc_log_info("\tHardware: MBC1\n");
			break;
		case 0x02u:	/* MBC1 + RAM */
			gbc->cart.mbc.type = MBC1;
			gbcc_log_info("\tHardware: MBC1 + RAM\n");
			break;
		case 0x03u:	/* MBC1 + RAM + BATTERY */
			gbc->cart.mbc.type = MBC1;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: MBC1 + RAM + Battery\n");
			break;
		case 0x05u:	/* MBC2 */
			gbc->cart.mbc.type = MBC2;
			gbcc_log_info("\tHardware: MBC2\n");
			break;
		case 0x06u:	/* MBC2 + BATTERY */
			gbc->cart.mbc.type = MBC2;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: MBC2 + Battery\n");
			break;
		case 0x08u:	/* ROM + RAM */
			gbcc_log_info("\tHardware: ROM + RAM\n");
			break;
		case 0x09u:	/* ROM + RAM + BATTERY */
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: ROM + RAM + Battery\n");
			break;
		case 0x0Bu:	/* MMM01 */
			gbc->cart.mbc.type = MMM01;
			gbcc_log_info("\tHardware: MMM01\n");
			break;
		case 0x0Cu:	/* MMM01 + RAM */
			gbc->cart.mbc.type = MMM01;
			gbcc_log_info("\tHardware: MMM01 + RAM\n");
			break;
		case 0x0Du:	/* MMM01 + RAM + BATTERY */
			gbc->cart.mbc.type = MMM01;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: MMM01 + RAM + Battery\n");
			break;
		case 0x0Fu:	/* MBC3 + TIMER + BATTERY */
			gbc->cart.mbc.type = MBC3;
			gbc->cart.timer = true;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: MBC3 + Timer + Battery\n");
			break;
		case 0x10u:	/* MBC3 + TIMER + RAM + BATTERY */
			gbc->cart.timer = true;
			gbc->cart.battery = true;
			gbc->cart.mbc.type = MBC3;
			gbcc_log_info("\tHardware: MBC3 + Timer + RAM + Battery\n");
			break;
		case 0x11u:	/* MBC3 */
			gbc->cart.mbc.type = MBC3;
			gbcc_log_info("\tHardware: MBC3\n");
			break;
		case 0x12u:	/* MBC3 + RAM */
			gbc->cart.mbc.type = MBC3;
			gbcc_log_info("\tHardware: MBC3 + RAM\n");
			break;
		case 0x13u:	/* MBC3 + RAM + BATTERY */
			gbc->cart.mbc.type = MBC3;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: MBC3 + RAM + Battery\n");
			break;
		case 0x19u:	/* MBC5 */
			gbc->cart.mbc.type = MBC5;
			gbcc_log_info("\tHardware: MBC5\n");
			break;
		case 0x1Au:	/* MBC5 + RAM */
			gbc->cart.mbc.type = MBC5;
			gbcc_log_info("\tHardware: MBC5 + RAM\n");
			break;
		case 0x1Bu:	/* MBC5 + RAM + BATTERY */
			gbc->cart.mbc.type = MBC5;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: MBC5 + RAM + Battery\n");
			break;
		case 0x1Cu:	/* MBC5 + RUMBLE */
			gbc->cart.mbc.type = MBC5;
			gbc->cart.rumble = true;
			gbcc_log_info("\tHardware: MBC5 + Rumble\n");
			break;
		case 0x1Du:	/* MBC5 + RUMBLE + RAM */
			gbc->cart.mbc.type = MBC5;
			gbc->cart.rumble = true;
			gbcc_log_info("\tHardware: MBC5 + Rumble + RAM\n");
			break;
		case 0x1Eu:	/* MBC5 + RUMBLE + RAM + BATTERY */
			gbc->cart.mbc.type = MBC5;
			gbc->cart.rumble = true;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: MBC5 + Rumble + RAM + Battery\n");
			break;
		case 0x20u: 	/* MBC6 */
			gbcc_log_error("MBC6 not yet supported.\n");
			gbc->error = true;
			gbc->error_msg = "MBC6 not yet supported.\n";
			break;
		case 0x22u: 	/* MBC7 + SENSOR + RUMBLE + RAM + BATTERY */
			gbc->cart.mbc.type = MBC7;
			gbc->cart.rumble = true;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: MBC7 + Tilt Sensor + Rumble + RAM + Battery\n");
			break;
		case 0xFCu: 	/* Pocket Camera */
			gbc->cart.mbc.type = CAMERA;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: Pocket Camera\n");
			break;
		case 0xFDu: 	/* Bandai TAMA5 */
			gbcc_log_error("Bandai TAMA5 not yet supported.\n");
			gbc->error = true;
			gbc->error_msg = "Bandai TAMA5 not yet supported.\n";
			break;
		case 0xFEu: 	/* HuC3 */
			gbc->cart.mbc.type = HUC3;
			gbcc_log_info("\tHardware: HuC3\n");
			gbcc_log_warning("HuC3 support is experimental, saves etc. are unlikely to work.\n");
			break;
		case 0xFFu: 	/* HuC1 + RAM + BATTERY */
			gbc->cart.mbc.type = HUC1;
			gbc->cart.battery = true;
			gbcc_log_info("\tHardware: HuC1 + RAM + Battery\n");
			break;
		default:
			gbcc_log_error("Unrecognised hardware %02X, falling back to MBC3.\n", gbc->memory.rom0[CART_TYPE]);
			gbc->cart.mbc.type = MBC3;
			break;
	}
}

void init_ram(struct gbcc_core *gbc)
{
	uint8_t ram_size_flag;

	ram_size_flag = gbc->memory.rom0[CART_RAM_SIZE_FLAG];

	switch (ram_size_flag) {
		case 0x00u:
			if (gbc->cart.mbc.type == MBC2) {
				gbc->cart.ram_size = 0x0200u;
			}
			break;
		case 0x01u:
		case 0x02u:
		case 0x03u:
		case 0x04u:
			gbc->cart.ram_size = 0x200u << (2u * ram_size_flag);
			break;
		case 0x05u:
			gbc->cart.ram_size = 0x2000u << 3u;
			break;
		default:
			gbcc_log_error("Unknown ram size flag: %u\n", ram_size_flag);
			gbc->error = true;
			gbc->error_msg = "Invalid ram size.\n";
			return;
	}

	if (gbc->cart.ram_size > 0) {
		gbc->cart.ram_banks = gbc->cart.ram_size / SRAM_SIZE;
		gbcc_log_info("\tCartridge RAM: 0x%0zX bytes (%zu banks)\n", gbc->cart.ram_size, gbc->cart.ram_banks);
		if (gbc->cart.ram_banks < 1) {
			/* 
			 * Allocate a minimum of 1 ram bank, to prevent
			 * crashing if invalid ram size is specified
			 */
			gbc->cart.ram = (uint8_t *) calloc(SRAM_SIZE, 1);
			gbc->cart.ram_banks = 1;
		} else {
			gbc->cart.ram = (uint8_t *) calloc(gbc->cart.ram_size, 1);
		}

		if (gbc->cart.ram == NULL) {
			gbcc_log_error("Error allocating RAM.\n");
			gbc->error = true;
			gbc->error_msg = "Couldn't allocate cartridge RAM.\n";
			return;
		}
	}
}

void print_destination_code(struct gbcc_core *gbc)
{
	uint8_t dest_code = gbc->memory.rom0[CART_DESTINATION_CODE];
	switch (dest_code) {
		case 0x00u:
			gbcc_log_info("\tRegion: Japan\n");
			break;
		case 0x01u:
			gbcc_log_info("\tRegion: Non-Japanese\n");
			break;
		default:
			gbcc_log_error("Unrecognised region code: 0x%02X\n", dest_code);
			gbc->error = true;
			gbc->error_msg = "Unrecognised region code.\n";
			return;
	}
}

void init_registers(struct gbcc_core *gbc) {
	struct cpu *cpu = &gbc->cpu;
	switch (gbc->mode) {
		case DMG:
			cpu->reg.af = 0x01B0u;
			cpu->reg.bc = 0x0013u;
			cpu->reg.de = 0x00D8u;
			cpu->reg.hl = 0x014Du;
			cpu->reg.sp = 0xFFFEu;
			cpu->reg.pc = 0x0100u;
			break;
		case GBC:
			cpu->reg.af = 0x1180u;
			cpu->reg.bc = 0x0000u;
			cpu->reg.de = 0xFF56u;
			cpu->reg.hl = 0x000Du;
			cpu->reg.sp = 0xFFFEu;
			cpu->reg.pc = 0x0100u;
			break;
	}
}

void init_mmap(struct gbcc_core *gbc)
{
	gbc->memory.romx = gbc->memory.rom0 + ROM0_SIZE;
	gbc->memory.vram = gbc->memory.vram_bank[0];
	gbc->memory.sram = gbc->cart.ram;
	gbc->memory.wram0 = gbc->memory.wram_bank[0];
	gbc->memory.wramx = gbc->memory.wram_bank[1];
	gbc->memory.echo = gbc->memory.wram0;
}

void init_ioreg(struct gbcc_core *gbc)
{
	/* Values taken from the pandocs and mooneye tests*/
	gbc->memory.ioreg[JOYP - IOREG_START] = 0xCFu;
	gbc->memory.ioreg[SC - IOREG_START] = 0x7Eu;
	gbc->memory.ioreg[DIV - IOREG_START] = 0xACu;
	gbc->memory.ioreg[TIMA - IOREG_START] = 0x00u;
	gbc->memory.ioreg[TMA - IOREG_START] = 0x00u;
	gbc->memory.ioreg[TAC - IOREG_START] = 0x00u;
	gbc->memory.ioreg[IF - IOREG_START] = 0x01u;
	gbc->memory.ioreg[NR10 - IOREG_START] = 0x80u;
	gbc->memory.ioreg[NR11 - IOREG_START] = 0xBFu;
	gbc->memory.ioreg[NR12 - IOREG_START] = 0xF3u;
	gbc->memory.ioreg[NR14 - IOREG_START] = 0xBFu;
	gbc->memory.ioreg[NR21 - IOREG_START] = 0x3Fu;
	gbc->memory.ioreg[NR22 - IOREG_START] = 0x00u;
	gbc->memory.ioreg[NR24 - IOREG_START] = 0xBFu;
	gbc->memory.ioreg[NR30 - IOREG_START] = 0x7Fu;
	gbc->memory.ioreg[NR31 - IOREG_START] = 0xFFu;
	gbc->memory.ioreg[NR32 - IOREG_START] = 0x9Fu;
	gbc->memory.ioreg[NR33 - IOREG_START] = 0xBFu;
	gbc->memory.ioreg[NR41 - IOREG_START] = 0xFFu;
	gbc->memory.ioreg[NR42 - IOREG_START] = 0x00u;
	gbc->memory.ioreg[NR43 - IOREG_START] = 0x00u;
	gbc->memory.ioreg[NR50 - IOREG_START] = 0x77u;
	gbc->memory.ioreg[NR51 - IOREG_START] = 0xF3u;
	gbc->memory.ioreg[NR52 - IOREG_START] = 0xF1u;
	gbc->memory.ioreg[LCDC - IOREG_START] = 0x91u;
	gbc->memory.ioreg[SCY - IOREG_START] = 0x00u;
	gbc->memory.ioreg[SCX - IOREG_START] = 0x00u;
	gbc->memory.ioreg[LY - IOREG_START] = 0x00u;
	gbc->memory.ioreg[LYC - IOREG_START] = 0x00u;
	gbc->memory.ioreg[BGP - IOREG_START] = 0xFCu;
	gbc->memory.ioreg[OBP0 - IOREG_START] = 0xFFu;
	gbc->memory.ioreg[OBP1 - IOREG_START] = 0xFFu;
	gbc->memory.ioreg[WY - IOREG_START] = 0x00u;
	gbc->memory.ioreg[WX - IOREG_START] = 0x00u;
	gbc->memory.ioreg[HDMA5 - IOREG_START] = 0xFFu;
	gbc->memory.ioreg[SVBK - IOREG_START] = 0x01u;
	gbc->memory.iereg = 0x00u;
	memset(gbc->ppu.bgp, 0xFFu, sizeof(gbc->ppu.bgp));
}
