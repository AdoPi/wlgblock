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
#include "debug.h"
#include "gbcc.h"
#include "memory.h"
#include "nelem.h"
#include "time_diff.h"
#include <stdint.h>
#include <time.h>

#define SYNC_FREQ 1024
#define SYNC_RESET_CLOCKS 512
#define CLOCKS_PER_SYNC (GBC_CLOCK_FREQ / SYNC_FREQ)
#define SLEEP_TIME (SECOND / SYNC_FREQ)
#define SLEEP_DETECT (SECOND / 10)

static const bool duty_table[4][8] = {
	{0, 0, 0, 0, 0, 0, 0, 1}, 	/* 00000001b */
	{1, 0, 0, 0, 0, 0, 0, 1}, 	/* 10000001b */
	{1, 0, 0, 0, 0, 1, 1, 1}, 	/* 10000111b */
	{0, 1, 1, 1, 1, 1, 1, 0}  	/* 01111110b */
};

static void length_counter_clock(struct channel *ch);
static uint16_t frequency_calc(struct sweep *sweep);
static bool timer_clock(struct timer *timer);
static void timer_reset(struct timer *timer);
static bool duty_clock(struct duty *duty);
static void envelope_clock(struct envelope *envelope);
static void time_sync(struct gbcc_core *gbc);
static void ch1_trigger(struct gbcc_core *gbc);
static void ch2_trigger(struct gbcc_core *gbc);
static void ch3_trigger(struct gbcc_core *gbc);
static void ch4_trigger(struct gbcc_core *gbc);

void gbcc_apu_init(struct gbcc_core *gbc)
{
	gbc->apu = (struct apu){0};
	gbc->apu.wave.addr = WAVE_START;
	clock_gettime(CLOCK_REALTIME, &gbc->apu.start_time);
}

ANDROID_INLINE
void gbcc_apu_clock(struct gbcc_core *gbc)
{
	struct apu *apu = &gbc->apu;
	if (!gbc->sync_to_video) {
		apu->sync_clock++;
		if (apu->sync_clock == CLOCKS_PER_SYNC) {
			apu->sync_clock = 0;
			apu->sample++;
			time_sync(gbc);
		}
	}

	if (apu->disabled) {
		return;
	}

	/* Duty */
	/* Duty cycle doesn't clock after powering on until first trigger */
	if (apu->ch1.duty.enabled) {
		apu->ch1.state = duty_clock(&apu->ch1.duty);
	}
	if (apu->ch2.duty.enabled) {
		apu->ch2.state = duty_clock(&apu->ch2.duty);
	}

	/* Noise */
	/*
	 * < 14 check is some obscure behaviour, where the lfsr isn't clocked
	 * if the shift is 14 or 15. Short-circuiting prevents timer_clock
	 * being called in this case.
	 */
	if (apu->noise.shift < 14 && timer_clock(&apu->noise.timer)) {
		uint8_t lfsr_low = apu->noise.lfsr & 0xFFu;
		uint8_t tmp = check_bit(lfsr_low, 0) ^ check_bit(lfsr_low, 1);
		apu->noise.lfsr >>= 1u;
		apu->noise.lfsr &= ~bit16(14);
		apu->noise.lfsr |= tmp * bit16(14);
		if (apu->noise.width_mode) {
			apu->noise.lfsr &= ~bit(6);
			apu->noise.lfsr |= tmp * bit(6);
		}
		apu->ch4.state = !check_bit16(apu->noise.lfsr, 0);
	}

	/* Wave */
	if (timer_clock(&apu->wave.timer)) {
		apu->wave.position++;
		apu->wave.position &= 31u;
		apu->wave.addr = WAVE_START + (apu->wave.position / 2);
		//printf("Wave clocked to %04X\n", apu->wave.addr);
		apu->wave.buffer = gbcc_memory_read_force(gbc, apu->wave.addr);
		/* Alternates between high & low nibble, high first */
		if (apu->wave.position % 2) {
			apu->wave.buffer &= 0x0Fu;
		} else {
			apu->wave.buffer >>= 4u;
		}
	}
}

