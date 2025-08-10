/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "audio.h"
#include "gbcc.h"

/* Max amplitude / no. channels / max global volume multiplier */
#define MAX_CHANNEL_AMPLITUDE (INT16_MAX / 4 / 0x10u)
/* Max channel amplitude / max envelope volume multiplier */
#define BASE_AMPLITUDE (MAX_CHANNEL_AMPLITUDE / 0x10u)

static void ch1_update(struct gbcc *gbc);
static void ch2_update(struct gbcc *gbc);
static void ch3_update(struct gbcc *gbc);
static void ch4_update(struct gbcc *gbc);


void gbcc_audio_initialise(struct gbcc *gbc, size_t sample_rate, size_t buffer_samples)
{
	struct gbcc_audio *audio = &gbc->audio;

	audio->sample_rate = sample_rate;
	audio->buffer_samples = buffer_samples;
	audio->buffer_bytes = buffer_samples * 2 * sizeof(*audio->mix_buffer);
	audio->clocks_per_sample = (float)GBC_CLOCK_FREQ / (float)sample_rate;
	audio->mix_buffer = calloc(buffer_samples * 2, sizeof(*audio->mix_buffer));
	audio->volume = 1.0f;
	gbcc_audio_platform_initialise(gbc);
}

void gbcc_audio_destroy(struct gbcc *gbc)
{
	gbcc_audio_platform_destroy(gbc);
	free(gbc->audio.mix_buffer);
}

ANDROID_INLINE
void gbcc_audio_update(struct gbcc *gbc)
{
	struct gbcc_audio *audio = &gbc->audio;

	float mult = 1;
	if (gbc->core.keys.turbo) {
		if (gbc->turbo_speed > 0) {
			mult = gbc->turbo_speed;
		} else {
			return;
		}
	}
	audio->clock++;
	if (gbc->core.sync_to_video) {
		mult /= audio->scale;
	}
	/* When err > 0, it tells us how much we overshot the last sample by */
	float err = audio->clock - audio->clocks_per_sample * mult * (float)audio->sample;
	if (err >= 0) {
		if (err < 1) {
			/*
			 * Some minor black magic to keep our audio buffers
			 * being filled at the correct rate. If we overshot
			 * this sample, we take the next sample slightly early.
			 *
			 * err < 1 is just a sanity check, so we don't mess
			 * everything up if there's a stutter.
			 */
			audio->clock += err;
		}
		if (audio->sample >= audio->buffer_samples) {
			gbcc_audio_platform_queue_buffer(gbc);
			audio->index = 0;
			audio->clock = 0;
			audio->sample = 0;
		}
		audio->sample++;
		audio->mix_buffer[audio->index] = 0;
		audio->mix_buffer[audio->index + 1] = 0;
		ch1_update(gbc);
		ch2_update(gbc);
		ch3_update(gbc);
		ch4_update(gbc);
		float left_vol = (1 + gbc->core.apu.left_vol) * audio->volume;
		float right_vol = (1 + gbc->core.apu.right_vol) * audio->volume;
		audio->mix_buffer[audio->index] = (int16_t)(audio->mix_buffer[audio->index] * left_vol);
		audio->mix_buffer[audio->index + 1] = (int16_t)(audio->mix_buffer[audio->index + 1] * right_vol);
		audio->index += 2;
	}
}

void ch1_update(struct gbcc *gbc)
{
	struct channel *ch1 = &gbc->core.apu.ch1;
	if (ch1->dac) {
		/* The DAC produces a signal in the range [-1, 1] */
		gbc->audio.mix_buffer[gbc->audio.index] -= MAX_CHANNEL_AMPLITUDE / 2;
		gbc->audio.mix_buffer[gbc->audio.index + 1] -= MAX_CHANNEL_AMPLITUDE / 2;
	}
	if (!ch1->enabled) {
		return;
	}
	uint8_t volume = ch1->envelope.volume;
	gbc->audio.mix_buffer[gbc->audio.index] += (GBCC_AUDIO_FMT)(ch1->left * ch1->state * volume * BASE_AMPLITUDE);
	gbc->audio.mix_buffer[gbc->audio.index + 1] += (GBCC_AUDIO_FMT)(ch1->right * ch1->state * volume * BASE_AMPLITUDE);
}

void ch2_update(struct gbcc *gbc)
{
	struct channel *ch2 = &gbc->core.apu.ch2;
	if (ch2->dac) {
		/* The DAC produces a signal in the range [-1, 1] */
		gbc->audio.mix_buffer[gbc->audio.index] -= MAX_CHANNEL_AMPLITUDE / 2;
		gbc->audio.mix_buffer[gbc->audio.index + 1] -= MAX_CHANNEL_AMPLITUDE / 2;
	}
	if (!ch2->enabled) {
		return;
	}
	uint8_t volume = ch2->envelope.volume;
	gbc->audio.mix_buffer[gbc->audio.index] += (GBCC_AUDIO_FMT)(ch2->left * ch2->state * volume * BASE_AMPLITUDE);
	gbc->audio.mix_buffer[gbc->audio.index + 1] += (GBCC_AUDIO_FMT)(ch2->right * ch2->state * volume * BASE_AMPLITUDE);
}

void ch3_update(struct gbcc *gbc)
{
	struct channel *ch3 = &gbc->core.apu.ch3;
	if (ch3->dac) {
		/* The DAC produces a signal in the range [-1, 1] */
		gbc->audio.mix_buffer[gbc->audio.index] -= MAX_CHANNEL_AMPLITUDE / 2;
		gbc->audio.mix_buffer[gbc->audio.index + 1] -= MAX_CHANNEL_AMPLITUDE / 2;
	}
	if (!ch3->enabled || gbc->core.apu.wave.shift == 0) {
		return;
	}
	gbc->audio.mix_buffer[gbc->audio.index] += (GBCC_AUDIO_FMT)(ch3->left * (gbc->core.apu.wave.buffer >> (gbc->core.apu.wave.shift - 1u)) * BASE_AMPLITUDE);
	gbc->audio.mix_buffer[gbc->audio.index + 1] += (GBCC_AUDIO_FMT)(ch3->right * (gbc->core.apu.wave.buffer >> (gbc->core.apu.wave.shift - 1u)) * BASE_AMPLITUDE);
}

void ch4_update(struct gbcc *gbc)
{
	struct channel *ch4 = &gbc->core.apu.ch4;
	if (ch4->dac) {
		/* The DAC produces a signal in the range [-1, 1] */
		gbc->audio.mix_buffer[gbc->audio.index] -= MAX_CHANNEL_AMPLITUDE / 2;
		gbc->audio.mix_buffer[gbc->audio.index + 1] -= MAX_CHANNEL_AMPLITUDE / 2;
	}
	if (!ch4->enabled) {
		return;
	}
	uint8_t volume = ch4->envelope.volume;
	gbc->audio.mix_buffer[gbc->audio.index] += (GBCC_AUDIO_FMT)(ch4->left * ch4->state * volume * BASE_AMPLITUDE);
	gbc->audio.mix_buffer[gbc->audio.index + 1] += (GBCC_AUDIO_FMT)(ch4->right * ch4->state * volume * BASE_AMPLITUDE);
}
