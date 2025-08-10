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
#include "bit_utils.h"
#include "cheats.h"
#include "cpu.h"
#include "debug.h"
#include "memory.h"
#include "ops.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define YIELD {cpu->instruction.step++; return;}

static uint8_t READ_OPERAND_MOD(struct gbcc_core *gbc);
static void WRITE_OPERAND_MOD(struct gbcc_core *gbc, uint8_t val);
static void WRITE_OPERAND_DIV(struct gbcc_core *gbc, uint8_t offset, uint8_t val);
static bool mod_is_hl(struct cpu *cpu);
static bool div_is_hl(struct cpu *cpu, uint8_t offset);
static void done(struct cpu *cpu);

static uint8_t get_flag(struct cpu *cpu, uint8_t flag)
{
	return !!(cpu->reg.f & flag);
}

static void set_flag(struct cpu *cpu, uint8_t flag)
{
	cpu->reg.f |= flag;
}

static void clear_flag(struct cpu *cpu, uint8_t flag)
{
	cpu->reg.f &= (uint8_t)~flag;
}

static void toggle_flag(struct cpu *cpu, uint8_t flag)
{
	cpu->reg.f ^= flag;
}

static void cond_flag(struct cpu *cpu, uint8_t flag, bool cond) {
	if (cond) {
		set_flag(cpu, flag);
	} else {
		clear_flag(cpu, flag);
	}
}

/* Main opcode jump table */
void (*const gbcc_ops[0x100])(struct gbcc_core *gbc) = {
/* 0x00 */	NOP,		LD_d16,		LD_A,		INC_DEC_16_BIT,
/* 0x04 */	INC_DEC_REG,	INC_DEC_REG,	LD_d8,		SHIFT_A,
/* 0x08 */	STORE_SP,	ADD_HL,		LD_A,		INC_DEC_16_BIT,
/* 0x0C */	INC_DEC_REG,	INC_DEC_REG,	LD_d8,		SHIFT_A,
/* 0x10 */	STOP,		LD_d16,		LD_A,		INC_DEC_16_BIT,
/* 0x14 */	INC_DEC_REG,	INC_DEC_REG,	LD_d8,		SHIFT_A,
/* 0x18 */	JR,		ADD_HL,		LD_A,		INC_DEC_16_BIT,
/* 0x1C */	INC_DEC_REG,	INC_DEC_REG,	LD_d8,		SHIFT_A,
/* 0x20 */	JR_COND,	LD_d16,		LD_A,		INC_DEC_16_BIT,
/* 0x24 */	INC_DEC_REG,	INC_DEC_REG,	LD_d8,		DAA,
/* 0x28 */	JR_COND,	ADD_HL,		LD_A,		INC_DEC_16_BIT,
/* 0x2C */	INC_DEC_REG,	INC_DEC_REG,	LD_d8,		CPL,
/* 0x30 */	JR_COND,	LD_d16,		LD_A,		INC_DEC_16_BIT,
/* 0x34 */	INC_DEC_HL,	INC_DEC_HL,	LD_d8,		SCF,
/* 0x38 */	JR_COND,	ADD_HL,		LD_A,		INC_DEC_16_BIT,
/* 0x3C */	INC_DEC_REG,	INC_DEC_REG,	LD_d8,		CCF,
/* 0x40 */	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,
/* 0x44 */	LD_REG_REG,	LD_REG_REG,	LD_REG_HL,	LD_REG_REG,
/* 0x48 */	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,
/* 0x4C */	LD_REG_REG,	LD_REG_REG,	LD_REG_HL,	LD_REG_REG,
/* 0x50 */	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,
/* 0x54 */	LD_REG_REG,	LD_REG_REG,	LD_REG_HL,	LD_REG_REG,
/* 0x58 */	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,
/* 0x5C */	LD_REG_REG,	LD_REG_REG,	LD_REG_HL,	LD_REG_REG,
/* 0x60 */	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,
/* 0x64 */	LD_REG_REG,	LD_REG_REG,	LD_REG_HL,	LD_REG_REG,
/* 0x68 */	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,
/* 0x6C */	LD_REG_REG,	LD_REG_REG,	LD_REG_HL,	LD_REG_REG,
/* 0x70 */	LD_REG_HL,	LD_REG_HL,	LD_REG_HL,	LD_REG_HL,
/* 0x74 */	LD_REG_HL,	LD_REG_HL,	HALT,		LD_REG_HL,
/* 0x78 */	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,	LD_REG_REG,
/* 0x7C */	LD_REG_REG,	LD_REG_REG,	LD_REG_HL,	LD_REG_REG,
/* 0x80 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0x84 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0x88 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0x8C */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0x90 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0x94 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0x98 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0x9C */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0xA0 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0xA4 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0xA8 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0xAC */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0xB0 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0xB4 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0xB8 */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0xBC */	ALU_OP,		ALU_OP,		ALU_OP,		ALU_OP,
/* 0xC0 */	RET_COND,	POP,		JP_COND,	JP,
/* 0xC4 */	CALL_COND,	PUSH,		ALU_OP,		RST,
/* 0xC8 */	RET_COND,	RET,		JP_COND,	PREFIX_CB,
/* 0xCC */	CALL_COND,	CALL,		ALU_OP,		RST,
/* 0xD0 */	RET_COND,	POP,		JP_COND,	INVALID,
/* 0xD4 */	CALL_COND,	PUSH,		ALU_OP,		RST,
/* 0xD8 */	RET_COND,	RETI,		JP_COND,	INVALID,
/* 0xDC */	CALL_COND,	INVALID,	ALU_OP,		RST,
/* 0xE0 */	LDH_a8,		POP,		LDH_C,		INVALID,
/* 0xE4 */	INVALID,	PUSH,		ALU_OP,		RST,
/* 0xE8 */	ADD_SP,		JP_HL,		LD_a16,		INVALID,
/* 0xEC */	INVALID,	INVALID,	ALU_OP,		RST,
/* 0xF0 */	LDH_a8,		POP,		LDH_C,		DI,
/* 0xF4 */	INVALID,	PUSH,		ALU_OP,		RST,
/* 0xF8 */	LD_HL_SP,	LD_SP_HL,	LD_a16,		EI,
/* 0xFC */	INVALID,	INVALID,	ALU_OP,		RST
};


