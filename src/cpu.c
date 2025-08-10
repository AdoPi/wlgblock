/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#include "core.h"
#include "apu.h"
#include "bit_utils.h"
#include "cpu.h"
#include "debug.h"
#include "gbcc.h"
#include "hdma.h"
#include "memory.h"
#include "ops.h"
#include "ppu.h"
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

static inline void clock_div(struct gbcc_core *gbc);
static void check_interrupts(struct gbcc_core *gbc);
static inline void cpu_clock(struct gbcc_core *gbc);

/* TODO: Check order of all of these */
ANDROID_INLINE
void gbcc_emulate_cycle(struct gbcc_core *gbc)
{
	check_interrupts(gbc);
	gbcc_apu_clock(gbc);
	gbcc_ppu_clock(gbc);
	cpu_clock(gbc);
	clock_div(gbc);
	gbcc_link_cable_clock(gbc);
	if (gbc->cpu.double_speed) {
		cpu_clock(gbc);
		clock_div(gbc);
		gbcc_link_cable_clock(gbc);
	}
}

ANDROID_INLINE
void cpu_clock(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	/* CPU clocks every 4 cycles */
	cpu->clock++;
	cpu->clock &= 3u;
	if (cpu->clock != 0) {
		return;
	}
	if (!(cpu->instruction.running) && (cpu->halt.set || cpu->stop)) {
		return;
	}
	if (cpu->ime_timer.timer > 0) {
		cpu->ime_timer.timer--;
		if (cpu->ime_timer.timer == 1) {
			cpu->ime = cpu->ime_timer.target_state;
		}
	}
	if (cpu->dma.timer > 0) {
		cpu->dma.running = true;
		gbc->memory.oam[low_byte(cpu->dma.source)] = gbcc_memory_read(gbc, cpu->dma.source);
		cpu->dma.timer--;
		cpu->dma.source++;
	} else {
		cpu->dma.running = false;
	}
	if (cpu->dma.requested) {
		cpu->dma.requested = false;
		cpu->dma.timer = DMA_TIMER;
		cpu->dma.source = cpu->dma.new_source;
	}
	if (gbc->hdma.to_copy > 0) {
		gbcc_hdma_copy_chunk(gbc);
		return;
	}
	if (!cpu->instruction.running) {
		if (cpu->interrupt.running || (cpu->ime && cpu->interrupt.request)) {
			INTERRUPT(gbc);
			return;
		}
		//printf("%d::%04X\n", gbc->cart.mbc.romx_bank, cpu->reg.pc);
		cpu->opcode = gbcc_fetch_instruction(gbc);
		//gbcc_print_registers(gbc);
		//gbcc_print_op(gbc);
		cpu->instruction.running = true;
	}
	if (cpu->instruction.prefix_cb) {
		gbcc_ops[0xCB](gbc);
	} else {
		gbcc_ops[cpu->opcode](gbc);
	}
}

__attribute__((always_inline))
void clock_div(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	cpu->div_timer++;
	uint8_t tac = gbcc_memory_read_force(gbc, TAC);
	uint16_t mask = 0;
	switch (tac & 0x03u) {
		/* 
		 * TIMA register detects the falling edge of a bit in
		 * the internal DIV timer, which is selected by TAC.
		 */
		case 0:
			mask = bit16(9);
			break;
		case 1:
			mask = bit16(3);
			break;
		case 2:
			mask = bit16(5);
			break;
		case 3:
			mask = bit16(7);
			break;
	}
	/* If TAC is disabled, this will always see 0 */
	mask *= check_bit(tac, 2);
	bool old_bit = cpu->tac_bit;
	cpu->tac_bit = cpu->div_timer & mask;
	if (old_bit && !cpu->tac_bit) {
		/* 
		 * The selected bit was previously high, and is now low, so
		 * the tima increment logic triggers.
		 */
		uint8_t tima = gbcc_memory_read(gbc, TIMA);
		tima++;
		if (tima == 0) {
			/* 
			 * TIMA overflow
			 * Rather than being reloaded immediately, TIMA takes
			 * 4 cycles to be reloaded, and another 4 to write, 
			 * so we just queue it here. This also affects the
			 * interrupt.
			 */
			cpu->tima_reload = 8;
		}
		gbcc_memory_write(gbc, TIMA, tima);
	}
	if (cpu->tima_reload > 0) {
		cpu->tima_reload--;
		if (cpu->tima_reload == 4) {
			/*
			 * Some more weird behaviour here: if TIMA has been
			 * written to while this copy & interrupt are waiting,
			 * they get cancelled, and everything proceeds as
			 * normal.
			 */
			if (gbcc_memory_read(gbc, TIMA) != 0) {
				cpu->tima_reload = 0;
			} else {
				gbcc_memory_copy(gbc, TMA, TIMA);
				gbcc_memory_set_bit(gbc, IF, 2);
			}
		}
		else if (cpu->tima_reload == 0) {
			gbcc_memory_copy(gbc, TMA, TIMA);
		}
	}
	if (!gbc->apu.disabled) {
		/* APU also updates based on falling edge of DIV timer bit */
		if (gbc->cpu.double_speed) {
			mask = bit16(13);
		} else {
			mask = bit16(12);
		}
		old_bit = gbc->apu.div_bit;
		gbc->apu.div_bit = cpu->div_timer & mask;
		if (old_bit && !gbc->apu.div_bit) {
			gbcc_apu_sequencer_clock(gbc);
		}
	}
}

void check_interrupts(struct gbcc_core *gbc)
{
	if (gbc->keys.interrupt) {
		gbcc_memory_set_bit(gbc, IF, 4);
		gbc->keys.interrupt = false;
	}

	struct cpu *cpu = &gbc->cpu;
	uint8_t iereg = gbcc_memory_read(gbc, IE);
	uint8_t ifreg = gbcc_memory_read(gbc, IF);
	uint8_t interrupt = (uint8_t)(iereg & ifreg) & 0x1Fu;
	if (interrupt) {
		cpu->halt.set = false;
		gbc->cpu.stop = false;
		if (cpu->ime) {
			cpu->interrupt.request = true;
		}
	} else {
		cpu->interrupt.request = false;
	}
}

uint8_t gbcc_fetch_instruction(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->halt.skip) {
		/* HALT bug; CPU fails to increment pc */
		cpu->halt.skip = false;
		return gbcc_memory_read(gbc, cpu->reg.pc);
	} 
	return gbcc_memory_read(gbc, cpu->reg.pc++);
}
