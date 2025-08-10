/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../gbcc.h"
#include "../audio.h"
#include "../bit_utils.h"
#include "../debug.h"
#include "../memory.h"
#include "../nelem.h"
#include "../time_diff.h"
#include "../wav.h"

static void buffer_callback(SLAndroidSimpleBufferQueueItf bq, void *_audio);

void gbcc_audio_platform_initialise(struct gbcc *gbc)
{
	struct gbcc_audio *audio = &gbc->audio;
	struct gbcc_audio_platform *sl = &audio->platform;
	audio->scale = 0.9955;

	for (size_t i = 0; i < N_ELEM(sl->mix_buffers); i++) {
		sl->mix_buffers[i] = calloc(audio->buffer_samples * 2, sizeof(*sl->mix_buffers[i]));
		sl->playback_buffers[i] = calloc(audio->buffer_samples * 2, sizeof(*sl->playback_buffers[i]));
	}
	sl->write_buffer = 1;
	sl->read_buffer = 0;

	SLresult result;

	/* Create & realize the engine and output mix */
	result = slCreateEngine(&sl->engine_object, 0, NULL, 0, NULL, NULL);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to create audio engine.\n");
		exit(EXIT_FAILURE);
	}

	result = (*sl->engine_object)->Realize(sl->engine_object, SL_BOOLEAN_FALSE);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to realise audio engine.\n");
		exit(EXIT_FAILURE);
	}

	result = (*sl->engine_object)->GetInterface(sl->engine_object, SL_IID_ENGINE, &sl->engine);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to get audio engine interface.\n");
		exit(EXIT_FAILURE);
	}

	result = (*sl->engine)->CreateOutputMix(sl->engine, &sl->output_mix, 0, NULL, NULL);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to create output mix object.\n");
		exit(EXIT_FAILURE);
	}

	result = (*sl->output_mix)->Realize(sl->output_mix, SL_BOOLEAN_FALSE);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to realise output mix.\n");
		exit(EXIT_FAILURE);
	}

	/* Create the buffer queue player */
	SLDataLocator_AndroidSimpleBufferQueue bq_locator = {
		.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
		.numBuffers = 4
	};

	SLDataFormat_PCM format = {
		.formatType = SL_DATAFORMAT_PCM,
		.numChannels = 2,
		.samplesPerSec = audio->sample_rate * 1000,
		.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16,
		.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16,
		.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
		.endianness = SL_BYTEORDER_LITTLEENDIAN
	};

	SLDataSource source = {
		.pLocator = &bq_locator,
		.pFormat = &format
	};

	SLDataLocator_OutputMix outmix_locator = {
		.locatorType = SL_DATALOCATOR_OUTPUTMIX,
		.outputMix = sl->output_mix
	};

	SLDataSink sink = {
		.pLocator = &outmix_locator,
		.pFormat = NULL
	};

	const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
	const SLboolean req[1] = {SL_BOOLEAN_TRUE};

	result = (*sl->engine)->CreateAudioPlayer(sl->engine, &sl->player_object, &source, &sink, 1, ids, req);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to create audio player.\n");
		exit(EXIT_FAILURE);
	}

	result = (*sl->player_object)->Realize(sl->player_object, SL_BOOLEAN_FALSE);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to realise audio player.\n");
		exit(EXIT_FAILURE);
	}

	result = (*sl->player_object)->GetInterface(sl->player_object, SL_IID_PLAY, &sl->player);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to get player interface.\n");
		exit(EXIT_FAILURE);
	}

	result = (*sl->player_object)->GetInterface(sl->player_object, SL_IID_BUFFERQUEUE, &sl->buffer_queue);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to get buffer queue interface.\n");
		exit(EXIT_FAILURE);
	}

	result = (*sl->buffer_queue)->RegisterCallback(sl->buffer_queue, buffer_callback, audio);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to get buffer queue interface.\n");
		exit(EXIT_FAILURE);
	}

	result = (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->playback_buffers[0], audio->buffer_bytes);
	result |= (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->playback_buffers[1], audio->buffer_bytes);
	result |= (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->playback_buffers[2], audio->buffer_bytes);
	result |= (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->playback_buffers[3], audio->buffer_bytes);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to queue buffer.\n");
		exit(EXIT_FAILURE);
	}

	// Start playback
	result = (*sl->player)->SetPlayState(sl->player, SL_PLAYSTATE_PLAYING);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("Failed to get start playback.\n");
		exit(EXIT_FAILURE);
	}
}

void gbcc_audio_platform_destroy(struct gbcc *gbc) {
	struct gbcc_audio *audio = &gbc->audio;
	struct gbcc_audio_platform *sl = &audio->platform;

	for (size_t i = 0; i < N_ELEM(sl->mix_buffers); i++) {
		free(sl->mix_buffers[i]);
		free(sl->playback_buffers[i]);
	}

	(*sl->player_object)->Destroy(sl->player_object);
	(*sl->output_mix)->Destroy(sl->output_mix);
	(*sl->engine_object)->Destroy(sl->engine_object);
	*sl = (struct gbcc_audio_platform){0};
}

void gbcc_audio_platform_queue_buffer(struct gbcc *gbc)
{
	struct gbcc_audio *audio = &gbc->audio;
	struct gbcc_audio_platform *sl = &audio->platform;

	sl->write_buffer = (sl->write_buffer + 1) % 4;
	while (sl->read_buffer == sl->write_buffer) {
		const struct timespec time = {.tv_sec = 0, .tv_nsec = 100000};
		nanosleep(&time, NULL);
	}
	memcpy(sl->mix_buffers[sl->write_buffer], audio->mix_buffer, audio->buffer_bytes);
}

void gbcc_audio_play_wav(const char *filename)
{
	gbcc_log_error("Stubbed function \"gbcc_audio_play_wav()\" called.");
}

void buffer_callback(SLAndroidSimpleBufferQueueItf bq, void *_audio) {
	struct gbcc_audio *audio = (struct gbcc_audio *)_audio;
	struct gbcc_audio_platform *sl = &audio->platform;
	memcpy(sl->playback_buffers[sl->read_buffer], sl->mix_buffers[sl->read_buffer], audio->buffer_bytes);
	memset(sl->mix_buffers[sl->read_buffer], 0, audio->buffer_bytes);
	sl->read_buffer = (sl->read_buffer + 1) % 4;
	SLresult result = (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->playback_buffers[sl->read_buffer], audio->buffer_bytes);
	if (result != SL_RESULT_SUCCESS) {
		gbcc_log_error("OpenSLES failed to enqueue buffer.\n");
	}
}
