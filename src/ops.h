/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

#ifndef GBCC_OPS_H
#define GBCC_OPS_H

#include "core.h"
#include <stdint.h>

extern void (*const gbcc_ops[0x100])(struct gbcc_core *gbc);
extern const uint8_t gbcc_op_times[0x100];

/* Not really an opcode, but behaves like a cpu instruction */
void INTERRUPT(struct gbcc_core *gbc);

/* Miscellaneous */
void INVALID(struct gbcc_core *gbc);
void NOP(struct gbcc_core *gbc);
void HALT(struct gbcc_core *gbc);
void STOP(struct gbcc_core *gbc);
void DAA(struct gbcc_core *gbc);
void CPL(struct gbcc_core *gbc);
void SCF(struct gbcc_core *gbc);
void CCF(struct gbcc_core *gbc);
void EI(struct gbcc_core *gbc);
void DI(struct gbcc_core *gbc);

/* Loads */
void LD_REG_REG(struct gbcc_core *gbc);
void LD_REG_HL(struct gbcc_core *gbc);
void LD_d8(struct gbcc_core *gbc);
void LD_d16(struct gbcc_core *gbc);
void LD_A(struct gbcc_core *gbc);
void LD_a16(struct gbcc_core *gbc);
void LDH_a8(struct gbcc_core *gbc);
void LDH_C(struct gbcc_core *gbc);
void STORE_SP(struct gbcc_core *gbc);
void LD_HL_SP(struct gbcc_core *gbc);
void LD_SP_HL(struct gbcc_core *gbc);
void POP(struct gbcc_core *gbc);
void PUSH(struct gbcc_core *gbc);

/* ALU */
void ALU_OP(struct gbcc_core *gbc);
void INC_DEC_REG(struct gbcc_core *gbc);
void INC_DEC_HL(struct gbcc_core *gbc);
void INC_DEC_16_BIT(struct gbcc_core *gbc);
void ADD_HL(struct gbcc_core *gbc);
void ADD_SP(struct gbcc_core *gbc);
void SHIFT_A(struct gbcc_core *gbc);

/* Jumps */
void JP(struct gbcc_core *gbc);
void JP_HL(struct gbcc_core *gbc);
void JP_COND(struct gbcc_core *gbc);
void JR(struct gbcc_core *gbc);
void JR_COND(struct gbcc_core *gbc);

/* Calls */
void CALL(struct gbcc_core *gbc);
void CALL_COND(struct gbcc_core *gbc);
void RET(struct gbcc_core *gbc);
void RETI(struct gbcc_core *gbc);
void RET_COND(struct gbcc_core *gbc);
void RST(struct gbcc_core *gbc);

/* CB-prefix */
void PREFIX_CB(struct gbcc_core *gbc);
void CB_SHIFT_OP(struct gbcc_core *gbc);
void CB_BIT(struct gbcc_core *gbc);
void CB_RES(struct gbcc_core *gbc);
void CB_SET(struct gbcc_core *gbc);

#endif /* GBCC_OPS_H */
