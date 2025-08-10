/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_APU_H
#define GBCC_APU_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

struct gbcc_core;

struct timer {
	uint16_t period;
	uint16_t counter;
};

struct duty {
	struct timer timer;
	uint8_t counter;
	uint8_t cycle;
	uint16_t freq;
	bool enabled;
};

struct sweep {
	struct timer timer;
	uint16_t freq;
	uint16_t period;
	uint8_t shift;
	bool decreasing;
	bool enabled;
	bool calculated;
};

struct noise {
	struct timer timer;
	uint8_t shift;
	bool width_mode;
	uint16_t lfsr;
};

struct wave {
	struct timer timer;
	uint16_t addr;
	uint16_t freq;
	uint8_t buffer;
	uint8_t position;
	uint8_t shift;
};

struct envelope {
	struct timer timer;
	uint8_t start_volume;
	uint8_t volume;
	int8_t dir;
	bool enabled;
};

struct channel {
	struct envelope envelope;
	struct duty duty;
	uint16_t counter;
	bool length_enable;
	bool state;
	bool enabled;
	bool dac;
	bool left;
	bool right;
};

struct apu {
	uint16_t sync_clock;
	uint16_t sample;
	uint8_t left_vol;
	uint8_t right_vol;
	bool disabled;
	bool div_bit;
	struct timespec cur_time;
	struct timespec start_time;
	struct channel ch1; 	/* Tone & Sweep */
	struct channel ch2; 	/* Tone */
	struct channel ch3; 	/* Wave Output */
	struct channel ch4; 	/* Noise */
	struct sweep sweep;
	struct noise noise;
	struct wave wave;
	uint8_t sequencer_counter;
};

void gbcc_apu_init(struct gbcc_core *gbc);
void gbcc_apu_clock(struct gbcc_core *gbc);
void gbcc_apu_sequencer_clock(struct gbcc_core *gbc);
void gbcc_apu_memory_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val);

#endif /* GBCC_AUDIO_H */
