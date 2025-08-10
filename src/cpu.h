/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_CPU_H
#define GBCC_CPU_H

#include <stdbool.h>
#include <stdint.h>

/* TODO: Find a better way to do this */
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN
#endif


struct gbcc_core;

struct cpu {
	/* Registers */
	struct {
		union {
			struct {
				#ifdef LITTLE_ENDIAN
				uint8_t f;
				uint8_t a;
				#else
				uint8_t a;
				uint8_t f;
				#endif
			};
			uint16_t af;
		};
		union {
			struct {
				#ifdef LITTLE_ENDIAN
				uint8_t c;
				uint8_t b;
				#else
				uint8_t b;
				uint8_t c;
				#endif
			};
			uint16_t bc;
		};
		union {
			struct {
				#ifdef LITTLE_ENDIAN
				uint8_t e;
				uint8_t d;
				#else
				uint8_t d;
				uint8_t e;
				#endif
			};
			uint16_t de;
		};
		union {
			struct {
				#ifdef LITTLE_ENDIAN
				uint8_t l;
				uint8_t h;
				#else
				uint8_t h;
				uint8_t l;
				#endif
			};
			uint16_t hl;
		};
		uint16_t sp;
		uint16_t pc;
	} reg;

	/* Non-Register state data */
	uint8_t opcode;
	bool ime;
	bool stop;
	bool double_speed;
	bool tac_bit;
	uint16_t div_timer;
	uint8_t tima_reload;
	uint8_t clock;
	struct {
		uint8_t timer;
		bool target_state;
	} ime_timer;
	struct {
		bool set;
		bool no_interrupt;
		bool skip;
	} halt;
	struct {
		uint16_t source;
		uint16_t new_source;
		uint16_t timer;
		bool requested;
		bool running;
	} dma;
	struct {
		uint16_t addr;
		bool request;
		bool running;
	} interrupt;
	struct {
		uint16_t addr;
		uint8_t op1;
		uint8_t op2;
		uint8_t step;
		bool running;
		bool prefix_cb;
	} instruction;
};

uint8_t gbcc_fetch_instruction(struct gbcc_core *gbc);
void gbcc_emulate_cycle(struct gbcc_core *gbc);

#endif /* GBCC_CPU_H */
