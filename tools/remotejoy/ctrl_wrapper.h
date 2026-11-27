/* Recognize the controller wrappers before patching their call sites. */
#ifndef REMOTEJOY_CTRL_WRAPPER_H
#define REMOTEJOY_CTRL_WRAPPER_H

#include "../../psplink/mips_insn.h"

enum CtrlWrapperLayout
{
	CTRL_WRAPPER_UNKNOWN,
	CTRL_WRAPPER_LEGACY,
	CTRL_WRAPPER_FOUR_ARGS
};

/* Internal sceCtrl mode flags, independent of public PSP_CTRL button masks. */
enum CtrlWrapperMode
{
	CTRL_WRAPPER_PEEK = 0,
	CTRL_WRAPPER_NEGATIVE = 1,
	CTRL_WRAPPER_READ = 2
};

enum CtrlWrapperInstruction
{
	CTRL_WRAPPER_LEGACY_CALL = 2,
	CTRL_WRAPPER_FOUR_ARGS_CALL = 3,
	CTRL_WRAPPER_STACK_SIZE = 16
};

static inline enum CtrlWrapperLayout ctrl_wrapper_layout(const uint32_t *code, unsigned int mode)
{
	/* Firmware 6.61 passes the port in a2 and the read/negative flags in a3.
	 * Verify the complete wrapper, including the mode in the delay slot. */
	uint32_t mode_inst = mode ? mips_addiu(MIPS_REG_A3, MIPS_REG_ZERO, (uint16_t)mode) :
		mips_move(MIPS_REG_A3, MIPS_REG_ZERO);
	if(code[0] == mips_addiu(MIPS_REG_SP, MIPS_REG_SP, (uint16_t)-CTRL_WRAPPER_STACK_SIZE) &&
		code[1] == mips_move(MIPS_REG_A2, MIPS_REG_ZERO) &&
		code[2] == mips_sw(MIPS_REG_RA, 0, MIPS_REG_SP) &&
		mips_opcode(code[CTRL_WRAPPER_FOUR_ARGS_CALL]) == MIPS_OP_JAL &&
		code[4] == mode_inst && code[5] == mips_lw(MIPS_REG_RA, 0, MIPS_REG_SP) &&
		code[6] == mips_jr(MIPS_REG_RA) &&
		code[7] == mips_addiu(MIPS_REG_SP, MIPS_REG_SP, CTRL_WRAPPER_STACK_SIZE))
	{
		return CTRL_WRAPPER_FOUR_ARGS;
	}
	/* The original RemoteJoy hook uses the three-argument call at +8. */
	if(mips_opcode(code[CTRL_WRAPPER_LEGACY_CALL]) == MIPS_OP_JAL)
	{
		return CTRL_WRAPPER_LEGACY;
	}
	return CTRL_WRAPPER_UNKNOWN;
}

#endif