void INTERRUPT(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->halt.no_interrupt) {
		if (cpu->instruction.step < 20) {
			YIELD
		}
		cpu->halt.no_interrupt = false;
		cpu->halt.set = false;
		done(cpu);
		return;
	}
	uint8_t iereg = gbcc_memory_read(gbc, IE);
	uint8_t ifreg = gbcc_memory_read(gbc, IF);
	uint8_t interrupt = (uint8_t)(iereg & ifreg) & 0x1Fu;
	if (!interrupt && cpu->instruction.step < 4) {
		if (cpu->instruction.step < 3) {
			cpu->interrupt.request = false;
			cpu->interrupt.running = false;
			done(cpu);
			return;
		}
		/* 
		 * Interrupt was cancelled by writing over a flag when pushing
		 * the high byte of pc.
		 */
		cpu->reg.pc = 0x0000u;
		cpu->ime = false;
		cpu->interrupt.request = false;
		cpu->interrupt.running = false;
		done(cpu);
		return;
	}
	switch (cpu->instruction.step) {
		case 0:
			cpu->ime = false;
			cpu->interrupt.running = true;
			YIELD
		case 1:
			YIELD
		case 2:
			gbcc_memory_write(gbc, --cpu->reg.sp, high_byte(cpu->reg.pc));
			YIELD
		case 3:
			gbcc_memory_write(gbc, --cpu->reg.sp, low_byte(cpu->reg.pc));
			/* 
			 * Interrupt address is calculated here according to
			 * mooneye's tests. Interrupt priority is in memory
			 * address order; lower adresses have higher priority.
			 */
			if (check_bit(interrupt, 0)) {
				cpu->interrupt.addr = INT_VBLANK;
			} else if (check_bit(interrupt, 1)) {
				cpu->interrupt.addr = INT_LCDSTAT;
			} else if (check_bit(interrupt, 2)) {
				cpu->interrupt.addr = INT_TIMER;
			} else if (check_bit(interrupt, 3)) {
				cpu->interrupt.addr = INT_SERIAL;
			} else if (check_bit(interrupt, 4)) {
				cpu->interrupt.addr = INT_JOYPAD;
			}
			YIELD
		case 4:
			if (cpu->halt.set) {
				YIELD
			}
	}
	switch (cpu->interrupt.addr) {
		case INT_VBLANK:
			gbcc_memory_clear_bit(gbc, IF, 0);
			/*
			 * The GameShark apparently hooks the vblank interrupt
			 * & applies any codes. This is meant to take some cpu
			 * time, but here I just complete it instantaneously.
			 */
			if (gbc->cheats.enabled) {
				gbcc_cheats_gameshark_update(gbc);
			}
			break;
		case INT_LCDSTAT:
			gbcc_memory_clear_bit(gbc, IF, 1);
			break;
		case INT_TIMER:
			gbcc_memory_clear_bit(gbc, IF, 2);
			break;
		case INT_SERIAL:
			gbcc_memory_clear_bit(gbc, IF, 3);
			break;
		case INT_JOYPAD:
			gbcc_memory_clear_bit(gbc, IF, 4);
			break;
	}
	cpu->reg.pc = cpu->interrupt.addr;
	cpu->ime = false;
	cpu->interrupt.running = false;
	done(cpu);
}

/* Miscellaneous */

void INVALID(struct gbcc_core *gbc)
{
	gbc->error = true;
	gbc->error_msg = "Invalid opcode encountered.";
}

void NOP(struct gbcc_core *gbc)
{
	(void) gbc;
	done(&gbc->cpu);
}

void STOP(struct gbcc_core *gbc)
{
	uint8_t key1 = gbcc_memory_read_force(gbc, KEY1);
	if (gbc->mode == GBC && check_bit(key1, 0)) {
		gbc->cpu.double_speed = !gbc->cpu.double_speed;
		key1 = gbc->cpu.double_speed * bit(7);
		gbcc_memory_write_force(gbc, KEY1, key1);
	} else {
		gbc->cpu.stop = true;
	}
	gbcc_fetch_instruction(gbc); /* Discard next byte */
	done(&gbc->cpu);
}

