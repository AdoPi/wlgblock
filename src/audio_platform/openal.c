/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "../gbcc.h"
#include "../audio.h"
#include "../bit_utils.h"
#include "../debug.h"
#include "../memory.h"
#include "../nelem.h"
#include "../time_diff.h"
#include "../wav.h"

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int check_openal_error(const char *msg);
static void *wav_thread(void *filename);

void gbcc_audio_platform_initialise(struct gbcc *gbc)
{
	struct gbcc_audio *audio = &gbc->audio;
	audio->scale = 0.9955f;
	audio->platform.device = alcOpenDevice(NULL);
	if (!audio->platform.device) {
		gbcc_log_error("Failed to open audio device.\n");
	}
	audio->platform.context = alcCreateContext(audio->platform.device, NULL);
	if (!audio->platform.context) {
		gbcc_log_error("Failed to create OpenAL context.\n");
		exit(EXIT_FAILURE);
	}
	if (!alcMakeContextCurrent(audio->platform.context)) {
		gbcc_log_error("Failed to set OpenAL context.\n");
		exit(EXIT_FAILURE);
	}

	alGenSources(1, &audio->platform.source);
	if (check_openal_error("Failed to create source.\n")) {
		exit(EXIT_FAILURE);
	}

	alSourcef(audio->platform.source, AL_PITCH, 1);
	if (check_openal_error("Failed to set pitch.\n")) {
		exit(EXIT_FAILURE);
	}
	alSourcef(audio->platform.source, AL_GAIN, 1);
	if (check_openal_error("Failed to set gain.\n")) {
		exit(EXIT_FAILURE);
	}
	alSource3f(audio->platform.source, AL_POSITION, 0, 0, 0);
	if (check_openal_error("Failed to set position.\n")) {
		exit(EXIT_FAILURE);
	}
	alSource3f(audio->platform.source, AL_VELOCITY, 0, 0, 0);
	if (check_openal_error("Failed to set velocity.\n")) {
		exit(EXIT_FAILURE);
	}
	alSourcei(audio->platform.source, AL_LOOPING, AL_FALSE);
	if (check_openal_error("Failed to set loop.\n")) {
		exit(EXIT_FAILURE);
	}

	alGenBuffers(N_ELEM(audio->platform.buffers), audio->platform.buffers);
	if (check_openal_error("Failed to create buffers.\n")) {
		exit(EXIT_FAILURE);
	}

	memset(audio->mix_buffer, 0, audio->buffer_bytes);
	for (size_t i = 0; i < N_ELEM(audio->platform.buffers); i++) {
		alBufferData(
				audio->platform.buffers[i],
				AL_FORMAT_STEREO16,
				audio->mix_buffer,
				(ALsizei)audio->buffer_bytes,
				(ALsizei)audio->sample_rate
			    );
	}
	alSourceQueueBuffers(audio->platform.source, N_ELEM(audio->platform.buffers), audio->platform.buffers);
	check_openal_error("Failed to queue buffers.\n");
	alSourcePlay(audio->platform.source);
	check_openal_error("Failed to play audio.\n");
}

void gbcc_audio_platform_destroy(struct gbcc *gbc) {
	alDeleteSources(1, &gbc->audio.platform.source);
	alDeleteBuffers(N_ELEM(gbc->audio.platform.buffers), gbc->audio.platform.buffers);
	alcDestroyContext(gbc->audio.platform.context);
	alcCloseDevice(gbc->audio.platform.device);
}

ANDROID_INLINE
void gbcc_audio_platform_queue_buffer(struct gbcc *gbc)
{
	struct gbcc_audio *audio = &gbc->audio;
	ALint processed = 0;
	alGetSourcei(audio->platform.source, AL_BUFFERS_PROCESSED, &processed);
	while (!processed) {
		const struct timespec time = {.tv_sec = 0, .tv_nsec = 100000};
		nanosleep(&time, NULL);
		alGetSourcei(audio->platform.source, AL_BUFFERS_PROCESSED, &processed);
	}
	ALuint buffer;
	alSourceUnqueueBuffers(audio->platform.source, 1, &buffer);
	check_openal_error("Failed to unqueue buffer.\n");
	alBufferData(buffer, AL_FORMAT_STEREO16, audio->mix_buffer, (ALsizei)audio->buffer_bytes, (ALsizei)audio->sample_rate);
	check_openal_error("Failed to fill buffer.\n");
	alSourceQueueBuffers(audio->platform.source, 1, &buffer);
	check_openal_error("Failed to queue buffer.\n");
	ALint state;
	alGetSourcei(audio->platform.source, AL_SOURCE_STATE, &state);
	check_openal_error("Failed to get source state.\n");
	if (state == AL_STOPPED) {
		clock_gettime(CLOCK_REALTIME, &gbc->core.apu.start_time);
		gbc->core.apu.sample = 0;
		alSourcePlay(audio->platform.source);
		check_openal_error("Failed to resume audio playback.\n");
	}
}


