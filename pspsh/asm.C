/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * asm.c - PSPLINK pc terminal simple MIPS assembler
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/pspsh/asm.C $
 * $Id: asm.C 2200 2007-03-08 21:21:20Z tyranid $
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>

/* Format codes
 * %d - Rd
 * %t - Rt
 * %s - Rs
 * %i - 16bit signed immediate
 * %I - 16bit unsigned immediate (always printed in hex)
 * %o - 16bit signed offset (rt base)
 * %O - 16bit signed offset (PC relative)
 * %V - 16bit signed offset (rs base)
 * %j - 26bit absolute offset
 * %a - SA
 * %0 - Cop0 register
 * %1 - Cop1 register
 * %p - General cop (i.e. numbered) register
 * %n - ins/ext size
 * %r - Debug register
 * %k - Cache function
 * %D - Fd
 * %T - Ft
 * %S - Fs
 * %x? - Vt (? is (s/scalar, p/pair, t/triple, q/quad, m/matrix pair, n/matrix triple, o/matrix quad)
 * %y? - Vs
 * %z? - Vd
 * %X? - Vo (? is (s, q))
 * %Y - VFPU offset
 * %Z - VFPU condition code
 * %v? - VFPU immediate, ? (3, 5, 8)
 * %c - code (for break)
 * %C - code (for syscall)
 * %? - Indicates vmmul special exception
 */

#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)
#define RD(op) ((op >> 11) & 0x1F)
#define FT(op) ((op >> 16) & 0x1F)
#define FS(op) ((op >> 11) & 0x1F)
#define FD(op) ((op >> 6) & 0x1F)
#define SA(op) ((op >> 6)  & 0x1F)
#define IMM(op) ((signed short) (op & 0xFFFF))
#define IMMU(op) ((unsigned short) (op & 0xFFFF))
#define JUMP(op, pc) ((pc & 0xF0000000) | ((op & 0x3FFFFFF) << 2))
#define CODE(op) ((op >> 6) & 0xFFFFF)
#define SIZE(op) ((op >> 11) & 0x1F)
#define POS(op)  ((op >> 6) & 0x1F)
#define VO(op)   (((op & 3) << 5) | ((op >> 16) & 0x1F))
#define VCC(op)  ((op >> 18) & 7)
#define VD(op)   (op & 0x7F)
#define VS(op)   ((op >> 8) & 0x7F)
#define VT(op)   ((op >> 16) & 0x7F)

#define SETRT(op, reg) (op |= ((reg) << 16))
#define SETRS(op, reg) (op |= ((reg) << 21))
#define SETRD(op, reg) (op |= ((reg) << 11))
#define SETFT(op, reg) (op |= ((reg) << 16))
#define SETFS(op, reg) (op |= ((reg) << 11))
#define SETFD(op, reg) (op |= ((reg) << 6))
#define SETSA(op, val) (op |= ((val) << 6))

#if 0
#define SETIMM(op, imm) ((signed short) (op & 0xFFFF))
#define SETIMMU(op, imm) ((unsigned short) (op & 0xFFFF))
#define SETJUMP(op, addr) ((0xF0000000) | ((op & 0x3FFFFFF) << 2))
#define SETCODE(op, code) ((op >> 6) & 0xFFFFF)
#define SETSIZE(op, size) ((op >> 11) & 0x1F)
#define SETPOS(op, pos)  ((op >> 6) & 0x1F)
#define SETVO(op, vo)   (((op & 3) << 5) | ((op >> 16) & 0x1F))
#define SETVCC(op, vcc)  ((op >> 18) & 7)
#define SETVD(op, vd)   (op & 0x7F)
#define SETVS(op, vs)   ((op >> 8) & 0x7F)
#define SETVT(op, vt)   ((op >> 16) & 0x7F)
#endif

struct Instruction
{
	const char *name;
	unsigned int opcode;
	unsigned int mask;
	const char *fmt;
	int operands;
};

#define INSTR_TYPE_PSP    1
#define INSTR_TYPE_B      2
#define INSTR_TYPE_JUMP   4
#define INSTR_TYPE_JAL    8

#define INSTR_TYPE_BRANCH (INSTR_TYPE_B | INSTR_TYPE_JUMP | INSTR_TYPE_JAL)

#define ADDR_TYPE_NONE 0
#define ADDR_TYPE_16   1
#define ADDR_TYPE_26   2
#define ADDR_TYPE_REG  3