void HALT(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	uint8_t iereg = gbcc_memory_read(gbc, IE);
	uint8_t ifreg = gbcc_memory_read(gbc, IF);
	bool interrupt = (uint8_t)(iereg & ifreg) & 0x1Fu;
	if (cpu->ime) {
		/* HALT proceeds normally */
		cpu->halt.set = true;
		cpu->halt.no_interrupt = false;
		cpu->halt.skip = false;
	} else if (!interrupt) {
		/* HALT proceeds, but on resumption the interrupt isn't executed */
		cpu->halt.set = true;
		cpu->halt.no_interrupt = true;
		cpu->halt.skip = false;
	} else {
		/* HALT bug; next instruction doesn't increase PC */
		cpu->halt.set = false;
		cpu->halt.no_interrupt = false;
		cpu->halt.skip = true;
	}
	done(cpu);
}

void DAA(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	uint8_t *op = &(cpu->reg.a);
	if (get_flag(cpu, NF)) {
		if (get_flag(cpu, CF)) {
			*op -= 0x60u;
		}
		if (get_flag(cpu, HF)) {
			*op -= 0x06u;
		}
	} else {
		if (get_flag(cpu, CF) || *op > 0x99u) {
			*op += 0x60u;
			set_flag(cpu, CF);
		}
		if (get_flag(cpu, HF) || (*op & 0x0Fu) > 0x09u) {
			*op += 0x06u;
		}
	}
	cond_flag(cpu, ZF, (*op == 0));
	clear_flag(cpu, HF);
	done(cpu);
}

void CPL(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	cpu->reg.a = ~cpu->reg.a;
	set_flag(cpu, NF);
	set_flag(cpu, HF);
	done(cpu);
}

void SCF(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	set_flag(cpu, CF);
	clear_flag(cpu, NF);
	clear_flag(cpu, HF);
	done(cpu);
}

void CCF(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	toggle_flag(cpu, CF);
	clear_flag(cpu, NF);
	clear_flag(cpu, HF);
	done(cpu);
}

/* 
 * EI and DI take 1 M-cycle to take effect, so we don't immediately set the
 * IME here. However, EI immediately followed by DI will not result in an
 * interrupt, so we need to know when executing DI if the last instruction was
 * EI, hence setting the timer to 2, and making the IME change on step 1
 * normally.
 */
void EI(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	cpu->ime_timer.target_state = true;
	cpu->ime_timer.timer = 2;
	done(cpu);
}

void DI(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->ime_timer.timer > 0 && cpu->ime_timer.target_state == true) {
		cpu->ime = false;
		done(cpu);
		return;
	}
	cpu->ime_timer.target_state = false;
	cpu->ime_timer.timer = 2;
	done(cpu);
}

/* Loads */

void LD_REG_REG(struct gbcc_core *gbc)
{
	if (gbc->cpu.opcode == 0x52) {
		/* Use ld d,d as a debug statement */
		gbcc_print_registers(gbc, true);
	}
	WRITE_OPERAND_DIV(gbc, 0x40u, READ_OPERAND_MOD(gbc));
	done(&gbc->cpu);
}

void LD_REG_HL(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = READ_OPERAND_MOD(gbc);
			WRITE_OPERAND_DIV(gbc, 0x40u, cpu->instruction.op1);
	}
	done(cpu);
}

void LD_d8(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			if (div_is_hl(cpu, 0x00u)) {
				YIELD
			}
			/* Fall through */
		case 2:
			WRITE_OPERAND_DIV(gbc, 0x00u, cpu->instruction.op1);
	}
	done(cpu);
}

void LD_d16(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
		case 2:
			cpu->instruction.op2 = gbcc_fetch_instruction(gbc);
	}
	uint16_t val = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
	switch (cpu->opcode / 0x10u) {
		case 0:
			cpu->reg.bc = val;
			break;
		case 1:
			cpu->reg.de = val;
			break;
		case 2:
			cpu->reg.hl = val;
			break;
		case 3:
			cpu->reg.sp = val;
			break;
		default:
			gbcc_log_error("Impossible case in LD_d16\n");
			return;
	}
	done(cpu);
}

void LD_A(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->instruction.step == 0) {
		switch (cpu->opcode / 0x10u) {
			case 0:
				cpu->instruction.addr = cpu->reg.bc;
				break;
			case 1:
				cpu->instruction.addr = cpu->reg.de;
				break;
			case 2:	/* LDI */
				cpu->instruction.addr = cpu->reg.hl++;
				break;
			case 3:	/* LDD */
				cpu->instruction.addr = cpu->reg.hl--;
				break;
			default:
				gbcc_log_error("Impossible case in LD_A\n");
				return;
		}
		YIELD
	}
	switch((cpu->opcode % 0x10u) / 0x08u) {
		case 0:
			gbcc_memory_write(gbc, cpu->instruction.addr, cpu->reg.a);
			break;
		case 1:
			cpu->reg.a = gbcc_memory_read(gbc, cpu->instruction.addr);
			break;
		default:
			gbcc_log_error("Impossible case in LD_A\n");
			return;
	}
	done(cpu);
}

void LD_a16(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
		case 2:
			cpu->instruction.op2 = gbcc_fetch_instruction(gbc);
			YIELD
	}
	uint16_t addr = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
	switch ((cpu->opcode - 0xE0u) / 0x10u) {
		case 0:
			gbcc_memory_write(gbc, addr, cpu->reg.a);
			break;
		case 1:
			cpu->reg.a = gbcc_memory_read(gbc, addr);
			break;
		default:
			gbcc_log_error("Impossible case in LD_a16\n");
			break;
	}
	done(cpu);
}