void length_counter_clock(struct channel *ch)
{
	ch->counter--;
	if (ch->counter == 0) {
		ch->enabled = false;
	}
}

bool timer_clock(struct timer *timer)
{
	if (--timer->counter == 0) {
		timer_reset(timer);
		return true;
	}
	return false;
}

void timer_reset(struct timer *timer)
{
	timer->counter = timer->period;
}


bool duty_clock(struct duty *duty)
{
	duty->timer.period = (2048u - duty->freq) * 4;
	/*
	 * Manually clock the duty timer here for performance reasons, rather
	 * than calling timer_clock.
	 */
	if (duty->timer.counter == 1) {
		duty->counter++;
		duty->counter %= 8u;
		duty->timer.counter = duty->timer.period;
	} else {
		duty->timer.counter--;
	}

	return duty_table[duty->cycle][duty->counter];
}

void envelope_clock(struct envelope *envelope)
{
	if (!envelope->enabled) {
		return;
	}
	if (envelope->timer.period == 0) {
		envelope->timer.period = 8;
	}
	if (timer_clock(&envelope->timer)) {
		envelope->volume += envelope->dir;
		if (envelope->volume == 0x10u || envelope->volume == 0xFFu) {
			envelope->volume -= envelope->dir;
			envelope->enabled = false;
		}
	}
}

uint16_t frequency_calc(struct sweep *sweep)
{
	if (sweep->decreasing) {
		sweep->calculated = true;
		return sweep->freq - (sweep->freq >> sweep->shift);
	}
	return sweep->freq + (sweep->freq >> sweep->shift);
}

void gbcc_apu_sequencer_clock(struct gbcc_core *gbc)
{
	/* Length counters every other clock */
	if (!(gbc->apu.sequencer_counter & 0x01u)) {
		if (gbc->apu.ch1.length_enable && gbc->apu.ch1.enabled) {
			length_counter_clock(&gbc->apu.ch1);
		}
		if (gbc->apu.ch2.length_enable && gbc->apu.ch2.enabled) {
			length_counter_clock(&gbc->apu.ch2);
		}
		if (gbc->apu.ch3.length_enable && gbc->apu.ch3.enabled) {
			length_counter_clock(&gbc->apu.ch3);
		}
		if (gbc->apu.ch4.length_enable && gbc->apu.ch4.enabled) {
			length_counter_clock(&gbc->apu.ch4);
		}
	}

	/* Sweep on clocks 2 & 6 */
	if (gbc->apu.sweep.enabled && (gbc->apu.sequencer_counter == 2u || gbc->apu.sequencer_counter == 6u)) {
		if (timer_clock(&gbc->apu.sweep.timer) && gbc->apu.sweep.period != 0) {
			uint16_t freq = frequency_calc(&gbc->apu.sweep);
			if (gbc->apu.sweep.shift != 0 && freq < 2048) {
				gbc->apu.sweep.freq = freq;
				gbc->apu.ch1.duty.freq = gbc->apu.sweep.freq;
				gbcc_memory_write_force(gbc, NR13, freq & 0xFFu);
				uint8_t nr14 = gbcc_memory_read_force(gbc, NR14);
				nr14 &= ~0x07u;
				nr14 |= (freq & 0x0700u) >> 8u;
				gbcc_memory_write_force(gbc, NR14, nr14);
			}
			freq = frequency_calc(&gbc->apu.sweep);
			if (freq > 2047) {
				gbc->apu.ch1.enabled = false;
			}
		}
	}

	/* Envelope */
	if (gbc->apu.sequencer_counter == 7u) {
		envelope_clock(&gbc->apu.ch1.envelope);
		envelope_clock(&gbc->apu.ch2.envelope);
		envelope_clock(&gbc->apu.ch4.envelope);
	}

	gbc->apu.sequencer_counter++;
	gbc->apu.sequencer_counter &= 0x7u;
}

