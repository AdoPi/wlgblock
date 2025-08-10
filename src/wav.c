/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "debug.h"
#include "wav.h"

void wav_parse_header(struct wav_header *header, FILE *wav)
{
	if (fread(header->ChunkID, 1, 4, wav) != 4) {
		gbcc_log_error("Failed to read wav ChunkID.\n");
		return;
	}
	if (fread(&header->ChunkSize, 4, 1, wav) != 1) {
		gbcc_log_error("Failed to read wav ChunkSize.\n");
		return;
	}
	if (fread(header->Format, 1, 4, wav) != 4) {
		gbcc_log_error("Failed to read wav Format.\n");
		return;
	}
	if (fread(header->Subchunk1ID, 1, 4, wav) != 4) {
		gbcc_log_error("Failed to read wav Subchunk1ID.\n");
		return;
	}
	if (fread(&header->Subchunk1Size, 4, 1, wav) != 1) {
		gbcc_log_error("Failed to read wav Subchunk1Size.\n");
		return;
	}
	if (fread(&header->AudioFormat, 2, 1, wav) != 1) {
		gbcc_log_error("Failed to read wav AudioFormat.\n");
		return;
	}
	if (fread(&header->NumChannels, 2, 1, wav) != 1) {
		gbcc_log_error("Failed to read wav NumChannels.\n");
		return;
	}
	if (fread(&header->SampleRate, 4, 1, wav) != 1) {
		gbcc_log_error("Failed to read wav SampleRate.\n");
		return;
	}
	if (fread(&header->ByteRate, 4, 1, wav) != 1) {
		gbcc_log_error("Failed to read wav ByteRate.\n");
		return;
	}
	if (fread(&header->BlockAlign, 2, 1, wav) != 1) {
		gbcc_log_error("Failed to read wav BlockAlign.\n");
		return;
	}
	if (fread(&header->BitsPerSample, 2, 1, wav) != 1) {
		gbcc_log_error("Failed to read wav BitsPerSample.\n");
		return;
	}
	if (fread(header->Subchunk2ID, 1, 4, wav) != 4) {
		gbcc_log_error("Failed to read wav Subchunk2ID.\n");
		return;
	}
	if (fread(&header->Subchunk2Size, 4, 1, wav) != 1) {
		gbcc_log_error("Failed to read wav Subchunk2Size.\n");
		return;
	}
}

void wav_print_header(struct wav_header *header)
{
	//printf(header->ChunkID, 1, 4, wav);
	printf("ChunkSize:\t0x%08X\n", header->ChunkSize);
	//printf("Format:\t0x%08X\n", header->Format);

	//printf("Subchunk1ID:\t0x%08X\n", header->Subchunk1ID);
	printf("Subchunk1Size:\t0x%08X\n", header->Subchunk1Size);
	printf("AudioFormat:\t0x%04X\n", header->AudioFormat);
	printf("NumChannels:\t0x%04X\n", header->NumChannels);
	printf("SampleRate:\t0x%08X\n", header->SampleRate);
	printf("ByteRate:\t0x%08X\n", header->ByteRate);
	printf("BlockAlign:\t0x%04X\n", header->BlockAlign);
	printf("BitsPerSample:\t0x%04X\n", header->BitsPerSample);

	//printf("Subchunk2ID:\t0x%08X\n", header->Subchunk2ID);
	printf("Subchunk2Size:\t0x%08X\n", header->Subchunk2Size);
}