void LDH_a8(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.addr = 0xFF00u + gbcc_fetch_instruction(gbc);
			YIELD
	}
	switch ((cpu->opcode - 0xE0u) / 0x10u) {
		case 0:
			gbcc_memory_write(gbc, cpu->instruction.addr, cpu->reg.a);
			break;
		case 1:
			cpu->reg.a = gbcc_memory_read(gbc, cpu->instruction.addr);
			break;
		default:
			gbcc_log_error("Impossible case in LD_OFFSET\n");
			return;
	}
	done(cpu);
}

void LDH_C(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->instruction.step == 0) {
		cpu->instruction.addr = 0xFF00u + cpu->reg.c;
		YIELD
	}

	switch ((cpu->opcode - 0xE0u) / 0x10u) {
		case 0:
			gbcc_memory_write(gbc, cpu->instruction.addr, cpu->reg.a);
			break;
		case 1:
			cpu->reg.a = gbcc_memory_read(gbc, cpu->instruction.addr);
			break;
		default:
			gbcc_log_error("Impossible case in LD_OFFSET\n");
			return;
	}
	done(cpu);
}

void STORE_SP(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
		case 2:
			cpu->instruction.op2 = gbcc_fetch_instruction(gbc);
			YIELD
		case 3:
			cpu->instruction.addr = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
			gbcc_memory_write(gbc, cpu->instruction.addr, low_byte(cpu->reg.sp));
			YIELD
		case 4:
			gbcc_memory_write(gbc, cpu->instruction.addr+1, high_byte(cpu->reg.sp));
	}
	done(cpu);
}

void LD_HL_SP(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
	}
	uint16_t tmp = cpu->reg.sp;
	uint16_t tmp2 = (uint16_t)(cpu->reg.sp + (int8_t)cpu->instruction.op1);
	cpu->reg.hl = tmp2;
	cond_flag(cpu, HF, (tmp2 & 0x0Fu) < (tmp & 0x0Fu));
	cond_flag(cpu, CF, (tmp2 & 0xFFu) < (tmp & 0xFFu));
	clear_flag(cpu, ZF);
	clear_flag(cpu, NF);
	done(cpu);
}

void LD_SP_HL(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->instruction.step == 0) {
		YIELD
	}
	cpu->reg.sp = cpu->reg.hl;
	done(cpu);
}

void POP(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_memory_read(gbc, cpu->reg.sp++);
			YIELD
		case 2:
			cpu->instruction.op2 = gbcc_memory_read(gbc, cpu->reg.sp++);
	}
	uint16_t tmp = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
	switch ((cpu->opcode % 0x40u) / 0x10u) {
		case 0:
			cpu->reg.bc = tmp;
			break;
		case 1:
			cpu->reg.de = tmp;
			break;
		case 2:
			cpu->reg.hl = tmp;
			break;
		case 3:
			cpu->reg.af = tmp;
			/* Lower 4 bits of AF are always 0 */
			cpu->reg.af &= 0xFFF0u;
			break;
		default:
			gbcc_log_error("Impossible case in PUSH_POP\n");
			return;
	}
	done(cpu);
}

void PUSH(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			switch ((cpu->opcode % 0x40u) / 0x10u) {
				case 0:
					cpu->instruction.op1 = cpu->reg.b;
					cpu->instruction.op2 = cpu->reg.c;
					break;
				case 1:
					cpu->instruction.op1 = cpu->reg.d;
					cpu->instruction.op2 = cpu->reg.e;
					break;
				case 2:
					cpu->instruction.op1 = cpu->reg.h;
					cpu->instruction.op2 = cpu->reg.l;
					break;
				case 3:
					cpu->instruction.op1 = cpu->reg.a;
					cpu->instruction.op2 = cpu->reg.f;
					break;
				default:
					gbcc_log_error("Impossible case in PUSH_POP\n");
					return;
			}
			YIELD
		case 1:
			YIELD
		case 2:
			gbcc_memory_write(gbc, --(cpu->reg.sp), cpu->instruction.op1);
			YIELD
		case 3:
			gbcc_memory_write(gbc, --(cpu->reg.sp), cpu->instruction.op2);
	}
	done(cpu);
}

/* ALU */