const char *regName[32] =
{
    "zr", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", 
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

struct Instruction macro[] = 
{
	/* Macro instructions */
	{ "nop",		0x00000000, 0xFFFFFFFF, "" 			, 0 },
	{ "li",     	0x24000000, 0xFFE00000, "%t, %i" 	, 2 },
	{ "li",			0x34000000, 0xFFE00000, "%t, %I"	, 2 },
	{ "move", 		0x00000021, 0xFC1F07FF, "%d, %s"	, 2 },
	{ "move",   	0x00000025, 0xFC1F07FF, "%d, %s"	, 2 },
	{ "b",			0x10000000, 0xFFFF0000, "%O"		, 1 },
	{ "b",			0x04010000, 0xFFFF0000, "%O"		, 1 },
	{ "bal",		0x04110000, 0xFFFF0000, "%O"		, 1 },
	{ "bnez",		0x14000000, 0xFC1F0000,	"%s, %O"	, 2 },
	{ "bnezl",		0x54000000, 0xFC1F0000,	"%s, %O"	, 2 },
	{ "beqz",		0x10000000, 0xFC1F0000,	"%s, %O", 2 },
	{ "beqzl",		0x50000000, 0xFC1F0000,	"%s, %O", 2 },
	{ "neg",		0x00000022, 0xFFE007FF,	"%d, %t"	, 2 },
	{ "negu",		0x00000023, 0xFFE007FF,	"%d, %t"	, 2 },
	{ "not",		0x00000027, 0xFC1F07FF,	"%d, %s"	, 2 },
	{ "jalr",		0x0000F809, 0xFC1FFFFF,	"%s", 1 },
};

static struct Instruction g_inst[] = 
{
	/* MIPS instructions */
	{ "add",		0x00000020, 0xFC0007FF, "%d, %s, %t", 3 },
	{ "addi",		0x20000000, 0xFC000000, "%t, %s, %i", 3 },
	{ "addiu",		0x24000000, 0xFC000000, "%t, %s, %i", 3 },
	{ "addu",		0x00000021, 0xFC0007FF, "%d, %s, %t", 3 },
	{ "and",		0x00000024, 0xFC0007FF,	"%d, %s, %t", 3 },
	{ "andi",		0x30000000, 0xFC000000,	"%t, %s, %I", 3 },
	{ "beq",		0x10000000, 0xFC000000,	"%s, %t, %O", 3 },
	{ "beql",		0x50000000, 0xFC000000,	"%s, %t, %O", 3 },
	{ "bgez",		0x04010000, 0xFC1F0000,	"%s, %O", 2 },
	{ "bgezal",		0x04110000, 0xFC1F0000,	"%s, %0", 2 },
	{ "bgezl",		0x04030000, 0xFC1F0000,	"%s, %O", 2 },
	{ "bgtz",		0x1C000000, 0xFC1F0000,	"%s, %O", 2 },
	{ "bgtzl",		0x5C000000, 0xFC1F0000,	"%s, %O", 2 },
	{ "bitrev",		0x7C000520, 0xFFE007FF, "%d, %t", 2 },
	{ "blez",		0x18000000, 0xFC1F0000,	"%s, %O", 2 },
	{ "blezl",		0x58000000, 0xFC1F0000,	"%s, %O", 2 },
	{ "bltz",		0x04000000, 0xFC1F0000,	"%s, %O", 2 },
	{ "bltzl",		0x04020000, 0xFC1F0000,	"%s, %O", 2 },
	{ "bltzal",		0x04100000, 0xFC1F0000,	"%s, %O", 2 },
	{ "bltzall",	0x04120000, 0xFC1F0000,	"%s, %O", 2 },
	{ "bne",		0x14000000, 0xFC000000,	"%s, %t, %O", 3 },
	{ "bnel",		0x54000000, 0xFC000000,	"%s, %t, %O", 3 },
	{ "break",		0x0000000D, 0xFC00003F,	"%c", 1 },
	{ "cache",		0xBC000000, 0xFC000000, "%k, %o", 2 },
	{ "cfc0",		0x40400000, 0xFFE007FF,	"%t, %p", 2 },
	{ "clo",		0x00000017, 0xFC1F07FF, "%d, %s", 2 },
	{ "clz",		0x00000016, 0xFC1F07FF, "%d, %s", 2 },
	{ "ctc0",		0x40C00000, 0xFFE007FF,	"%t, %p", 2 },
	{ "max",		0x0000002C, 0xFC0007FF, "%d, %s, %t", 3 },
	{ "min",		0x0000002D, 0xFC0007FF, "%d, %s, %t", 3 },
	{ "dbreak",		0x7000003F, 0xFFFFFFFF,	"", 0 },
	{ "div",		0x0000001A, 0xFC00FFFF, "%s, %t", 2 },
	{ "divu",		0x0000001B, 0xFC00FFFF, "%s, %t", 2 },
	{ "dret",		0x7000003E, 0xFFFFFFFF,	"", 0 },
	{ "eret",		0x42000018, 0xFFFFFFFF, "", 0 },
	{ "ext",		0x7C000000, 0xFC00003F, "%t, %s, %a, %n", 4 },
	{ "ins",		0x7C000004, 0xFC00003F, "%t, %s, %a, %n", 4 },
	{ "j",			0x08000000, 0xFC000000,	"%j", 1 },
	{ "jr",			0x00000008, 0xFC1FFFFF,	"%s", 1 },
	{ "jalr",		0x00000009, 0xFC1F07FF,	"%s, %d", 2 },
	{ "jal",		0x0C000000, 0xFC000000,	"%j", 1 },
	{ "lb",			0x80000000, 0xFC000000,	"%t, %o", 2 },
	{ "lbu",		0x90000000, 0xFC000000,	"%t, %o", 2 },
	{ "lh",			0x84000000, 0xFC000000,	"%t, %o", 2 },
	{ "lhu",		0x94000000, 0xFC000000,	"%t, %o", 2 },
	{ "ll",			0xC0000000, 0xFC000000,	"%t, %O", 2 },
	{ "lui",		0x3C000000, 0xFFE00000,	"%t, %I", 2 },
	{ "lw",			0x8C000000, 0xFC000000,	"%t, %o", 2 },
	{ "lwl",		0x88000000, 0xFC000000,	"%t, %o", 2 },
	{ "lwr",		0x98000000, 0xFC000000,	"%t, %o", 2 },
	{ "madd",		0x0000001C, 0xFC00FFFF, "%s, %t", 2 },
	{ "maddu",		0x0000001D, 0xFC00FFFF, "%s, %t", 2 },
	{ "mfc0",		0x40000000, 0xFFE007FF,	"%t, %0", 2 },
	{ "mfdr",		0x7000003D, 0xFFE007FF,	"%t, %r", 2 },
	{ "mfhi",		0x00000010, 0xFFFF07FF, "%d", 1 },
	{ "mfic",		0x70000024, 0xFFE007FF, "%t, %p", 2 },
	{ "mflo",		0x00000012, 0xFFFF07FF, "%d", 1 },
	{ "movn",		0x0000000B, 0xFC0007FF, "%d, %s, %t", 3 },
	{ "movz",		0x0000000A, 0xFC0007FF, "%d, %s, %t", 3 },
	{ "msub",		0x0000002e, 0xfc00ffff, "%d, %t", 2 },
	{ "msubu",		0x0000002f, 0xfc00ffff, "%d, %t", 2 },
	{ "mtc0",		0x40800000, 0xFFE007FF,	"%t, %0", 2 },
	{ "mtdr",		0x7080003D, 0xFFE007FF,	"%t, %r", 2 },
	{ "mtic",		0x70000026, 0xFFE007FF, "%t, %p", 2 },
	{ "halt",       0x70000000, 0xFFFFFFFF, "" , 0 },
	{ "mthi",		0x00000011, 0xFC1FFFFF,	"%s", 1 },
	{ "mtlo",		0x00000013, 0xFC1FFFFF,	"%s", 1 },
	{ "mult",		0x00000018, 0xFC00FFFF, "%s, %t", 2 },
	{ "multu",		0x00000019, 0xFC0007FF, "%s, %t", 2 },
	{ "nor",		0x00000027, 0xFC0007FF,	"%d, %s, %t", 3 },
	{ "or",			0x00000025, 0xFC0007FF,	"%d, %s, %t", 3 },
	{ "ori",		0x34000000, 0xFC000000,	"%t, %s, %I", 3 },
	{ "rotr",		0x00200002, 0xFFE0003F, "%d, %t, %a", 3 },
	{ "rotv",		0x00000046, 0xFC0007FF, "%d, %t, %s", 3 },
	{ "seb",		0x7C000420, 0xFFE007FF,	"%d, %t", 2 },
	{ "seh",		0x7C000620, 0xFFE007FF,	"%d, %t", 2 },
	{ "sb",			0xA0000000, 0xFC000000,	"%t, %o", 2 },
	{ "sh",			0xA4000000, 0xFC000000,	"%t, %o", 2 },
	{ "sllv",		0x00000004, 0xFC0007FF,	"%d, %t, %s", 3 },
	{ "sll",		0x00000000, 0xFFE0003F,	"%d, %t, %a", 3 },
	{ "slt",		0x0000002A, 0xFC0007FF,	"%d, %s, %t", 3 },
	{ "slti",		0x28000000, 0xFC000000,	"%t, %s, %i", 3 },
	{ "sltiu",		0x2C000000, 0xFC000000,	"%t, %s, %i", 3 },
	{ "sltu",		0x0000002B, 0xFC0007FF,	"%d, %s, %t", 3 },
	{ "sra",		0x00000003, 0xFFE0003F,	"%d, %t, %a", 3 },
	{ "srav",		0x00000007, 0xFC0007FF,	"%d, %t, %s", 3 },
	{ "srlv",		0x00000006, 0xFC0007FF,	"%d, %t, %s", 3 },
	{ "srl",		0x00000002, 0xFFE0003F,	"%d, %t, %a", 3 },
	{ "sw",			0xAC000000, 0xFC000000,	"%t, %o", 2 },
	{ "swl",		0xA8000000, 0xFC000000,	"%t, %o", 2 },
	{ "swr",		0xB8000000, 0xFC000000,	"%t, %o", 2 },
	{ "sub",		0x00000022, 0xFC0007FF,	"%d, %s, %t", 3 },
	{ "subu",		0x00000023, 0xFC0007FF,	"%d, %s, %t", 3 },
	{ "sync",		0x0000000F, 0xFFFFFFFF,	"", 0 },
	{ "syscall",	0x0000000C, 0xFC00003F,	"%C", 1 },
	{ "xor",		0x00000026, 0xFC0007FF,	"%d, %s, %t", 3 },
	{ "xori",		0x38000000, 0xFC000000,	"%t, %s, %I", 3 },
	{ "wsbh",		0x7C0000A0, 0xFFE007FF,	"%d, %t", 2 },
	{ "wsbw",		0x7C0000E0, 0xFFE007FF, "%d, %t", 2 },

	/* We dont currently support fpu or vfpu */
#if 0
	/* FPU instructions */
	{"abs.s",	0x46000005, 0xFFFF003F, "%D, %S", ADDR_TYPE_NONE, 0 },
	{"add.s",	0x46000000, 0xFFE0003F,	"%D, %S, %T", ADDR_TYPE_NONE, 0 },
	{"bc1f",	0x45000000, 0xFFFF0000,	"%O", ADDR_TYPE_16, INSTR_TYPE_B },
	{"bc1fl",	0x45020000, 0xFFFF0000,	"%O", ADDR_TYPE_16, INSTR_TYPE_B },
	{"bc1t",	0x45010000, 0xFFFF0000,	"%O", ADDR_TYPE_16, INSTR_TYPE_B },
	{"bc1tl",	0x45030000, 0xFFFF0000,	"%O", ADDR_TYPE_16, INSTR_TYPE_B },
	{"c.f.s",	0x46000030, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.un.s",	0x46000031, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.eq.s",	0x46000032, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.ueq.s",	0x46000033, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.olt.s",	0x46000034, 0xFFE007FF,	"%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.ult.s",	0x46000035, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.ole.s",	0x46000036, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.ule.s",	0x46000037, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.sf.s",	0x46000038, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.ngle.s",0x46000039, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.seq.s",	0x4600003A, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.ngl.s",	0x4600003B, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.lt.s",	0x4600003C, 0xFFE007FF,	"%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.nge.s",	0x4600003D, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.le.s",	0x4600003E, 0xFFE007FF,	"%S, %T", ADDR_TYPE_NONE, 0 },
	{"c.ngt.s",	0x4600003F, 0xFFE007FF, "%S, %T", ADDR_TYPE_NONE, 0 },
	{"ceil.w.s",0x4600000E, 0xFFFF003F, "%D, %S", ADDR_TYPE_NONE, 0 },
	{"cfc1",	0x44400000, 0xFFE007FF, "%t, %p", ADDR_TYPE_NONE, 0 },
	{"ctc1",	0x44c00000, 0xFFE007FF, "%t, %p", ADDR_TYPE_NONE, 0 },
	{"cvt.s.w",	0x46800020, 0xFFFF003F, "%D, %S", ADDR_TYPE_NONE, 0 },
	{"cvt.w.s",	0x46000024, 0xFFFF003F, "%D, %S", ADDR_TYPE_NONE, 0 },
	{"div.s",	0x46000003, 0xFFE0003F, "%D, %S, %T", ADDR_TYPE_NONE, 0 },
	{"floor.w.s",0x4600000F, 0xFFFF003F,"%D, %S", ADDR_TYPE_NONE, 0 },
	{"lwc1",	0xc4000000, 0xFC000000, "%T, %o", ADDR_TYPE_NONE, 0 },
	{"mfc1",	0x44000000, 0xFFE007FF, "%t, %1", ADDR_TYPE_NONE, 0 },
	{"mov.s",	0x46000006, 0xFFFF003F, "%D, %S", ADDR_TYPE_NONE, 0 },
	{"mtc1",	0x44800000, 0xFFE007FF, "%t, %1", ADDR_TYPE_NONE, 0 },
	{"mul.s",	0x46000002, 0xFFE0003F, "%D, %S, %T", ADDR_TYPE_NONE, 0 },
	{"neg.s",	0x46000007, 0xFFFF003F, "%D, %S", ADDR_TYPE_NONE, 0 },
	{"round.w.s",0x4600000C, 0xFFFF003F,"%D, %S", ADDR_TYPE_NONE, 0 },
	{"sqrt.s",	0x46000004, 0xFFFF003F, "%D, %S", ADDR_TYPE_NONE, 0 },
	{"sub.s",	0x46000001, 0xFFE0003F, "%D, %S, %T", ADDR_TYPE_NONE, 0 },
	{"swc1",	0xe4000000, 0xFC000000, "%T, %o", ADDR_TYPE_NONE, 0 },
	{"trunc.w.s",0x4600000D, 0xFFFF003F,"%D, %S", ADDR_TYPE_NONE, 0 },
	
	/* VPU instructions */
	{ "bvf",	 0x49000000, 0xFFE30000, "%Z, %O" , ADDR_TYPE_16, INSTR_TYPE_PSP | INSTR_TYPE_B },
	{ "bvfl",	 0x49020000, 0xFFE30000, "%Z, %O" , ADDR_TYPE_16, INSTR_TYPE_PSP | INSTR_TYPE_B },
	{ "bvt",	 0x49010000, 0xFFE30000, "%Z, %O" , ADDR_TYPE_16, INSTR_TYPE_PSP | INSTR_TYPE_B },
	{ "bvtl",	 0x49030000, 0xFFE30000, "%Z, %O" , ADDR_TYPE_16, INSTR_TYPE_PSP | INSTR_TYPE_B },
	{ "lv.q",	 0xD8000000, 0xFC000002, "%Xq, %Y" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "lv.s",	 0xC8000000, 0xFC000000, "%Xs, %Y" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "lvl.q",	 0xD4000000, 0xFC000002, "%Xq, %Y" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "lvr.q",	 0xD4000002, 0xFC000002, "%Xq, %Y" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "mfv",	 0x48600000, 0xFFE0FF80, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "mfvc",	 0x48600000, 0xFFE0FF00, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "mtv",	 0x48E00000, 0xFFE0FF80, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "mtvc",	 0x48E00000, 0xFFE0FF00, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "sv.q",	 0xF8000000, 0xFC000002, "%Xq, %Y" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "sv.s",	 0xE8000000, 0xFC000000, "%Xs, %Y" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "svl.q",	 0xF4000000, 0xFC000002, "%Xq, %Y" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "svr.q",	 0xF4000002, 0xFC000002, "%Xq, %Y" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vabs.p",	 0xD0010080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vabs.q",	 0xD0018080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vabs.s",	 0xD0010000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vabs.t",	 0xD0018000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vadd.p",	 0x60000080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vadd.q",	 0x60008080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vadd.s",	 0x60000000, 0xFF808080, "%zs, %yz, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vadd.t",	 0x60008000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vasin.p", 0xD0170080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vasin.q", 0xD0178080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vasin.s", 0xD0170000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vasin.t", 0xD0178000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vavg.p",	 0xD0470080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vavg.q",	 0xD0478080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vavg.t",	 0xD0478000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vbfy1.p", 0xD0420080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vbfy1.q", 0xD0428080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vbfy2.q", 0xD0438080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmovf.p", 0xD2A80080, 0xFFF88080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmovf.q",0xD2A88080, 0xFFF88080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmovf.s", 0xD2A80000, 0xFFF88080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmovf.t",0xD2A88000, 0xFFF88080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmovt.p", 0xD2A00080, 0xFFF88080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmovt.q",0xD2A08080, 0xFFF88080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmovt.s", 0xD2A00000, 0xFFF88080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmovt.t",0xD2A08000, 0xFFF88080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.p",	 0x6C000080, 0xFF8080F0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.p",	 0x6C000080, 0xFFFF80F0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.p",	 0x6C000080, 0xFFFFFFF0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.q",	 0x6C008080, 0xFF8080F0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.q",	 0x6C008080, 0xFFFF80F0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.q",	 0x6C008080, 0xFFFFFFF0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.s",	 0x6C000000, 0xFF8080F0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.s",	 0x6C000000, 0xFFFF80F0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.s",	 0x6C000000, 0xFFFFFFF0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.t",	 0x6C008000, 0xFF8080F0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.t",	 0x6C008000, 0xFFFF80F0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcmp.t",	 0x6C008000, 0xFFFFFFF0, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcos.p",	 0xD0130080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcos.q",	 0xD0138080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcos.s",	 0xD0130000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcos.t",	 0xD0138000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcrs.t",	 0x66808000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcrsp.t", 0xF2808000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcst.p",	 0xD0600080, 0xFFE0FF80, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcst.q",	 0xD0608080, 0xFFE0FF80, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcst.s",	 0xD0600000, 0xFFE0FF80, "%zs, %ys, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vcst.t",	 0xD0608000, 0xFFE0FF80, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vdet.p",	 0x67000080, 0xFF808080, "%zs, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vdiv.p",	 0x63800080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vdiv.q",	 0x63808080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vdiv.s",	 0x63800000, 0xFF808080, "%zs, %yz, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vdiv.t",	 0x63808000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vdot.p",	 0x64800080, 0xFF808080, "%zs, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vdot.q",	 0x64808080, 0xFF808080, "%zs, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vdot.t",	 0x64808000, 0xFF808080, "%zs, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vexp2.p", 0xD0140080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vexp2.q", 0xD0148080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vexp2.s", 0xD0140000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vexp2.t", 0xD0148000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2h.p",	 0xD0320080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2h.q",	 0xD0328080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2id.p", 0xD2600080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2id.q", 0xD2608080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2id.s", 0xD2600000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2id.t", 0xD2608000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2in.p", 0xD2000080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2in.q", 0xD2008080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2in.s", 0xD2000000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2in.t", 0xD2008000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2iu.p", 0xD2400080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2iu.q", 0xD2408080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2iu.s", 0xD2400000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2iu.t", 0xD2408000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2iz.p", 0xD2200080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2iz.q", 0xD2208080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2iz.s", 0xD2200000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vf2iz.t", 0xD2208000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vfad.p",	 0xD0460080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vfad.q",	 0xD0468080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vfad.t",	 0xD0468000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vfim.s",	 0xDF800000, 0xFF800000, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vflush",	 0xFFFF040D, 0xFFFFFFFF, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vh2f.p",	 0xD0330080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vh2f.s",	 0xD0330000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vhdp.p",	 0x66000080, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vhdp.q",	 0x66008080, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vhdp.t",	 0x66008000, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vhtfm2.p", 0xF0800000, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vhtfm3.t",0xF1000080, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vhtfm4.q",0xF1808000, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2c.q",	 0xD03D8080, 0xFFFF8080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2f.p",	 0xD2800080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2f.q",	 0xD2808080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2f.s",	 0xD2800000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2f.t",	 0xD2808000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2s.p",	 0xD03F0080, 0xFFFF8080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2s.q",	 0xD03F8080, 0xFFFF8080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2uc.q", 0xD03C8080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2us.p", 0xD03E0080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vi2us.q", 0xD03E8080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vidt.p",	 0xD0030080, 0xFFFFFF80, "%zp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vidt.q",	 0xD0038080, 0xFFFFFF80, "%zq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "viim.s",	 0xDF000000, 0xFF800000, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vlgb.s",	 0xD0370000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vlog2.p", 0xD0150080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vlog2.q", 0xD0158080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vlog2.s", 0xD0150000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vlog2.t", 0xD0158000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmax.p",	 0x6D800080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmax.q",	 0x6D808080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmax.s",	 0x6D800000, 0xFF808080, "%zs, %ys, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmax.t",	 0x6D808000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmfvc",	 0xD0500000, 0xFFFF0080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmidt.p", 0xF3830080, 0xFFFFFF80, "%zp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmidt.q", 0xF3838080, 0xFFFFFF80, "%zq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmidt.t", 0xF3838000, 0xFFFFFF80, "%zt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmin.p",	 0x6D000080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmin.q",	 0x6D008080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmin.s",	 0x6D000000, 0xFF808080, "%zs, %ys, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmin.t",	 0x6D008000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmmov.p", 0xF3800080, 0xFFFF8080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmmov.q", 0xF3808080, 0xFFFF8080, "%zo, %yo" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmmov.t", 0xF3808000, 0xFFFF8080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmmul.p", 0xF0000080, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmmul.q", 0xF0008080, 0xFF808080, "%?%zo, %yo, %xo" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmmul.t", 0xF0008000, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmone.p", 0xF3870080, 0xFFFFFF80, "%zp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmone.q", 0xF3878080, 0xFFFFFF80, "%zq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmone.t", 0xF3878000, 0xFFFFFF80, "%zt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmov.p",	 0xD0000080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmov.q",	 0xD0008080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmov.s",	 0xD0000000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmov.t",	 0xD0008000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmscl.p", 0xF2000080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmscl.q", 0xF2008080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmscl.t", 0xF2008000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmtvc",	 0xD0510000, 0xFFFF8000, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmul.p",	 0x64000080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmul.q",	 0x64008080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmul.s",	 0x64000000, 0xFF808080, "%zs, %ys, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmul.t",	 0x64008000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmzero.p", 0xF3860080, 0xFFFFFF80, "%zp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmzero.q",0xF3868080, 0xFFFFFF80, "%zq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vmzero.t",0xF3868000, 0xFFFFFF80, "%zt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vneg.p",	 0xD0020080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vneg.q",	 0xD0028080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vneg.s",	 0xD0020000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vneg.t",	 0xD0028000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vnop",	 0xFFFF0000, 0xFFFFFFFF, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vnrcp.p", 0xD0180080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vnrcp.q", 0xD0188080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vnrcp.s", 0xD0180000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vnrcp.t", 0xD0188000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vnsin.p", 0xD01A0080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vnsin.q", 0xD01A8080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vnsin.s", 0xD01A0000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vnsin.t", 0xD01A8000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vocp.p",	 0xD0440080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vocp.q",	 0xD0448080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vocp.s",	 0xD0440000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vocp.t",	 0xD0448000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vone.p",	 0xD0070080, 0xFFFFFF80, "%zp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vone.q",	 0xD0078080, 0xFFFFFF80, "%zq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vone.s",	 0xD0070000, 0xFFFFFF80, "%zs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vone.t",	 0xD0078000, 0xFFFFFF80, "%zt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vpfxd",	 0xDE000000, 0xFF000000, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vpfxs",	 0xDC000000, 0xFF000000, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vpfxt",	 0xDD000000, 0xFF000000, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vqmul.q", 0xF2808080, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrcp.p",	 0xD0100080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrcp.q",	 0xD0108080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrcp.s",	 0xD0100000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrcp.t",	 0xD0108000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrexp2.p",0xD01C0080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrexp2.q",0xD01C8080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrexp2.s", 0xD01C0000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrexp2.t",0xD01C8000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndf1.p", 0xD0220080, 0xFFFFFF80, "%zp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndf1.q",0xD0228080, 0xFFFFFF80, "%zq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndf1.s", 0xD0220000, 0xFFFFFF80, "%zs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndf1.t",0xD0228000, 0xFFFFFF80, "%zt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndf2.p", 0xD0230080, 0xFFFFFF80, "%zp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndf2.q",0xD0238080, 0xFFFFFF80, "%zq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndf2.s", 0xD0230000, 0xFFFFFF80, "%zs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndf2.t",0xD0238000, 0xFFFFFF80, "%zt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndi.p", 0xD0210080, 0xFFFFFF80, "%zp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndi.q", 0xD0218080, 0xFFFFFF80, "%zq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndi.s", 0xD0210000, 0xFFFFFF80, "%zs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrndi.t", 0xD0218000, 0xFFFFFF80, "%zt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrnds.s", 0xD0200000, 0xFFFF80FF, "%ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrot.p",	 0xF3A00080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrot.q",	 0xF3A08080, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrot.t",	 0xF3A08000, 0xFFE08080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrsq.p",	 0xD0110080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrsq.q",	 0xD0118080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrsq.s",	 0xD0110000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vrsq.t",	 0xD0118000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vs2i.p",	 0xD03B0080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vs2i.s",	 0xD03B0000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsat0.p", 0xD0040080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsat0.q", 0xD0048080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsat0.s", 0xD0040000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsat0.t", 0xD0048000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsat1.p", 0xD0050080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsat1.q", 0xD0058080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsat1.s", 0xD0050000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsat1.t", 0xD0058000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsbn.s",	 0x61000000, 0xFF808080, "%zs, %ys, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsbz.s",	 0xD0360000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vscl.p",	 0x65000080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vscl.q",	 0x65008080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vscl.t",	 0x65008000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vscmp.p", 0x6E800080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vscmp.q", 0x6E808080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vscmp.s", 0x6E800000, 0xFF808080, "%zs, %ys, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vscmp.t", 0x6E808000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsge.p",	 0x6F000080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsge.q",	 0x6F008080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsge.s",	 0x6F000000, 0xFF808080, "%zs, %ys, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsge.t",	 0x6F008000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsgn.p",	 0xD04A0080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsgn.q",	 0xD04A8080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsgn.s",	 0xD04A0000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsgn.t",	 0xD04A8000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsin.p",	 0xD0120080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsin.q",	 0xD0128080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsin.s",	 0xD0120000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsin.t",	 0xD0128000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vslt.p",	 0x6F800080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vslt.q",	 0x6F808080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vslt.s",	 0x6F800000, 0xFF808080, "%zs, %ys, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vslt.t",	 0x6F808000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsocp.p", 0xD0450080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsocp.s", 0xD0450000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsqrt.p", 0xD0160080, 0xFFFF8080, "%zp, %yp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsqrt.q", 0xD0168080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsqrt.s", 0xD0160000, 0xFFFF8080, "%zs, %ys" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsqrt.t", 0xD0168000, 0xFFFF8080, "%zt, %yt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsrt1.q", 0xD0408080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsrt2.q", 0xD0418080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsrt3.q", 0xD0488080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsrt4.q", 0xD0498080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsub.p",	 0x60800080, 0xFF808080, "%zp, %yp, %xp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsub.q",	 0x60808080, 0xFF808080, "%zq, %yq, %xq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsub.s",	 0x60800000, 0xFF808080, "%zs, %ys, %xs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsub.t",	 0x60808000, 0xFF808080, "%zt, %yt, %xt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsync",	 0xFFFF0000, 0xFFFF0000, "%I" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vsync",	 0xFFFF0320, 0xFFFFFFFF, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vt4444.q",0xD0598080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vt5551.q",0xD05A8080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vt5650.q",0xD05B8080, 0xFFFF8080, "%zq, %yq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vtfm2.p", 0xF0800080, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vtfm3.t", 0xF1008000, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vtfm4.q", 0xF1808080, 0xFF808080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vus2i.p", 0xD03A0080, 0xFFFF8080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vus2i.s", 0xD03A0000, 0xFFFF8080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vwb.q",	 0xF8000002, 0xFC000002, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vwbn.s",	 0xD3000000, 0xFF008080, "" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vzero.p", 0xD0060080, 0xFFFFFF80, "%zp" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vzero.q", 0xD0068080, 0xFFFFFF80, "%zq" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vzero.s", 0xD0060000, 0xFFFFFF80, "%zs" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
	{ "vzero.t", 0xD0068000, 0xFFFFFF80, "%zt" , ADDR_TYPE_NONE, INSTR_TYPE_PSP },
#endif
	};

static const char *cop0_regs[32] = 
{
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	"BadVaddr", "Count", NULL, "Compare", "Status", "Cause", "EPC", "PrID",
	"Config", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, "EBase", NULL, NULL, "TagLo", "TagHi", "ErrorPC", NULL
};

static const char *dr_regs[16] = 
{
	"DRCNTL", "DEPC", "DDATA0", "DDATA1", "IBC", "DBC", NULL, NULL, 
	"IBA", "IBAM", NULL, NULL, "DBA", "DBAM", "DBD", "DBDM"
};

static int parse_regnum(const char *reg, int max)
{
	char *endp;
	int ret;

	ret = strtoul(reg, &endp, 10);
	if((endp != reg) && (*endp == 0) && (ret >= 0) && (ret < max))
	{
		return ret;
	}

	return -1;
}

static int parse_gpr(const char *reg, int max)
{
	int ret = -1;

	if(reg[0] == '$')
	{
		int i;
		for(i = 0; i < 32; i++)
		{
			if(strcasecmp(&reg[1], regName[i]) == 0)
			{
				ret = i;
				break;
			}
		}

		if(ret < 0)
		{
			ret = parse_regnum(&reg[1], 32);
		}
	}
	else if(reg[0] == 'r')
	{
		ret = parse_regnum(&reg[1], 32);
	}
	else
	{
		ret = -1;
	}

	return ret;
}

static struct Instruction *find_inst(const char *op, int argcount)
{
	size_t i;

	for(i = 0; i < (sizeof(macro)/sizeof(struct Instruction)); i++)
	{
		if(strcasecmp(macro[i].name, op) == 0)
		{
			if(macro[i].operands == argcount)
			{
				return &macro[i];
			}
		}
	}

	for(i = 0; i < (sizeof(g_inst)/sizeof(struct Instruction)); i++)
	{
		if(strcasecmp(g_inst[i].name, op) == 0)
		{
			if(g_inst[i].operands == argcount)
			{
				return &g_inst[i];
			}
		}
	}

	return NULL;
}

static char* strip_wsp(char *buf)
{
	int i;
	char *ret = buf;

	/* First strip wsp at end */

	for(i = strlen(buf)-1; i >= 0; i--)
	{
		if(isspace(buf[i]))
		{
			buf[i] = 0;
		}
		else
		{
			break;
		}
	}

	/* Now wsp at start */
	while(isspace(*ret))
	{
		ret++;
	}

	if(ret[0] == 0)
	{
		return NULL;
	}

	return ret;
}

static int set_imm(const char *imm, unsigned int *opcode, int sign)
{
	int val;
	char *endp;
	int ret = -1;

	val = strtol(imm, &endp, 0);
	if((endp != imm) && (*endp == 0))
	{
		if(sign)
		{
			if(val < SHRT_MIN)
			{
				fprintf(stderr, "Warning: Signed 16bit immediate underflow in %s\n", imm);
				val = SHRT_MIN;
			}
			   
			if(val > SHRT_MAX)
			{
				fprintf(stderr, "Warning: Signed 16bit immediate overflow in %s\n", imm);
				val = SHRT_MAX;
			}
		}
		else
		{
			if(val & 0xFFFF0000)
			{
				fprintf(stderr, "Warning: Unsigned 16bit immediate overflow in %s\n", imm);
			}
		}

		*opcode |= (val & 0xFFFF);
		ret = 0;
	}
	else
	{
		fprintf(stderr, "Invalid immediate %s\n", imm);
	}

	return ret;
}

static int set_gpr(const char *reg, unsigned int *opcode, char type)
{
	int regval;

	regval = parse_gpr(reg, 32);
	if(regval < 0)
	{
		fprintf(stderr, "Invalid GPR %s\n", reg);
		return -1;
	}

	switch(type)
	{
		case 'd': SETRD(*opcode, regval);
				  break;
		case 's': SETRS(*opcode, regval);
				  break;
		case 't': SETRT(*opcode, regval);
				  break;
		default:  fprintf(stderr, "Internal error, no matching register type\n");
				  return -1;
	};

	return 0;
}

static int set_cop0(const char *reg, unsigned int *opcode)
{
	int i;
	int regval = -1;

	if(reg[0] == '$')
	{
		regval = parse_regnum(&reg[1], 32);
	}
	else
	{
		for(i = 0; i < 32; i++)
		{
			if(cop0_regs[i])
			{
				if(strcasecmp(cop0_regs[i], reg) == 0)
				{
					regval = i;
					break;
				}
			}
		}
	}
	
	if(regval < 0)
	{
		fprintf(stderr, "Invalid cop0 register %s\n", reg);
		return -1;
	}

	SETRD(*opcode, regval);

	return 0;
}

static int set_dr(const char *reg, unsigned int *opcode)
{
	int i;
	int regval = -1;

	if(reg[0] == '$')
	{
		regval = parse_regnum(&reg[1], 16);
	}
	else
	{
		for(i = 0; i < 16; i++)
		{
			if(dr_regs[i])
			{
				if(strcasecmp(cop0_regs[i], reg) == 0)
				{
					regval = i;
					break;
				}
			}
		}
	}
	
	if(regval < 0)
	{
		fprintf(stderr, "Invalid debug register %s\n", reg);
		return -1;
	}

	SETRD(*opcode, regval);

	return 0;
}

static int set_sa(const char *sa, unsigned int *opcode)
{
	int val = -1;

	val = parse_regnum(sa, 32);
	
	if(val < 0)
	{
		fprintf(stderr, "Invalid SA value %s\n", sa);
		return -1;
	}

	SETSA(*opcode, val);

	return 0;
}

static int set_n(const char *n, unsigned int *opcode)
{
	int val = -1;

	val = parse_regnum(n, 33);
	
	if(val <= 0)
	{
		fprintf(stderr, "Invalid N value %s\n", n);
		return -1;
	}

	SETRD(*opcode, val-1);

	return 0;
}

static int set_k(const char *n, unsigned int *opcode)
{
	int val = -1;

	val = parse_regnum(n, 32);
	
	if(val < 0)
	{
		fprintf(stderr, "Invalid cache value %s\n", n);
		return -1;
	}

	SETRT(*opcode, val);

	return 0;
}

static int set_regofs(const char *ofs, unsigned int *opcode)
{
	char buffer[128];
	char *imm;
	char *reg;
	char *end;
	int  len;
	int  ret = -1;

	do
	{
		len = snprintf(buffer, sizeof(buffer), "%s", ofs);
		if((len < 0) || (len >= (int) sizeof(buffer)))
		{
			fprintf(stderr, "Could not copy offset\n");
			break;
		}

		imm = buffer;
		reg = strchr(buffer, '(');
		if(reg == NULL)
		{
			fprintf(stderr, "Invalid reg offset format %s (no starting bracket)\n", ofs);
			break;
		}

		*reg = 0;
		reg++;

		end = strchr(reg, ')');
		if(end == NULL)
		{
			fprintf(stderr, "Invalid reg offset format %s (no ending bracket)\n", ofs);
			break;
		}

		*end = 0;

		imm = strip_wsp(imm);
		if(imm == NULL)
		{
			fprintf(stderr, "Missing immediate value %s\n", ofs);
			break;
		}

		reg = strip_wsp(reg);
		if(reg == NULL)
		{
			fprintf(stderr, "Missing register value %s\n", ofs);
			break;
		}

		if(set_imm(imm, opcode, 1) < 0)
		{
			break;
		}

		if(set_gpr(reg, opcode, 's') < 0)
		{
			break;
		}

		ret = 0;
	}
	while(0);

	return ret;
}

static int set_pcofs(const char *jump, unsigned int PC, unsigned int *opcode)
{
	unsigned int addr;
	char *endp;
	int ret = -1;

	addr = strtoul(jump, &endp, 0);
	if((endp != jump) && (*endp == 0))
	{
		int ofs;

		ofs = addr - PC - 4;
		ofs /= 4;
		if(ofs < SHRT_MIN)
		{
			fprintf(stderr, "Warning: Signed 16bit immediate underflow in %s\n", jump);
			ofs = SHRT_MIN;
		}
		   
		if(ofs > SHRT_MAX)
		{
			fprintf(stderr, "Warning: Signed 16bit immediate overflow in %s\n", jump);
			ofs = SHRT_MAX;
		}

		*opcode |= (ofs & 0xFFFF);

		ret = 0;
	}
	else
	{
		fprintf(stderr, "Invalid branch address %s\n", jump);
	}

	return ret;
}

static int set_jump(const char *jump, unsigned int *opcode)
{
	unsigned int addr;
	char *endp;
	int ret = -1;

	addr = strtoul(jump, &endp, 0);
	if((endp != jump) && (*endp == 0))
	{
		addr = (addr & 0x0FFFFFFF) >> 2;
		*opcode |= addr;
		ret = 0;
	}
	else
	{
		fprintf(stderr, "Invalid jump address %s\n", jump);
	}

	return ret;
}

static int encode_args(int argcount, char **args, const char *fmt, unsigned int PC, unsigned int *opcode)
{
	int i = 0;
	//int vmmul = 0;
	int argpos = 0;
	int error = 0;

	while(fmt[i])
	{
		if(fmt[i] == '%')
		{
			if(argpos == argcount)
			{
				fprintf(stderr, "Not enough operands passed for instruction\n");
				goto end;
			}

			i++;
			switch(fmt[i])
			{
				case 'd': 
				case 't': 
				case 's': if(set_gpr(args[argpos], opcode, fmt[i]) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
				case 'i': if(set_imm(args[argpos], opcode, 1) < 0)
						  {
							  error = 1;
						  }
						  break;
				case 'I': if(set_imm(args[argpos], opcode, 0) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
				case 'o': if(set_regofs(args[argpos], opcode) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
				case 'O': if(set_pcofs(args[argpos], PC, opcode) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
				case 'j': if(set_jump(args[argpos], opcode) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
				case 'a': if(set_sa(args[argpos], opcode) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
				case '0': if(set_cop0(args[argpos], opcode) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
				case 'n': if(set_n(args[argpos], opcode) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
				case 'k': if(set_k(args[argpos], opcode) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
				case 'r': if(set_dr(args[argpos], opcode) < 0)
						  {
							  error = 1;
							  break;
						  }
						  break;
#if 0
				case '1': //output = print_cop1(RD(opcode), output);
						  break;
				case 'p': //*output++ = '$';
						  //output = print_int(RD(opcode), output);
						  break;
				case 'D': //output = print_fpureg(FD(opcode), output);
						  break;
				case 'T': //output = print_fpureg(FT(opcode), output);
						  break;
				case 'S': //output = print_fpureg(FS(opcode), output);
						  break;
				case 'x': //if(fmt[i+1]) { output = print_vfpureg(VT(opcode), fmt[i+1], output); i++; }
						  break;
				case 'y': //if(fmt[i+1]) { 
						//	  int reg = VS(opcode);
						//	  if(vmmul) { if(reg & 0x20) { reg &= 0x5F; } else { reg |= 0x20; } }
						//	  output = print_vfpureg(reg, fmt[i+1], output); i++; 
						//	  }
							  break;
				case 'z': //if(fmt[i+1]) { output = print_vfpureg(VD(opcode), fmt[i+1], output); i++; }
						  break;
				case 'v': break;
				case 'X': //if(fmt[i+1]) { output = print_vfpureg(VO(opcode), fmt[i+1], output); i++; }
						  break;
				case 'Z': //output = print_imm(VCC(opcode), output);
						  break;
				case 'c': //output = print_hex(CODE(opcode), output);
						  break;
				case 'C': //output = print_syscall(CODE(opcode), output);
						  break;
				case 'Y': //output = print_ofs(IMM(opcode) & ~3, RS(opcode), output, realregs);
						  break;
				case '?': //vmmul = 1;
						  break;
#endif
				case 0: goto end;
				default: fprintf(stderr, "Unsupported operand type %c\n", fmt[i]); 
						error = 1; 
						break;
			};

			if(error)
			{
				break;
			}
			argpos++;
		}
		i++;
	}

end:

	if(error)
	{
		return -1;
	}

	return 0;
}

static int assemble_op(const char *op, int argcount, char **args, unsigned int PC, unsigned int *inst)
{
	struct Instruction *set;

	set = find_inst(op, argcount);
	if(set == NULL)
	{
		fprintf(stderr, "Could not find matching instruction for %s (operands %d)\n", op, argcount);
		return -1;
	}

	*inst = set->opcode;

	return encode_args(argcount, args, set->fmt, PC, inst);
}

int asmAssemble(const char *str, unsigned int PC, unsigned int *inst)
{
	char buf[1024];
	int ret;
	char *op = NULL;
	char *arghead = NULL;
	int argcount = 0;
	char *args[32];

	ret = snprintf(buf, sizeof(buf), "%s", str);
	if((ret < 0) || (ret >= (int) sizeof(buf)))
	{
		fprintf(stderr, "Error copying assembler string\n");
		return -1;
	}

	/* Split up line, start with the mnemonic, then arguments */
	op = strtok(buf, " \t\r\n");
	if(op == NULL)
	{
		fprintf(stderr, "Could not find instruction\n");
		return -1;
	}
	arghead = strtok(NULL, "");
	if(arghead)
	{
		char *tok;

		/* Remove leading whitespace */
		while(isspace(*arghead))
		{
			arghead++;
		}

		argcount = 0;
		tok = strtok(arghead, ",");
		while((tok) && (argcount < 32))
		{
			tok = strip_wsp(tok);
			if(tok == NULL)
			{
				fprintf(stderr, "Invalid empty argument\n");
				return -1;
			}
			args[argcount++] = tok;
			tok = strtok(NULL, ",");
		}
	}

	return assemble_op(op, argcount, args, PC, inst);
}
