/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_MBC_H
#define GBCC_MBC_H

#include "constants.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

struct gbcc_core;

struct gbcc_mbc {
	enum MBC type;
	uint16_t rom0_bank;
	uint16_t romx_bank;
	uint8_t sram_bank;
	uint8_t ramg;
	uint8_t romb0;
	uint8_t romb1;
	uint8_t ramb;
	bool unlocked;
	bool sram_enable;
	bool sram_changed;
	time_t last_save_time;
	struct gbcc_rtc {
		struct timespec base_time;
		uint8_t seconds;
		uint8_t minutes;
		uint8_t hours;
		uint8_t day_low;
		uint8_t day_high;
		uint8_t latch;
		uint8_t cur_reg;
		bool mapped;
		bool halt;
	} rtc;
	struct gbcc_accelerometer {
		uint16_t x;
		uint16_t y;
		uint16_t real_x;
		uint16_t real_y;
		struct {
			bool up;
			bool down;
			bool left;
			bool right;
		} tilt;
		bool latch;
	} accelerometer;
	struct gbcc_eeprom {
		enum EEPROM_COMMAND current_command;
		uint16_t command;
		uint8_t command_bit;
		bool start;
		bool write_enable;
		bool DO;
		bool DI;
		bool CLK;
		bool CS;
		bool last_DI;
		bool last_CLK;
		bool last_CS;
		uint8_t value_bit;
		uint8_t address;
		uint16_t value;
		uint16_t data[128];
	} eeprom;
	struct gbcc_camera {
		uint32_t capture_timer;
		bool capture_request;
		bool mapped;
		uint8_t filter_mode;
		uint8_t matrix[4][4][3];
		/* M64282FP registers */
		uint8_t reg0;
		uint8_t reg1;
		uint8_t reg2;
		uint8_t reg3;
		uint8_t reg4;
		uint8_t reg5;
		uint8_t reg6;
		uint8_t reg7;
		struct {
			bool n; // Exclusive vertical edge enhacement
			uint8_t vh; // Vertical / horizontal edge mode
			uint8_t g; // Analogue output gain
			uint16_t exposure_steps; // Number of 16us steps
			uint8_t p; // 1D kernel "plus"
			uint8_t m; // 1D kernel "minus"
			uint8_t x; // 1D kernel "x" (always 0x01)
			uint8_t e; // Edge enhacement ratio
			bool i; // Invert output
			uint8_t v; // Output bias voltage
			uint8_t z; // Zero point calibration
			uint8_t o; // Output reference voltage
		} reg;
	} camera;
};

uint8_t gbcc_mbc_none_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_mbc1_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_mbc2_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_mbc3_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_mbc5_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_mbc6_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_mbc7_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_huc1_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_huc3_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_mmm01_read(struct gbcc_core *gbc, uint16_t addr);
uint8_t gbcc_mbc_cam_read(struct gbcc_core *gbc, uint16_t addr);
void gbcc_mbc_none_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_mbc1_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_mbc2_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_mbc3_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_mbc5_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_mbc6_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_mbc7_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_huc1_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_huc3_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_mmm01_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);
void gbcc_mbc_cam_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);

#endif /* GBCC_MBC_H */