void ALU_OP(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	uint8_t *op1 = &(cpu->reg.a);
	uint8_t op2;
	uint8_t tmp;
	uint8_t offset;
	
	if (cpu->instruction.step == 0 && mod_is_hl(cpu)) {
		YIELD
	}

	if (cpu->opcode < 0xC0u) {
		op2 = READ_OPERAND_MOD(gbc);
		offset = 0x80u;
	} else {
		op2 = gbcc_fetch_instruction(gbc);
		offset = 0xC0u;
	}

	switch ((cpu->opcode - offset) / 0x08u) {
		case 0: /* ADD */
			cond_flag(cpu, HF, (((*op1 & 0x0Fu) + (op2 & 0x0Fu)) & 0x10u) == 0x10u);
			tmp = *op1;
			*op1 += op2;
			cond_flag(cpu, ZF, (*op1 == 0));
			clear_flag(cpu, NF);
			cond_flag(cpu, CF, *op1 < tmp);
			break;
		case 1: /* ADC */
			cond_flag(cpu, HF, (((*op1 & 0x0Fu) + (op2 & 0x0Fu) + get_flag(cpu, CF)) & 0x10u) == 0x10u);
			*op1 += get_flag(cpu, CF);
			if (*op1 == 0 && get_flag(cpu, CF)) {
				set_flag(cpu, CF);
			} else {
				clear_flag(cpu, CF);
			}
			tmp = *op1;
			*op1 += op2;
			if (*op1 < tmp) {
				set_flag(cpu, CF);
			}
			cond_flag(cpu, ZF, (*op1 == 0));
			clear_flag(cpu, NF);
			break;
		case 2: /* SUB */
			cond_flag(cpu, HF, ((*op1 & 0x0Fu) - (op2 & 0x0Fu)) > 0x0Fu);
			tmp = *op1;
			*op1 -= op2;
			cond_flag(cpu, ZF, (*op1 == 0));
			set_flag(cpu, NF);
			cond_flag(cpu, CF, *op1 > tmp);
			break;
		case 3: /* SBC */
			cond_flag(cpu, HF, ((*op1 & 0x0Fu) - (op2 & 0x0Fu) - get_flag(cpu, CF)) > 0x0Fu);
			*op1 -= get_flag(cpu, CF);
			if (*op1 == 0xFF && get_flag(cpu, CF)) {
				set_flag(cpu, CF);
			} else {
				clear_flag(cpu, CF);
			}
			tmp = *op1;
			*op1 -= op2;
			if (*op1 > tmp) {
				set_flag(cpu, CF);
			}
			cond_flag(cpu, ZF, (*op1 == 0));
			set_flag(cpu, NF);
			break;
		case 4: /* AND */
			*op1 &= op2;
			cond_flag(cpu, ZF, (*op1 == 0));
			clear_flag(cpu, NF);
			set_flag(cpu, HF);
			clear_flag(cpu, CF);
			break;
		case 5: /* XOR */
			*op1 ^= op2;
			cond_flag(cpu, ZF, (*op1 == 0));
			clear_flag(cpu, NF);
			clear_flag(cpu, HF);
			clear_flag(cpu, CF);
			break;
		case 6: /* OR */
			*op1 |= op2;
			cond_flag(cpu, ZF, (*op1 == 0));
			clear_flag(cpu, NF);
			clear_flag(cpu, HF);
			clear_flag(cpu, CF);
			break;
		case 7: /* CP */
			cond_flag(cpu, HF, ((*op1 & 0x0Fu) - (op2 & 0x0Fu)) > 0x0Fu);
			tmp = *op1 - op2;
			cond_flag(cpu, ZF, (tmp == 0));
			set_flag(cpu, NF);
			cond_flag(cpu, CF, tmp > *op1);
			break;
		default:
			gbcc_log_error("Impossible case in ALU_OP\n");
			return;
	}
	done(cpu);
}

void INC_DEC_REG(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	uint8_t op;

	switch (cpu->opcode / 0x08u) {
		case 0:
			op = cpu->reg.b;
			break;
		case 1:
			op = cpu->reg.c;
			break;
		case 2:
			op = cpu->reg.d;
			break;
		case 3:
			op = cpu->reg.e;
			break;
		case 4:
			op = cpu->reg.h;
			break;
		case 5:
			op = cpu->reg.l;
			break;
		case 6:
			op = gbcc_memory_read(gbc, cpu->reg.hl);
			break;
		case 7:
			op = cpu->reg.a;
			break;
		default:
			gbcc_log_error("Impossible case in INC_DEC_8_BIT\n");
			return;
	}

	switch ((cpu->opcode % 0x08u) / 0x05u) {
		case 0:	/* INC */
			clear_flag(cpu, NF);
			cond_flag(cpu, HF, (((op & 0x0Fu) + 1) & 0x10u) == 0x10u);
			WRITE_OPERAND_DIV(gbc, 0x00u, ++op);
			break;
		case 1:	/* DEC */
			set_flag(cpu, NF);
			cond_flag(cpu, HF, (((op & 0x0Fu) - 1) & 0x10u) == 0x10u);
			WRITE_OPERAND_DIV(gbc, 0x00u, --op);
			break;
		default:
			gbcc_log_error("Impossible case in INC_DEC_8_BIT\n");
			return;
	}
	cond_flag(cpu, ZF, (op == 0));
	done(cpu);
}

void INC_DEC_HL(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_memory_read(gbc, cpu->reg.hl);
			YIELD
	}
	uint8_t op = cpu->instruction.op1;
	switch ((cpu->opcode % 0x08u) / 0x05u) {
		case 0:	/* INC */
			clear_flag(cpu, NF);
			cond_flag(cpu, HF, (((op & 0x0Fu) + 1) & 0x10u) == 0x10u);
			WRITE_OPERAND_DIV(gbc, 0x00u, ++op);
			break;
		case 1:	/* DEC */
			set_flag(cpu, NF);
			cond_flag(cpu, HF, (((op & 0x0Fu) - 1) & 0x10u) == 0x10u);
			WRITE_OPERAND_DIV(gbc, 0x00u, --op);
			break;
		default:
			gbcc_log_error("Impossible case in INC_DEC_8_BIT\n");
			return;
	}
	cond_flag(cpu, ZF, (op == 0));
	done(cpu);
}