void time_sync(struct gbcc_core *gbc)
{
	clock_gettime(CLOCK_REALTIME, &gbc->apu.cur_time);
	uint64_t diff = gbcc_time_diff(&gbc->apu.cur_time, &gbc->apu.start_time);
	if (diff > SLEEP_DETECT + (SECOND * gbc->apu.sample) / SYNC_FREQ || gbc->keys.turbo) {
		gbc->apu.start_time = gbc->apu.cur_time;
		gbc->apu.sample = 0;
		return;
	}
	while (diff < (SECOND * gbc->apu.sample) / SYNC_FREQ) {
		const struct timespec time = {.tv_sec = 0, .tv_nsec = SLEEP_TIME};
		nanosleep(&time, NULL);
		clock_gettime(CLOCK_REALTIME, &gbc->apu.cur_time);
		diff = gbcc_time_diff(&gbc->apu.cur_time, &gbc->apu.start_time);
	}
	if (gbc->apu.sample > SYNC_RESET_CLOCKS) {
		gbc->apu.sample = 0;
		gbc->apu.start_time = gbc->apu.cur_time;
	}
}

void gbcc_apu_memory_write(struct gbcc_core *gbc, uint16_t addr, uint8_t val)
{
	uint8_t tmp;
	switch (addr) {
		case NR10:
			gbc->apu.sweep.period = (val & 0x70u) >> 4u;
			gbc->apu.sweep.timer.period = (val & 0x70u) >> 4u;
			if (gbc->apu.sweep.timer.period == 0) {
				gbc->apu.sweep.timer.period = 8;
			}
			if (!check_bit(val, 3)
					&& gbc->apu.sweep.decreasing
					&& gbc->apu.sweep.calculated) {
				gbc->apu.ch1.enabled = false;
			}
			gbc->apu.sweep.decreasing = check_bit(val, 3);
			gbc->apu.sweep.shift = val & 0x07u;
			break;
		case NR11:
			gbc->apu.ch1.duty.cycle = (val & 0xC0u) >> 6u;
			gbc->apu.ch1.counter = 64 - (val & 0x3Fu);
			break;
		case NR12:
			gbc->apu.ch1.dac = val & 0xF8u;
			if (!gbc->apu.ch1.dac) {
				gbc->apu.ch1.enabled = false;
			}
			gbc->apu.ch1.envelope.start_volume = (val & 0xF0u) >> 4u;
			gbc->apu.ch1.envelope.dir = (val & 0x08u) ? 1 : -1;
			gbc->apu.ch1.envelope.timer.period = val & 0x07u;

			// Obscure behaviour: writing a value in add mode with
			// 0 period increments the volume by one
			// TODO: This is not the full behaviour, but apparently
			// the only reliable bit
			if (gbc->apu.ch1.enabled && (val & 0x0Fu) == 0x08u) {
				gbc->apu.ch1.envelope.volume++;
				gbc->apu.ch1.envelope.volume &= 0x0Fu;
			}
			break;
		case NR13:
			gbc->apu.ch1.duty.freq &= ~0x00FFu;
			gbc->apu.ch1.duty.freq |= val;
			break;
		case NR14:
			tmp = gbc->apu.ch1.length_enable;
			gbc->apu.ch1.length_enable = check_bit(val, 6);
			if (gbc->apu.sequencer_counter & 0x01u
					&& !tmp
					&& check_bit(val, 6)
					&& gbc->apu.ch1.counter > 0) {
				/* Obscure extra length clock */
				length_counter_clock(&gbc->apu.ch1);
			}
			gbc->apu.ch1.duty.freq &= ~0xFF00u;
			gbc->apu.ch1.duty.freq |= (val & 0x07u) << 8u;
			if (check_bit(val, 7)) {
				ch1_trigger(gbc);
			}
			break;
		case NR20:
			/* Unused */
			break;
		case NR21:
			gbc->apu.ch2.duty.cycle = (val & 0xC0u) >> 6u;
			gbc->apu.ch2.counter = 64 - (val & 0x3Fu);
			break;
		case NR22:
			gbc->apu.ch2.dac = val & 0xF8u;
			if (!gbc->apu.ch2.dac) {
				gbc->apu.ch2.enabled = false;
			}
			gbc->apu.ch2.envelope.start_volume = (val & 0xF0u) >> 4u;
			gbc->apu.ch2.envelope.dir = (val & 0x08u) ? 1 : -1;
			gbc->apu.ch2.envelope.timer.period = val & 0x07u;

			// Obscure behaviour: writing a value in add mode with
			// 0 period increments the volume by one
			// TODO: This is not the full behaviour, but apparently
			// the only reliable bit
			if (gbc->apu.ch2.enabled && (val & 0x0Fu) == 0x08u) {
				gbc->apu.ch2.envelope.volume++;
				gbc->apu.ch2.envelope.volume &= 0x0Fu;
			}
			break;
		case NR23:
			gbc->apu.ch2.duty.freq &= ~0x00FFu;
			gbc->apu.ch2.duty.freq |= val;
			break;
		case NR24:
			tmp = gbc->apu.ch2.length_enable;
			gbc->apu.ch2.length_enable = check_bit(val, 6);
			if (gbc->apu.sequencer_counter & 0x01u
					&& !tmp
					&& check_bit(val, 6)
					&& gbc->apu.ch2.counter > 0) {
				/* Obscure extra length clock */
				length_counter_clock(&gbc->apu.ch2);
			}
			gbc->apu.ch2.duty.freq &= ~0xFF00u;
			gbc->apu.ch2.duty.freq |= (val & 0x07u) << 8u;
			if (check_bit(val, 7)) {
				ch2_trigger(gbc);
			}
			break;
		case NR30:
			gbc->apu.ch3.dac = check_bit(val, 7);
			if (!gbc->apu.ch3.dac) {
				gbc->apu.ch3.enabled = false;
			}
			break;
		case NR31:
			gbc->apu.ch3.counter = 256 - val;
			break;
		case NR32:
			gbc->apu.wave.shift = (val & 0x60u) >> 5u;
			break;
		case NR33:
			gbc->apu.wave.freq &= ~0x00FFu;
			gbc->apu.wave.freq |= val;
			gbc->apu.wave.timer.period = (2048u - gbc->apu.wave.freq) * 2;
			break;
		case NR34:
			tmp = gbc->apu.ch3.length_enable;
			gbc->apu.ch3.length_enable = check_bit(val, 6);
			if (gbc->apu.sequencer_counter & 0x01u
					&& !tmp
					&& check_bit(val, 6)
					&& gbc->apu.ch3.counter > 0) {
				/* Obscure extra length clock */
				length_counter_clock(&gbc->apu.ch3);
			}
			gbc->apu.wave.freq &= ~0xFF00u;
			gbc->apu.wave.freq |= (val & 0x07u) << 8u;
			gbc->apu.wave.timer.period = (2048u - gbc->apu.wave.freq) * 2;
			if (check_bit(val, 7)) {
				ch3_trigger(gbc);
			}
			break;
		case NR40:
			/* Unused */
			break;
		case NR41:
			gbc->apu.ch4.counter = 64 - (val & 0x3Fu);
			break;
		case NR42:
			gbc->apu.ch4.dac = val & 0xF8u;
			if (!gbc->apu.ch4.dac) {
				gbc->apu.ch4.enabled = false;
			}
			gbc->apu.ch4.envelope.start_volume = (val & 0xF0u) >> 4u;
			gbc->apu.ch4.envelope.dir = (val & 0x08u) ? 1 : -1;
			gbc->apu.ch4.envelope.timer.period = val & 0x07u;

			// Obscure behaviour: writing a value in add mode with
			// 0 period increments the volume by one
			// TODO: This is not the full behaviour, but apparently
			// the only reliable bit
			if (gbc->apu.ch4.enabled && (val & 0x0Fu) == 0x08u) {
				gbc->apu.ch4.envelope.volume++;
				gbc->apu.ch4.envelope.volume &= 0x0Fu;
			}
			break;
		case NR43:
			gbc->apu.noise.shift = (val & 0xF0u) >> 4u;
			gbc->apu.noise.width_mode = check_bit(val, 3);
			gbc->apu.noise.timer.period = (uint16_t)((val & 0x07u) << 4u);
			if (gbc->apu.noise.timer.period == 0) {
				gbc->apu.noise.timer.period = 0x08u;
			}
			gbc->apu.noise.timer.period <<= gbc->apu.noise.shift;
			break;
		case NR44:
			tmp = gbc->apu.ch4.length_enable;
			gbc->apu.ch4.length_enable = check_bit(val, 6);
			if (gbc->apu.sequencer_counter & 0x01u
					&& !tmp
					&& check_bit(val, 6)
					&& gbc->apu.ch4.counter > 0) {
				/* Obscure extra length clock */
				length_counter_clock(&gbc->apu.ch4);
			}
			if (check_bit(val, 7)) {
				ch4_trigger(gbc);
			}
			break;
		case NR50:
			gbc->apu.left_vol = (val & 0x70u) >> 4u;
			gbc->apu.right_vol = val & 0x07u;
			break;
		case NR51:
			gbc->apu.ch4.left = check_bit(val, 7);
			gbc->apu.ch3.left = check_bit(val, 6);
			gbc->apu.ch2.left = check_bit(val, 5);
			gbc->apu.ch1.left = check_bit(val, 4);
			gbc->apu.ch4.right = check_bit(val, 3);
			gbc->apu.ch3.right = check_bit(val, 2);
			gbc->apu.ch2.right = check_bit(val, 1);
			gbc->apu.ch1.right = check_bit(val, 0);
			break;
		case NR52:
			gbc->apu.disabled = !check_bit(val, 7);
			if (gbc->apu.disabled) {
				for (size_t i = NR10; i < NR52; i++) {
					gbc->memory.ioreg[i - IOREG_START] = 0;
				}
				gbcc_apu_init(gbc);
				gbc->apu.disabled = true;
			}
			break;
		default:
			gbcc_log_error("Invalid APU address 0x%04X\n", addr);
	}
}

