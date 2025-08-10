/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef WAVE_H
#define WAVE_H

#include <stdint.h>
#include <stdio.h>

struct wav_header {
	char ChunkID[4];
	uint32_t ChunkSize;
	char Format[4];

	char Subchunk1ID[4];
	uint32_t Subchunk1Size;
	uint16_t AudioFormat;
	uint16_t NumChannels;
	uint32_t SampleRate;
	uint32_t ByteRate;
	uint16_t BlockAlign;
	uint16_t BitsPerSample;

	char Subchunk2ID[4];
	uint32_t Subchunk2Size;
};

void wav_parse_header(struct wav_header *header, FILE *wav);
void wav_print_header(struct wav_header *header);

#endif /* WAVE_H */