void INC_DEC_16_BIT(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->instruction.step == 0) {
		YIELD
	}
	uint16_t *op;
	switch (cpu->opcode / 0x10u) {
		case 0:
			op = &(cpu->reg.bc);
			break;
		case 1:
			op = &(cpu->reg.de);
			break;
		case 2:
			op = &(cpu->reg.hl);
			break;
		case 3:
			op = &(cpu->reg.sp);
			break;
		default:
			gbcc_log_error("Impossible case in INC_DEC_16_BIT\n");
			return;
	}
	switch ((cpu->opcode % 0x10u) / 0x08u) {
		case 0:	/* INC */
			*op += 1u;
			break;
		case 1:	/* DEC */
			*op -= 1u;
			break;
		default:
			gbcc_log_error("Impossible case in INC_DEC_16_BIT\n");
			return;
	}
	done(cpu);
}

void ADD_HL(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->instruction.step == 0) {
		YIELD
	}
	uint16_t op;
	uint16_t tmp;
	switch (cpu->opcode / 0x10u) {
		case 0:
			op = cpu->reg.bc;
			break;
		case 1:
			op = cpu->reg.de;
			break;
		case 2:
			op = cpu->reg.hl;
			break;
		case 3:
			op = cpu->reg.sp;
			break;
		default:
			gbcc_log_error("Impossible case in ADD_HL\n");
			return;
	}
	cond_flag(cpu, HF, (((cpu->reg.hl & 0x0FFFu) + (op & 0x0FFFu)) & 0x1000u) == 0x1000u);
	tmp = cpu->reg.hl;
	cpu->reg.hl += op;
	cond_flag(cpu, CF, (tmp > cpu->reg.hl));
	clear_flag(cpu, NF);
	done(cpu);
}

void ADD_SP(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
		case 2:
			YIELD
	}
	uint16_t tmp = cpu->reg.sp;
	cpu->reg.sp += (int8_t)cpu->instruction.op1;
	cond_flag(cpu, HF, (cpu->reg.sp & 0x0Fu) < (tmp & 0x0Fu));
	cond_flag(cpu, CF, (cpu->reg.sp & 0xFFu) < (tmp & 0xFFu));
	clear_flag(cpu, ZF);
	clear_flag(cpu, NF);
	done(cpu);
}

void SHIFT_A(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	uint8_t *op = &(cpu->reg.a);
	uint8_t tmp;

	switch (cpu->opcode / 0x08u) {
		case 0:	/* RLC */
			cond_flag(cpu, CF, (*op & 0x80u) >> 7u);
			*op = (uint8_t)(*op << 1u) | (uint8_t)(*op >> 7u);
			break;
		case 1:	/* RRC */
			cond_flag(cpu, CF, *op & 1u);
			*op = (uint8_t)(*op >> 1u) | (uint8_t)(*op << 7u);
			break;
		case 2:	/* RL */
			tmp = get_flag(cpu, CF);
			cond_flag(cpu, CF, (*op & 0x80u) >> 7u);
			*op = (uint8_t)(*op << 1u) | tmp;
			break;
		case 3:	/* RR */
			tmp = get_flag(cpu, CF);
			cond_flag(cpu, CF, *op & 1u);
			*op = (uint8_t)(*op >> 1u) | (uint8_t)(tmp << 7u);
			break;
		default:
			gbcc_log_error("Impossible case in SHIFT_A\n");
			return;
	}
	clear_flag(cpu, ZF);
	clear_flag(cpu, NF);
	clear_flag(cpu, HF);
	done(cpu);
}

/* Jumps */

void JP(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
		case 2:
			cpu->instruction.op2 = gbcc_fetch_instruction(gbc);
			YIELD
	}
	cpu->reg.pc = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
	done(cpu);
}

void JP_HL(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	cpu->reg.pc = cpu->reg.hl;
	done(cpu);
}

void JP_COND(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	bool jp = false;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
		case 2:
			cpu->instruction.op2 = gbcc_fetch_instruction(gbc);
			cpu->instruction.addr = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
			switch ((cpu->opcode - 0xC0u) / 0x08u) {
				case 0:	/* JP NZ */
					jp = !get_flag(cpu, ZF);
					break;
				case 1:	/* JP Z */
					jp = get_flag(cpu, ZF);
					break;
				case 2:	/* JP NC */
					jp = !get_flag(cpu, CF);
					break;
				case 3:	/* JP C */
					jp = get_flag(cpu, CF);
					break;
			}
			if (jp) {
				cpu->reg.pc = cpu->instruction.addr;
				YIELD
			} else {
				break;
			}
	}
	done(cpu);
}

void JR(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
		case 2:
			cpu->reg.pc += (int8_t)cpu->instruction.op1;
	}
	done(cpu);
}

void JR_COND(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			break;
		case 2: /* Jump was taken */
			done(cpu);
			return;
	}
	switch ((cpu->opcode - 0x20u) / 0x08u) {
		case 0:	/* JR NZ */
			if (!get_flag(cpu, ZF)) {
				cpu->reg.pc += (int8_t)cpu->instruction.op1;
				YIELD
			}
			break;
		case 1:	/* JR Z */
			if (get_flag(cpu, ZF)) {
				cpu->reg.pc += (int8_t)cpu->instruction.op1;
				YIELD
			}
			break;
		case 2:	/* JR NC */
			if (!get_flag(cpu, CF)) {
				cpu->reg.pc += (int8_t)cpu->instruction.op1;
				YIELD
			}
			break;
		case 3:	/* JR C */
			if (get_flag(cpu, CF)) {
				cpu->reg.pc += (int8_t)cpu->instruction.op1;
				YIELD
			}
			break;
		default:
			gbcc_log_error("Impossible case in JR_COND\n");
			return;
	}
	done(cpu);
}

