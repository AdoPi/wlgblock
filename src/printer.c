/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "bit_utils.h"
#include "debug.h"
#include "printer.h"
#include "printer_platform.h"
#include "audio.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Magic bytes that mark the start of a command packet */
#define MAGIC_BYTE_1 0x88u
#define MAGIC_BYTE_2 0x33u

/* Meaning of command bytes */
#define INITALISE 0x01u
#define START_PRINTING 0x02u
#define FILL_BUFFER 0x04u
#define READ_STATUS 0x0Fu

/* Order of bytes in packet, ignoring length of data */
#define COMMAND 0
#define COMPRESSION 1
#define LENGTH_LSB 2
#define LENGTH_MSB 3
#define DATA 4
#define CHECKSUM_LSB 5
#define CHECKSUM_MSB 6
#define ALIVE_INDICATOR 7
#define STATUS 8

/* Meaning of status flag bits */
#define STATUS_CHECKSUM_ERROR 0
#define STATUS_PRINTING 1
#define STATUS_DATA_FULL 2
#define STATUS_DATA_UNPROCESSED 3
#define STATUS_PACKET_ERROR 4
#define STATUS_PAPER_JAM 5
#define STATUS_HEAT_ERROR 6
#define STATUS_LOW_BATTERY 7

static void check_magic(struct printer *p, uint8_t byte);
static void execute(struct printer *p);
static void fill_buffer(struct printer *p, uint8_t byte);
static void parse_print_args(struct printer *p, uint8_t byte);

void gbcc_printer_initialise(struct printer *p)
{
	*p = (struct printer){0};
}

uint8_t gbcc_printer_parse_byte(struct printer *p, uint8_t byte)
{
	if (!p->in_packet) {
		check_magic(p, byte);
		return 0;
	}
	switch(p->packet.current_byte) {
		case COMMAND:
			p->packet.command = byte;
			p->packet.current_byte++;
			p->packet.printer_checksum += byte;
			break;
		case COMPRESSION:
			/* TODO: Handle compression */
			p->packet.compression = byte;
			p->packet.current_byte++;
			p->packet.printer_checksum += byte;
			break;
		case LENGTH_LSB:
			p->packet.data_length = byte;
			p->packet.current_byte++;
			p->packet.printer_checksum += byte;
			break;
		case LENGTH_MSB:
			p->packet.data_length |= byte << 8u;
			p->packet.current_byte++;
			p->packet.printer_checksum += byte;
			break;
		case DATA:
			if (p->packet.data_byte < p->packet.data_length) {
				switch (p->packet.command) {
					case INITALISE:
						/*
						 * INITIALISE has unknown
						 * function, but seems to have
						 * no effect
						 */
						break;
					case START_PRINTING:
						parse_print_args(p, byte);
						break;
					case FILL_BUFFER:
						fill_buffer(p, byte);
						break;
					case READ_STATUS:
						/* READ_STATUS is a no-op */
						break;
				}
				p->packet.data_byte++;
				p->packet.printer_checksum += byte;
				p->status = set_bit(p->status, STATUS_DATA_UNPROCESSED);
				break;
			}
			p->packet.current_byte++;
			/* Fallthrough */
		case CHECKSUM_LSB:
			p->packet.gb_checksum = byte;
			p->packet.current_byte++;
			break;
		case CHECKSUM_MSB:
			p->packet.gb_checksum |= byte << 8u;
			p->packet.current_byte++;
			break;
		case ALIVE_INDICATOR:
			p->packet.current_byte++;
			return 0x81u;
		case STATUS:
			p->magic = 0;
			p->in_packet = 0;
			if (p->packet.gb_checksum != p->packet.printer_checksum) {
				gbcc_log_error("Printer checksum incorrect "
						"(Expected 0x%04X, "
						"Calculated 0x%04X).\n",
						p->packet.printer_checksum,
						p->packet.gb_checksum);
				p->status = set_bit(p->status, STATUS_CHECKSUM_ERROR);
			}
			{
				uint8_t tmp = p->status;
				execute(p);
				p->packet = (struct packet){0};
				return tmp;
			}
	}
	return 0;
}

uint8_t gbcc_printer_get_palette_colour(struct printer *p, uint8_t colour) {
	uint8_t colours[4] = {
		(p->palette & 0x03u) >> 0u,
		(p->palette & 0x0Cu) >> 2u,
		(p->palette & 0x30u) >> 4u,
		(p->palette & 0xC0u) >> 6u
	};
	return colours[colour];
}

void check_magic(struct printer *p, uint8_t byte)
{
	switch (byte) {
		case MAGIC_BYTE_1:
			p->magic = 1;
			break;
		case MAGIC_BYTE_2:
			if (p->magic) {
				p->in_packet = true;
			}
			/* Fallthrough */
		default:
			p->magic = 0;
	}
}

void execute(struct printer *p)
{
	switch (p->packet.command) {
		case 0x1u:
			if (check_bit(p->status, STATUS_PRINTING)) {
				return;
			}
			gbcc_printer_initialise(p);
			break;
		case 0x2u:
			if (check_bit(p->status, STATUS_PRINTING)) {
				return;
			}
			p->status = set_bit(p->status, STATUS_PRINTING);
			gbcc_printer_platform_start_printing(p);
			break;
		case 0x4u:
			break;
		case 0xFu:
			break;
		default:
			gbcc_log_error("Invalid printer command %02X.\n", p->packet.command);
			break;
	}
}

void fill_buffer(struct printer *p, uint8_t byte)
{
	if (p->image_buffer.length < GBC_PRINTER_IMAGE_BUFFER_SIZE) {
		p->image_buffer.data[p->image_buffer.length] = byte;
		p->image_buffer.length++;
	} else {
		p->status = set_bit(p->status, STATUS_DATA_FULL);
	}
}

void parse_print_args(struct printer *p, uint8_t byte)
{
	switch (p->packet.data_byte) {
		case 0:
			/* TODO */
			break;
		case 1:
			p->margin.top_width = byte >> 4u;
			p->margin.bottom_width = byte & 0x0Fu;
			break;
		case 2:
			p->palette = byte;
			break;
		case 3:
			p->exposure = byte;
			break;
	}
}
