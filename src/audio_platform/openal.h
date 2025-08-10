/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_OPENAL_H
#define GBCC_OPENAL_H

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

struct gbcc_audio_platform {
	ALCdevice *device;
	ALCcontext *context;
	ALuint source;
	ALuint buffers[8];
};

#endif /* GBCC_OPENAL_H */