/* Calls */

void CALL(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
		case 2:
			cpu->instruction.op2 = gbcc_fetch_instruction(gbc);
			YIELD
		case 3:
			cpu->instruction.addr = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
			YIELD
		case 4:
			gbcc_memory_write(gbc, --(cpu->reg.sp), high_byte(cpu->reg.pc));
			YIELD
		case 5:
			gbcc_memory_write(gbc, --(cpu->reg.sp), low_byte(cpu->reg.pc));
	}
	cpu->reg.pc = cpu->instruction.addr;
	done(cpu);
}

void CALL_COND(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_fetch_instruction(gbc);
			YIELD
		case 2:
			cpu->instruction.op2 = gbcc_fetch_instruction(gbc);
			switch ((cpu->opcode - 0xC0u) / 0x08u) {
				case 0:	/* CALL NZ */
					if (!get_flag(cpu, ZF)) {
						YIELD
					}
					break;
				case 1:	/* CALL Z */
					if (get_flag(cpu, ZF)) {
						YIELD
					}
					break;
				case 2:	/* CALL NC */
					if (!get_flag(cpu, CF)) {
						YIELD
					}
					break;
				case 3:	/* CALL C */
					if (get_flag(cpu, CF)) {
						YIELD
					}
					break;
			}
			break;
		case 3:
			cpu->instruction.addr = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
			YIELD
		case 4:
			gbcc_memory_write(gbc, --(cpu->reg.sp), high_byte(cpu->reg.pc));
			YIELD
		case 5:
			gbcc_memory_write(gbc, --(cpu->reg.sp), low_byte(cpu->reg.pc));
			cpu->reg.pc = cpu->instruction.addr;
	}
	done(cpu);
}

void RET(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->instruction.op1 = gbcc_memory_read(gbc, cpu->reg.sp++);
			YIELD
		case 2:
			cpu->instruction.op2 = gbcc_memory_read(gbc, cpu->reg.sp++);
			YIELD
		case 3:
			cpu->reg.pc = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
	}
	done(cpu);
}

void RETI(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	RET(gbc);
	if (!cpu->instruction.running) {
		/* RET has finished */
		cpu->ime = true;
	}
}

void RET_COND(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	bool ret = false;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			switch ((cpu->opcode - 0xC0u) / 0x08u) {
				case 0:	/* RET NZ */
					ret = !get_flag(cpu, ZF);
					break;
				case 1:	/* RET Z */
					ret = get_flag(cpu, ZF);
					break;
				case 2:	/* RET NC */
					ret = !get_flag(cpu, CF);
					break;
				case 3:	/* RET C */
					ret = get_flag(cpu, CF);
					break;
			}
			if (ret) {
				YIELD
			} else {
				break;
			}
		case 2:
			cpu->instruction.op1 = gbcc_memory_read(gbc, cpu->reg.sp++);
			YIELD
		case 3:
			cpu->instruction.op2 = gbcc_memory_read(gbc, cpu->reg.sp++);
			YIELD
		case 4:
			cpu->reg.pc = cat_bytes(cpu->instruction.op1, cpu->instruction.op2);
	}
	done(cpu);
}

void RST(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			YIELD
		case 2:
			gbcc_memory_write(gbc, --cpu->reg.sp, high_byte(cpu->reg.pc));
			YIELD
		case 3:
			gbcc_memory_write(gbc, --cpu->reg.sp, low_byte(cpu->reg.pc));
	}
	cpu->reg.pc = cpu->opcode - 0xC7u;
	done(cpu);
}

/* CB-prefix */

void PREFIX_CB(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	cpu->instruction.prefix_cb = true;
	switch (cpu->instruction.step) {
		case 0:
			YIELD
		case 1:
			cpu->opcode = gbcc_fetch_instruction(gbc);
			break;
	}
	switch (cpu->opcode / 0x40u) {
		case 0:
			CB_SHIFT_OP(gbc);
			break;
		case 1:
			CB_BIT(gbc);
			break;
		case 2:
			CB_RES(gbc);
			break;
		case 3:
			CB_SET(gbc);
			break;
	}
}

