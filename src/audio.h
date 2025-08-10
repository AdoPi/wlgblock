/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_AUDIO_H
#define GBCC_AUDIO_H

#ifdef __ANDROID__
#include "audio_platform/opensl.h"
#else
#include "audio_platform/openal.h"
#endif
#include <stdint.h>
#include <time.h>

#define GBCC_AUDIO_FMT int16_t

struct gbcc;

struct gbcc_audio {
	struct gbcc_audio_platform platform;
	float clock;
	unsigned int sample;
	size_t index;
	size_t sample_rate;
	size_t buffer_samples;
	size_t buffer_bytes;
	float clocks_per_sample;
	float scale;
	float volume;
	GBCC_AUDIO_FMT *mix_buffer;
};

void gbcc_audio_initialise(struct gbcc *gbc, size_t sample_rate, size_t buffer_samples);
void gbcc_audio_destroy(struct gbcc *gbc);
void gbcc_audio_update(struct gbcc *gbc);
void gbcc_audio_play_wav(const char *filename);

void gbcc_audio_platform_initialise(struct gbcc *gbc);
void gbcc_audio_platform_destroy(struct gbcc *gbc);
void gbcc_audio_platform_queue_buffer(struct gbcc *gbc);

#endif /* GBCC_AUDIO_H */
