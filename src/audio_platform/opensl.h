/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_OPENSL_H
#define GBCC_OPENSL_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdbool.h>
#include <stdint.h>

struct gbcc_audio_platform {
	SLObjectItf engine_object;
	SLEngineItf engine;
	SLObjectItf output_mix;

	SLObjectItf player_object;
	SLPlayItf player;
	SLAndroidSimpleBufferQueueItf buffer_queue;
	SLmilliHertz sample_rate;
	uint16_t buffer_size;
	uint16_t *mix_buffers[4];
	uint16_t *playback_buffers[4];
	uint16_t read_buffer;
	uint16_t write_buffer;
};

#endif /* GBCC_OPENSL_H */