void CB_SHIFT_OP(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (mod_is_hl(cpu)) {
		switch (cpu->instruction.step) {
			case 1:
				YIELD
			case 2:
				cpu->instruction.op1 = READ_OPERAND_MOD(gbc);
				YIELD
		}
	} else {
		cpu->instruction.op1 = READ_OPERAND_MOD(gbc);
	}
	uint8_t op = cpu->instruction.op1;
	uint8_t tmp;

	switch (cpu->opcode / 0x08u) {
		case 0:	/* RLC */
			cond_flag(cpu, CF, check_bit(op, 7));
			op = (uint8_t)(op << 1u) | (uint8_t)(op >> 7u);
			break;
		case 1:	/* RRC */
			cond_flag(cpu, CF, check_bit(op, 0));
			op = (uint8_t)(op >> 1u) | (uint8_t)(op << 7u);
			break;
		case 2:	/* RL */
			tmp = get_flag(cpu, CF);
			cond_flag(cpu, CF, check_bit(op, 7));
			op = (uint8_t)(op << 1u) | tmp;
			break;
		case 3:	/* RR */
			tmp = get_flag(cpu, CF);
			cond_flag(cpu, CF, check_bit(op, 0));
			op = (uint8_t)(op >> 1u) | (uint8_t)(tmp << 7u);
			break;
		case 4:	/* SLA */
			cond_flag(cpu, CF, check_bit(op, 7));
			op <<= 1u;
			break;
		case 5:	/* SRA */
			cond_flag(cpu, CF, check_bit(op, 0));
			op = (uint8_t)(op >> 1u) | (uint8_t)(op & 0x80u);
			break;
		case 6:	/* SWAP */
			clear_flag(cpu, CF);
			op = (uint8_t)((op & 0x0Fu) << 4u) | (uint8_t)((op & 0xF0u) >> 4u);
			break;
		case 7:	/* SRL */
			cond_flag(cpu, CF, check_bit(op, 0));
			op >>= 1u;
			break;
		default:
			gbcc_log_error("Impossible case in CB_SHIFT_OP\n");
			return;
	}
	cond_flag(cpu, ZF, (op == 0));
	clear_flag(cpu, NF);
	clear_flag(cpu, HF);

	WRITE_OPERAND_MOD(gbc, op);
	done(cpu);
}

void CB_BIT(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (cpu->instruction.step == 1 && mod_is_hl(cpu)) {
		YIELD
	}
	uint8_t op = READ_OPERAND_MOD(gbc);
	uint8_t b = (cpu->opcode - 0x40u) / 0x08u;

	cond_flag(cpu, ZF, !check_bit(op, b));
	clear_flag(cpu, NF);
	set_flag(cpu, HF);
	done(cpu);
}

void CB_RES(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (mod_is_hl(cpu)) {
		switch (cpu->instruction.step) {
			case 1:
				YIELD
			case 2:
				cpu->instruction.op1 = READ_OPERAND_MOD(gbc);
				YIELD
		}
	} else {
		cpu->instruction.op1 = READ_OPERAND_MOD(gbc);
	}
	uint8_t b = (cpu->opcode - 0x80u) / 0x08u;

	WRITE_OPERAND_MOD(gbc, clear_bit(cpu->instruction.op1, b));
	done(cpu);
}

void CB_SET(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	if (mod_is_hl(cpu)) {
		switch (cpu->instruction.step) {
			case 1:
				YIELD
			case 2:
				cpu->instruction.op1 = READ_OPERAND_MOD(gbc);
				YIELD
		}
	} else {
		cpu->instruction.op1 = READ_OPERAND_MOD(gbc);
	}
	uint8_t b = (cpu->opcode - 0xC0u) / 0x08u;

	WRITE_OPERAND_MOD(gbc, set_bit(cpu->instruction.op1, b));
	done(cpu);
}

/* Helper functions */

uint8_t READ_OPERAND_MOD(struct gbcc_core *gbc)
{
	struct cpu *cpu = &gbc->cpu;
	uint8_t ret;
	switch (cpu->opcode % 0x08u) {
		case 0:
			return cpu->reg.b;
		case 1:
			return cpu->reg.c;
		case 2:
			return cpu->reg.d;
		case 3:
			return cpu->reg.e;
		case 4:
			return cpu->reg.h;
		case 5:
			return cpu->reg.l;
		case 6:
			ret = gbcc_memory_read(gbc, cpu->reg.hl);
			return ret;
		case 7:
			return cpu->reg.a;
		default:
			return 0;
	}
}

void WRITE_OPERAND_MOD(struct gbcc_core *gbc, uint8_t val)
{
	struct cpu *cpu = &gbc->cpu;
	switch (cpu->opcode % 0x08u) {
		case 0:
			cpu->reg.b = val;
			break;
		case 1:
			cpu->reg.c = val;
			break;
		case 2:
			cpu->reg.d = val;
			break;
		case 3:
			cpu->reg.e = val;
			break;
		case 4:
			cpu->reg.h = val;
			break;
		case 5:
			cpu->reg.l = val;
			break;
		case 6:
			gbcc_memory_write(gbc, cpu->reg.hl, val);
			break;
		case 7:
			cpu->reg.a = val;
			break;
	}
}

void WRITE_OPERAND_DIV(struct gbcc_core *gbc, uint8_t offset, uint8_t val)
{
	struct cpu *cpu = &gbc->cpu;
	switch ((cpu->opcode - offset) / 0x08u) {
		case 0:
			cpu->reg.b = val;
			break;
		case 1:
			cpu->reg.c = val;
			break;
		case 2:
			cpu->reg.d = val;
			break;
		case 3:
			cpu->reg.e = val;
			break;
		case 4:
			cpu->reg.h = val;
			break;
		case 5:
			cpu->reg.l = val;
			break;
		case 6:
			gbcc_memory_write(gbc, cpu->reg.hl, val);
			break;
		case 7:
			cpu->reg.a = val;
			break;
	}
}

bool mod_is_hl(struct cpu *cpu)
{
	return (cpu->opcode % 0x08u) == 6;
}

bool div_is_hl(struct cpu *cpu, uint8_t offset)
{
	return (cpu->opcode - offset) / 0x08u == 6;
}

void done(struct cpu *cpu)
{
	cpu->instruction.step = 0;
	cpu->instruction.running = false;
	cpu->instruction.prefix_cb = false;
}