void ch1_trigger(struct gbcc_core *gbc)
{
	uint8_t nr11 = gbcc_memory_read_force(gbc, NR11);
	uint8_t nr13 = gbcc_memory_read_force(gbc, NR13);
	uint8_t nr14 = gbcc_memory_read_force(gbc, NR14);
	gbc->apu.ch1.duty.enabled = true;
	gbc->apu.ch1.duty.cycle = (nr11 & 0xC0u) >> 6u;
	gbc->apu.ch1.duty.freq = nr13;
	gbc->apu.ch1.duty.freq |= (nr14 & 0x07u) << 8u;
	gbc->apu.ch1.enabled = true;
	if (gbc->apu.ch1.counter == 0) {
		if (gbc->apu.sequencer_counter & 0x01u && gbc->apu.ch1.length_enable) {
			gbc->apu.ch1.counter = 63;
		} else {
			gbc->apu.ch1.counter = 64;
		}
	}
	timer_reset(&gbc->apu.ch1.duty.timer);
	if (gbc->apu.ch1.envelope.timer.period > 0) {
		gbc->apu.ch1.envelope.enabled = true;
	} else {
		gbc->apu.ch1.envelope.enabled = false;
	}
	gbc->apu.ch1.envelope.volume = gbc->apu.ch1.envelope.start_volume;
	timer_reset(&gbc->apu.ch1.envelope.timer);
	gbc->apu.sweep.freq = gbc->apu.ch1.duty.freq;
	timer_reset(&gbc->apu.sweep.timer);
	gbc->apu.sweep.calculated = false;
	if (gbc->apu.sweep.shift == 0 && gbc->apu.sweep.period == 0) {
		gbc->apu.sweep.enabled = false;
	} else {
		gbc->apu.sweep.enabled = true;
	}
	if (gbc->apu.sweep.shift != 0) {
		uint16_t freq = frequency_calc(&gbc->apu.sweep);
		if (freq > 2047) {
			gbc->apu.ch1.enabled = false;
		}
	}
	if (!gbc->apu.ch1.dac) {
		gbc->apu.ch1.enabled = false;
	}
}

