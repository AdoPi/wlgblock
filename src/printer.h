/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_PRINTER_H
#define GBCC_PRINTER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define GBC_PRINTER_IMAGE_BUFFER_SIZE 0x2000

#define PRINTER_WIDTH_TILES 20
#define PRINTER_STRIP_HEIGHT 16

struct printer {
	struct {
		uint8_t data[GBC_PRINTER_IMAGE_BUFFER_SIZE];
		uint16_t length;
	} image_buffer;
	struct packet {
		uint8_t command;
		uint8_t compression;
		uint16_t data_length;
		uint16_t printer_checksum;
		uint16_t gb_checksum;
		uint16_t current_byte; 	/* Which byte are we about to read? */
		uint16_t data_byte;
	} packet;
	struct {
		uint8_t top_width;
		uint8_t top_line;
		uint8_t bottom_width;
		uint8_t bottom_line;
	} margin;
	uint8_t palette;
	uint8_t exposure;
	uint8_t status;
	bool magic; 	/* Flag for detecting magic bytes that start a packet */
	bool in_packet;
	uint16_t print_byte;
	uint8_t print_line;
	pthread_t print_thread;
};

void gbcc_printer_initialise(struct printer *p);
uint8_t gbcc_printer_parse_byte(struct printer *p, uint8_t byte);
uint8_t gbcc_printer_get_palette_colour(struct printer *p, uint8_t colour);

#endif /* GBCC_PRINTER_H */
