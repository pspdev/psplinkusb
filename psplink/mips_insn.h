/* MIPS instruction encoding shared by firmware hooks and the debugger. */
#ifndef PSPLINK_MIPS_INSN_H
#define PSPLINK_MIPS_INSN_H

#include <stdint.h>

enum MipsOpcode
{
	MIPS_OP_SPECIAL = 0,
	MIPS_OP_J = 2,
	MIPS_OP_JAL = 3,
	MIPS_OP_ADDIU = 9,
	MIPS_OP_LW = 35,
	MIPS_OP_SW = 43
};

enum MipsFunction
{
	MIPS_FUNC_JR = 8,
	MIPS_FUNC_JALR = 9,
	MIPS_FUNC_ADDU = 33
};

enum MipsRegister
{
	MIPS_REG_ZERO = 0,
	MIPS_REG_V0 = 2,
	MIPS_REG_A2 = 6,
	MIPS_REG_A3 = 7,
	MIPS_REG_SP = 29,
	MIPS_REG_RA = 31
};

/* Bit positions and masks defined by the MIPS instruction formats. */
enum MipsInstructionField
{
	MIPS_OPCODE_SHIFT = 26,
	MIPS_RS_SHIFT = 21,
	MIPS_RT_SHIFT = 16,
	MIPS_RD_SHIFT = 11,
	MIPS_REGISTER_MASK = 0x1f,
	MIPS_FUNCTION_MASK = 0x3f,
	MIPS_JUMP_INDEX_MASK = 0x03ffffff,
	MIPS_JUMP_ADDRESS_SHIFT = 2
};

#define MIPS_JUMP_REGION_MASK UINT32_C(0xf0000000)

static inline unsigned int mips_opcode(uint32_t instruction)
{
	return instruction >> MIPS_OPCODE_SHIFT;
}

static inline unsigned int mips_function(uint32_t instruction)
{
	return instruction & MIPS_FUNCTION_MASK;
}

static inline unsigned int mips_rs(uint32_t instruction)
{
	return (instruction >> MIPS_RS_SHIFT) & MIPS_REGISTER_MASK;
}

static inline uint32_t mips_jump_target(uint32_t pc, uint32_t instruction)
{
	return ((pc + sizeof(uint32_t)) & MIPS_JUMP_REGION_MASK) |
		((instruction & MIPS_JUMP_INDEX_MASK) << MIPS_JUMP_ADDRESS_SHIFT);
}

static inline uint32_t mips_encode_jump(enum MipsOpcode opcode, uint32_t target)
{
	return ((uint32_t)opcode << MIPS_OPCODE_SHIFT) |
		((target >> MIPS_JUMP_ADDRESS_SHIFT) & MIPS_JUMP_INDEX_MASK);
}

static inline uint32_t mips_encode_i(enum MipsOpcode opcode, enum MipsRegister rs,
		enum MipsRegister rt, uint16_t immediate)
{
	return ((uint32_t)opcode << MIPS_OPCODE_SHIFT) | ((uint32_t)rs << MIPS_RS_SHIFT) |
		((uint32_t)rt << MIPS_RT_SHIFT) | immediate;
}

static inline uint32_t mips_encode_r(enum MipsFunction function, enum MipsRegister rs,
		enum MipsRegister rt, enum MipsRegister rd)
{
	return ((uint32_t)rs << MIPS_RS_SHIFT) | ((uint32_t)rt << MIPS_RT_SHIFT) |
		((uint32_t)rd << MIPS_RD_SHIFT) | (uint32_t)function;
}

static inline uint32_t mips_addiu(enum MipsRegister rt, enum MipsRegister rs, uint16_t immediate)
{
	return mips_encode_i(MIPS_OP_ADDIU, rs, rt, immediate);
}

static inline uint32_t mips_lw(enum MipsRegister rt, int16_t offset, enum MipsRegister base)
{
	return mips_encode_i(MIPS_OP_LW, base, rt, (uint16_t)offset);
}

static inline uint32_t mips_sw(enum MipsRegister rt, int16_t offset, enum MipsRegister base)
{
	return mips_encode_i(MIPS_OP_SW, base, rt, (uint16_t)offset);
}

static inline uint32_t mips_move(enum MipsRegister rd, enum MipsRegister rs)
{
	return mips_encode_r(MIPS_FUNC_ADDU, rs, MIPS_REG_ZERO, rd);
}

static inline uint32_t mips_jr(enum MipsRegister rs)
{
	return mips_encode_r(MIPS_FUNC_JR, rs, MIPS_REG_ZERO, MIPS_REG_ZERO);
}

static inline uint32_t mips_nop(void)
{
	return 0; /* sll $zero, $zero, 0 */
}

#endif