void ch2_trigger(struct gbcc_core *gbc)
{
	uint8_t nr21 = gbcc_memory_read_force(gbc, NR21);
	uint8_t nr23 = gbcc_memory_read_force(gbc, NR23);
	uint8_t nr24 = gbcc_memory_read_force(gbc, NR24);
	gbc->apu.ch2.duty.enabled = true;
	gbc->apu.ch2.duty.cycle = (nr21 & 0xC0u) >> 6u;
	gbc->apu.ch2.duty.freq = nr23;
	gbc->apu.ch2.duty.freq |= (nr24 & 0x07u) << 8u;
	gbc->apu.ch2.enabled = true;
	if (gbc->apu.ch2.counter == 0) {
		if (gbc->apu.sequencer_counter & 0x01u && gbc->apu.ch2.length_enable) {
			gbc->apu.ch2.counter = 63;
		} else {
			gbc->apu.ch2.counter = 64;
		}
	}
	timer_reset(&gbc->apu.ch2.duty.timer);
	if (gbc->apu.ch2.envelope.timer.period > 0) {
		gbc->apu.ch2.envelope.enabled = true;
	} else {
		gbc->apu.ch2.envelope.enabled = false;
	}
	gbc->apu.ch2.envelope.volume = gbc->apu.ch2.envelope.start_volume;
	timer_reset(&gbc->apu.ch2.envelope.timer);
	if (!gbc->apu.ch2.dac) {
		gbc->apu.ch2.enabled = false;
	}
}