int check_openal_error(const char *msg)
{
	ALenum error = alGetError();
	switch (error) {
		case AL_NO_ERROR:
			return 0;
		case AL_INVALID_NAME:
			gbcc_log_error("Invalid name: %s", msg);
			break;
		case AL_INVALID_ENUM:
			gbcc_log_error("Invalid enum: %s", msg);
			break;
		case AL_INVALID_VALUE:
			gbcc_log_error("Invalid value: %s", msg);
			break;
		case AL_INVALID_OPERATION:
			gbcc_log_error("Invalid operation: %s", msg);
			break;
		case AL_OUT_OF_MEMORY:
			gbcc_log_error("Out of memory: %s", msg);
			break;
	}
	gbcc_log_error("%s", msg);
	return 1;
}


void gbcc_audio_play_wav(const char *filename)
{
	pthread_t thread;
	pthread_create(&thread, NULL, wav_thread, (void *)filename);
	pthread_detach(thread);
}

void *wav_thread(void *filename) {
	ALuint source;
	alGenSources(1, &source);
	if (check_openal_error("Failed to create source.\n")) {
		return NULL;
	}

	alSourcef(source, AL_PITCH, 1);
	if (check_openal_error("Failed to set pitch.\n")) {
		goto CLEANUP_SOURCE;
	}
	alSourcef(source, AL_GAIN, 1);
	if (check_openal_error("Failed to set gain.\n")) {
		goto CLEANUP_SOURCE;
	}
	alSource3f(source, AL_POSITION, 0, 0, 0);
	if (check_openal_error("Failed to set position.\n")) {
		goto CLEANUP_SOURCE;
	}
	alSource3f(source, AL_VELOCITY, 0, 0, 0);
	if (check_openal_error("Failed to set velocity.\n")) {
		goto CLEANUP_SOURCE;
	}
	alSourcei(source, AL_LOOPING, AL_FALSE);
	if (check_openal_error("Failed to set loop.\n")) {
		goto CLEANUP_SOURCE;
	}

	ALuint buffer;
	alGenBuffers(1, &buffer);
	if (check_openal_error("Failed to create buffer.\n")) {
		goto CLEANUP_SOURCE;
	}

	FILE *wav = fopen(filename, "rb");
	if (!wav) {
		gbcc_log_error("Failed to open print sound file.\n");
		goto CLEANUP_SOURCE;
	}
	struct wav_header header;
	wav_parse_header(&header, wav);
	if (header.AudioFormat != 1) {
		gbcc_log_error("Only PCM files are supported.\n");
		fclose(wav);
		goto CLEANUP_SOURCE;
	}
	uint8_t *data = malloc(header.Subchunk2Size);
	if (!data) {
		gbcc_log_error("Failed to allocate audio data buffer.\n");
		fclose(wav);
		goto CLEANUP_SOURCE;
	}
	if (fread(data, 1, header.Subchunk2Size, wav) != header.Subchunk2Size) {
		gbcc_log_error("Failed to read print audio data.\n");
		fclose(wav);
		goto CLEANUP_ALL;
	}
	fclose(wav);

	alBufferData(buffer, AL_FORMAT_MONO8, data, (ALsizei)header.Subchunk2Size, (ALsizei)header.SampleRate);
	if (check_openal_error("Failed to set buffer data.\n")) {
		goto CLEANUP_ALL;
	}
	alSourcei(source, AL_BUFFER, (ALint)buffer);
	if (check_openal_error("Failed to set source buffer.\n")) {
		goto CLEANUP_ALL;
	}


	unsigned long long ns = SECOND * header.Subchunk2Size / header.ByteRate;

	struct timespec tts = {
		.tv_sec = ns / SECOND,
		.tv_nsec = ns % SECOND
	};
	alSourcePlay(source);
	nanosleep(&tts, NULL);

CLEANUP_ALL:
	free(data);
CLEANUP_SOURCE:
	alDeleteSources(1, &source);
	check_openal_error("Failed to delete source.\n");
	return NULL;
}
