/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "../bit_utils.h"
#include "../debug.h"
#include "../printer.h"
#include "../printer_platform.h"
#include "../audio.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef PRINTER_SOUND_PATH
#define PRINTER_SOUND_PATH "print.wav"
#endif

static void *print(void *printer);
static bool print_margin(struct printer *p, bool top);
static bool print_strip(struct printer *p);

void gbcc_printer_platform_start_printing(struct printer *p)
{
	pthread_create(&p->print_thread, NULL, print, p);
	pthread_setname_np(p->print_thread, "PrinterThread");
	return;
}

bool print_margin(struct printer *p, bool top) {
	if (top && p->margin.top_line >= p->margin.top_width) {
		return 1;
	} else if (!top && p->margin.bottom_line >= p->margin.bottom_width) {
		return 1;
	}
	for (int line = 0; line < PRINTER_STRIP_HEIGHT; line++) {
		for (uint8_t tx = 0; tx < PRINTER_WIDTH_TILES; tx++) {
			for (uint8_t x = 0; x < 8; x++) {
				printf("█");
			}
		}
		const struct timespec to_sleep = {.tv_sec = 0, .tv_nsec = 3000000};
		nanosleep(&to_sleep, NULL);
		printf("\n");
	}
	if (top) {
		p->margin.top_line++;
	} else {
		p->margin.bottom_line++;
	}
	return 0;
}

bool print_strip(struct printer *p)
{
	if (p->print_byte == p->image_buffer.length) {
		return 1;
	}
	unsigned int line;
	for (line = 0; (line < PRINTER_STRIP_HEIGHT) && (p->print_byte < p->image_buffer.length); line++) {
		uint8_t ty = (uint8_t)((line + p->print_line) / 8);
		for (uint8_t tx = 0; tx < PRINTER_WIDTH_TILES; tx++) {
			uint16_t idx = ty * PRINTER_WIDTH_TILES * 16 + tx * 16 + (uint8_t)(line + p->print_line - ty * 8) * 2;
			uint8_t lo = p->image_buffer.data[idx];
			uint8_t hi = p->image_buffer.data[idx + 1];
			for (uint8_t x = 0; x < 8; x++) {
				switch (gbcc_printer_get_palette_colour(p, (uint8_t)(check_bit(hi, 7 - x) << 1u) | check_bit(lo, 7 - x))) {
					case 0:
						printf("█");
						break;
					case 1:
						printf("▒");
						break;
					case 2:
						printf("░");
						break;
					case 3:
						printf(" ");
						break;
				}
			}
			p->print_byte += 2;
		}
		printf("\n");
		const struct timespec to_sleep = {.tv_sec = 0, .tv_nsec = 3000000};
		nanosleep(&to_sleep, NULL);
	}
	p->print_line += line;
	return 0;
}

void *print(void *printer)
{
	struct printer *p = (struct printer *)printer;
	int stage = 0;
	while (stage < 3) {
		gbcc_audio_play_wav(PRINTER_SOUND_PATH);
		const struct timespec to_sleep = {.tv_sec = 0, .tv_nsec = 850000000};
		nanosleep(&to_sleep, NULL);
		if (stage == 0) {
			stage += print_margin(p, true);
		}
		if (stage == 1) {
			stage += print_strip(p);
			if (p->print_byte == p->image_buffer.length && p->margin.bottom_width == 0) {
				break;
			}
		}
		if (stage == 2) {
			stage += print_margin(p, false);
			if (p->margin.bottom_line >= p->margin.bottom_width) {
				break;
			}
		}
	}
	gbcc_printer_initialise(p);
	return 0;
}