void ch3_trigger(struct gbcc_core *gbc)
{
	gbc->apu.ch3.enabled = true;
	if (gbc->apu.ch3.counter == 0) {
		if (gbc->apu.sequencer_counter & 0x01u && gbc->apu.ch3.length_enable) {
			gbc->apu.ch3.counter = 255;
		} else {
			gbc->apu.ch3.counter = 256;
		}
	}
	timer_reset(&gbc->apu.wave.timer);
	gbc->apu.wave.addr = WAVE_START;
	gbc->apu.wave.position = 0;
	if (!gbc->apu.ch3.dac) {
		gbc->apu.ch3.enabled = false;
	}
}

void ch4_trigger(struct gbcc_core *gbc)
{
	gbc->apu.ch4.enabled = true;
	if (gbc->apu.ch4.counter == 0) {
		if (gbc->apu.sequencer_counter & 0x01u && gbc->apu.ch4.length_enable) {
			gbc->apu.ch4.counter = 63;
		} else {
			gbc->apu.ch4.counter = 64;
		}
	}
	timer_reset(&gbc->apu.noise.timer);
	if (gbc->apu.ch4.envelope.timer.period > 0) {
		gbc->apu.ch4.envelope.enabled = true;
	} else {
		gbc->apu.ch4.envelope.enabled = false;
	}
	gbc->apu.ch4.envelope.volume = gbc->apu.ch4.envelope.start_volume;
	timer_reset(&gbc->apu.ch4.envelope.timer);
	gbc->apu.noise.lfsr = 0xFFFFu;
	if (!gbc->apu.ch4.dac) {
		gbc->apu.ch4.enabled = false;
	}
}
