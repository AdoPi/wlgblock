#include "cheats.h"
#include "core.h"
#include "debug.h"
#include "nelem.h"
#include <stdio.h>
#include <string.h>

static struct gbcc_gamegenie_cheat parse_gamegenie_code(const char *code);
static struct gbcc_gameshark_cheat parse_gameshark_code(const char *code);
static uint8_t hex2int(char c);

void gbcc_cheats_add_fuzzy(struct gbcc_core *gbc, const char *code)
{	
	char cheat[10];
	int j = 0;
	for (size_t i = 0; i < strlen(code); i++) {
		if (code[i] == '-') {
			continue;
		}
		cheat[j++] = code[i];
		if (j == 10) {
			gbcc_log_error("Invalid cheat code '%s'.\n", code);
			break;
		}
	}
	if (j < 8) {
		gbcc_log_error("Invalid cheat code '%s'.\n", code);
	} else if (j == 8) {
		gbcc_cheats_add_gameshark(gbc, cheat);
	} else if (j == 9) {
		gbcc_cheats_add_gamegenie(gbc, cheat);
	}

}

void gbcc_cheats_add_gamegenie(struct gbcc_core *gbc, const char *code)
{
	uint8_t n = gbc->cheats.num_genie_cheats;
	if (n >= N_ELEM(gbc->cheats.gamegenie)) {
		return;
	}
	gbc->cheats.gamegenie[n] = parse_gamegenie_code(code);
	gbc->cheats.num_genie_cheats = n+1;
}

void gbcc_cheats_add_gameshark(struct gbcc_core *gbc, const char *code)
{
	uint8_t n = gbc->cheats.num_shark_cheats;
	if (n >= N_ELEM(gbc->cheats.gameshark)) {
		return;
	}
	gbc->cheats.gameshark[n] = parse_gameshark_code(code);
	gbc->cheats.num_shark_cheats = n+1;
}

uint8_t gbcc_cheats_gamegenie_read(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	for (int i = 0; i < gbc->cheats.num_genie_cheats; i++) {
		struct gbcc_gamegenie_cheat cheat = gbc->cheats.gamegenie[i];
		if (cheat.address == addr && cheat.old_data == val) {
			return cheat.new_data;
		}
	}
	return val;
}

void gbcc_cheats_gameshark_update(struct gbcc_core *gbc)
{
	for (int i = 0; i < gbc->cheats.num_shark_cheats; i++) {
		struct gbcc_gameshark_cheat cheat = gbc->cheats.gameshark[i];
		if (cheat.address >= SRAM_START && cheat.address < SRAM_END) {
			if (cheat.ram_bank > gbc->cart.ram_banks) {
				continue;
			}
			size_t addr = cheat.ram_bank * SRAM_SIZE + (cheat.address - SRAM_START);
			gbc->cart.ram[addr] = cheat.new_data;
		} else if (cheat.address >= WRAM0_START && cheat.address < WRAMX_END) {
			if (cheat.address < WRAMX_START) {
				gbc->memory.wram0[cheat.address - WRAM0_START] = cheat.new_data;
			} else {
				gbc->memory.wramx[cheat.address - WRAMX_START] = cheat.new_data;
			}
		}
	}
}

struct gbcc_gamegenie_cheat parse_gamegenie_code(const char code[9])
{
	struct gbcc_gamegenie_cheat cheat;
	cheat.new_data = (uint8_t)(hex2int(code[0]) << 4u) | hex2int(code[1]);
	cheat.old_data = (uint8_t)(hex2int(code[6]) << 4u) | hex2int(code[8]);
	cheat.old_data = (uint8_t)(cheat.old_data << 6u) | (cheat.old_data >> 2u);
	cheat.old_data ^= 0xBAu;
	cheat.address =
		(uint16_t)((hex2int(code[5]) ^ 0xFu) << 12u)
		| (uint16_t)(hex2int(code[2]) << 8u)
		| (uint16_t)(hex2int(code[3]) << 4u)
		| (uint16_t)(hex2int(code[4]) << 0u);
	/* Currently I ignore the unknown 8th character */
	/* TODO: short code format */
	return cheat;
}

struct gbcc_gameshark_cheat parse_gameshark_code(const char code[8])
{
	struct gbcc_gameshark_cheat cheat;
	cheat.ram_bank = (uint8_t)(hex2int(code[0]) << 4u) | hex2int(code[1]);
	cheat.new_data = (uint8_t)(hex2int(code[2]) << 4u) | hex2int(code[3]);
	cheat.address =
		(uint16_t)(hex2int(code[6]) << 12u)
		| (uint16_t)(hex2int(code[7]) << 8u)
		| (uint16_t)(hex2int(code[4]) << 4u)
		| (uint16_t)(hex2int(code[5]) << 0u);
	return cheat;
}

uint8_t hex2int(char c)
{
	if (c >= 'A' && c <= 'F') {
		return (uint8_t)(c - 'A') + 0xAu;
	} else if (c >= 'a' && c <= 'f') {
		return (uint8_t)(c - 'a') + 0xAu;
	} else if (c >= '0' && c <= '9') {
		return (uint8_t)(c - '0');
	} else {
		return 0xFFu;
	}
}
