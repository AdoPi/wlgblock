#ifndef GBCC_CHEATS_H
#define GBCC_CHEATS_H

#include <stdbool.h>
#include <stdint.h>

struct gbcc_core;

struct gbcc_gamegenie_cheat {
	uint16_t address;
	uint8_t old_data;
	uint8_t new_data;
};

struct gbcc_gameshark_cheat {
	uint16_t address;
	uint8_t ram_bank;
	uint8_t new_data;
};

void gbcc_cheats_add_fuzzy(struct gbcc_core *gbc, const char *code);
void gbcc_cheats_add_gamegenie(struct gbcc_core *gbc, const char *code);
void gbcc_cheats_add_gameshark(struct gbcc_core *gbc, const char *code);
uint8_t gbcc_cheats_gamegenie_read(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_cheats_gameshark_update(struct gbcc_core *gbc);

#endif /* GBCC_CHEATS_H */
