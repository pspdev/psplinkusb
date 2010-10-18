/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * exception.c - Exception handler for psplink.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/exception.c $
 * $Id: exception.c 2316 2007-09-17 18:13:33Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <pspexception.h>
#include <stdio.h>
#include <string.h>
#include "exception.h"
#include "util.h"
#include "psplink.h"
#include "debug.h"

struct PsplinkContext *g_currex = NULL;
struct PsplinkContext *g_list = NULL;
struct PsplinkContext g_psplinkContext[PSPLINK_MAX_CONTEXT] __attribute__((aligned(16)));
extern struct GlobalContext g_context;

void psplinkDefaultExHandler(void);
void psplinkDebugExHandler(void);

#define MAT_NUM(x) ((x) / 16)
#define COL_NUM(x) (((x) / 4) & 3)
#define ROW_NUM(x) ((x) & 3)

#define FPU_EXCEPTION 15

/* Mnemonic register names */
const char *regName[32] =
{
    "zr", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", 
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

/* Taken from the ps2, might not be 100% correct */
static const char *codeTxt[32] = 
{
    "Interrupt", "TLB modification", "TLB load/inst fetch", "TLB store",
    "Address load/inst fetch", "Address store", "Bus error (instr)", 
    "Bus error (data)", "Syscall", "Breakpoint", "Reserved instruction", 
    "Coprocessor unusable", "Arithmetic overflow", "Unknown 13", "Unknown 14", 
	"FPU Exception", "Unknown 16", "Unknown 17", "Unknown 18",
	"Unknown 20", "Unknown 21", "Unknown 22", "Unknown 23", 
	"Unknown 24", "Unknown 25", "Unknown 26", "Unknown 27", 
	"Unknown 28", "Unknown 29", "Unknown 30", "Unknown 31"
};

const char codeFpu[6] = { 'I', 'U', 'O', 'Z', 'V', 'E' };
const char codeDebug[7] = { 'X', 'S', 'B', '?', '?', 'I', 'D' };

/* Get a pointer to a register based on its name */
unsigned int *exceptionGetReg(const char *reg)
{
	static unsigned int modaddr = 0;

	if(g_currex == NULL)
	{
		return NULL;
	}

	if(strcmp(reg, "epc") == 0)
	{
		return &g_currex->regs.epc;
	}
	else if(strcmp(reg, "fsr") == 0)
	{
		return &g_currex->regs.fsr;
	}
	else if(strcmp(reg, "mod") == 0)
	{
		SceModule *pMod;
		SceKernelModuleInfo mod;

		modaddr = 0;
		pMod = sceKernelFindModuleByAddress(g_currex->regs.epc);
		if(pMod)
		{
			memset(&mod, 0, sizeof(mod));
			mod.size = sizeof(mod);
			enable_kprintf(0);
			if(!g_QueryModuleInfo(pMod->modid, &mod))
			{
				modaddr = mod.text_addr;
			}
			enable_kprintf(1);
		}

		return &modaddr;
	}
	else 
	{
		int reg_loop;

		for(reg_loop = 0; reg_loop < 32; reg_loop++)
		{
			if(strcmp(regName[reg_loop], reg) == 0)
			{
				return &g_currex->regs.r[reg_loop];
			}
		}
	}

	return NULL;
}

/* Print the cpu registers, pointer should contain a dummy entry
 * for zero as it is relatively addressed */
void exceptionPrintCPURegs(unsigned int *pRegs)
{
	int i;

	SHELL_PRINT("%s:0x00000000 %s:0x%08X %s:0x%08X %s:0x%08X\n", regName[0], 
			regName[1], pRegs[1], regName[2], 
			pRegs[2], regName[3], pRegs[3]);
	for(i = 4; i < 32; i+=4)
	{
		SHELL_PRINT("%s:0x%08X %s:0x%08X %s:0x%08X %s:0x%08X\n", regName[i], pRegs[i],
				regName[i+1], pRegs[i+1], regName[i+2], 
				pRegs[i+2], regName[i+3], pRegs[i+3]);
	}
}

static const char *exception_cause(struct PsplinkContext *pCtx)
{
	static char excause[40];

	if(pCtx->regs.type == PSPLINK_EXTYPE_NORMAL)
	{
		if(((pCtx->regs.cause >> 2) & 31) == FPU_EXCEPTION)
		{
			int i;
			unsigned int fpu;
			char *end;

			strcpy(excause, codeTxt[(pCtx->regs.cause >> 2) & 31]);
			strcat(excause, " (");
			end = excause + strlen(excause);

			fpu = pCtx->regs.fsr >> 12;
			for(i = 0; i < 6; i++)
			{
				if((fpu >> i) & 1)
				{
					*end++ = codeFpu[i];
				}
			}
			*end++ = ')';
			*end = 0;
		}
		else
		{
			strcpy(excause, codeTxt[(pCtx->regs.cause >> 2) & 31]);
		}
	}
	else
	{
		int i;
		char *end;

		strcpy(excause, "DEBUG");
		strcat(excause, " (");
		end = excause + strlen(excause);
		for(i = 0; i < 7; i++)
		{
			if(pCtx->drcntl & (1 << (i + 6)))
			{
				*end++ = codeDebug[i];
			}
		}
		*end++ = ')';
		*end = 0;
	}

	return excause;
}

/* Print the current exception */
void exceptionPrint(struct PsplinkContext *ctx)
{
	SceModule *pMod;
	SceKernelModuleInfo mod;
	SceKernelThreadInfo thread;
	unsigned int addr;

	if(ctx == NULL)
	{
		ctx = g_currex;
	}

	if(ctx)
	{
		SHELL_PRINT("Exception - %s\n", exception_cause(ctx));
		SHELL_PRINT("Thread ID - 0x%08X\n", ctx->thid);

		memset(&thread, 0, sizeof(thread));
		thread.size = sizeof(thread);
		if(!sceKernelReferThreadStatus(ctx->thid, &thread))
		{
			SHELL_PRINT("Th Name   - %s\n", thread.name);
		}

		if((ctx->regs.epc < 0x88000000) || (ctx->regs.epc > 0x88400000))
		{
			addr = ctx->regs.epc & 0x7FFFFFFF;
		}
		else
		{
			addr = ctx->regs.epc;
		}

		pMod = sceKernelFindModuleByAddress(addr);
		if(pMod)
		{
			SHELL_PRINT("Module ID - 0x%08X\n", pMod->modid);
			memset(&mod, 0, sizeof(mod));
			mod.size = sizeof(mod);
			enable_kprintf(0);
			if(!g_QueryModuleInfo(pMod->modid, &mod))
			{
				SHELL_PRINT("Mod Name  - %s\n", mod.name);
			}
			enable_kprintf(1);
		}

		SHELL_PRINT("EPC       - 0x%08X\n", ctx->regs.epc);
		if(g_currex->regs.type == PSPLINK_EXTYPE_NORMAL)
		{
			SHELL_PRINT("Cause     - 0x%08X\n", ctx->regs.cause);
			SHELL_PRINT("BadVAddr  - 0x%08X\n", ctx->regs.badvaddr);
		}
		else
		{
			SHELL_PRINT("DRCNTL    - 0x%08X\n", ctx->drcntl);
		}

		SHELL_PRINT("Status    - 0x%08X\n", ctx->regs.status);
		exceptionPrintCPURegs(ctx->regs.r);
	}
	else
	{
		SHELL_PRINT("No exception occurred\n");
	}
}

void exceptionList(void)
{
	int i;

	if(g_list)
	{
		for(i = 0; i < PSPLINK_MAX_CONTEXT; i++)
		{
			if(g_list[i].valid)
			{
				SHELL_PRINT("Exception %-2d: EPC 0x%08X, Cause %s\n", i, g_list[i].regs.epc, 
						exception_cause(&g_list[i]));
			}
		}
	}
	else
	{
		SHELL_PRINT("No exception handler registered\n");
	}
}

void exceptionPrintFPURegs(float *pFpu, unsigned int fsr, unsigned int fir)
{
	int i;

	pspSdkDisableFPUExceptions();

	for(i = 0; i < 32; i+=2)
	{
		char left[64], right[64];

		f_cvt(&pFpu[i], left, sizeof(left), 6, MODE_GENERIC);
		f_cvt(&pFpu[i+1], right, sizeof(right), 6, MODE_GENERIC);
		SHELL_PRINT("fpr%02d: %-20s - fpr%02d: %-20s\n", i, left, i+1, right);
	}
	SHELL_PRINT("fsr: %08X   - fir %08X\n", fsr, fir);
}

void exceptionFpuPrint(struct PsplinkContext *ctx)
{
	if(ctx == NULL)
	{
		ctx = g_currex;
	}

	if(ctx)
	{
		if(ctx->regs.status & 0x20000000)
		{
			exceptionPrintFPURegs(ctx->regs.fpr, ctx->regs.fsr,
					ctx->regs.fir);
		}
		else
		{
			SHELL_PRINT("FPU not enabled in context\n");
		}
	}
	else
	{
		SHELL_PRINT("No exception occurred\n");
	}
}

static void print_vfpu_row(char type, int m, int c, int r, float x, float y, float z, float w)
{
	char xs[64], ys[64], zs[64], ws[64];

	f_cvt(&x, xs, sizeof(xs), 6, MODE_GENERIC);
	f_cvt(&y, ys, sizeof(ys), 6, MODE_GENERIC);
	f_cvt(&z, zs, sizeof(zs), 6, MODE_GENERIC);
	f_cvt(&w, ws, sizeof(ws), 6, MODE_GENERIC);
	if(type != 0)
	{
		SHELL_PRINT("%c%d%d%d: { %14s, %14s, %14s, %14s }\n", 
				type, m, c, r, xs, ys, zs, ws );
	}
	else
	{
		SHELL_PRINT("      { %14s, %14s, %14s, %14s }\n", 
				xs, ys, zs, ws );
	}
}

void exceptionPrintVFPURegs(float *pFpu, int mode)
{
	int i, j;

	pspSdkDisableFPUExceptions();

	if(mode == VFPU_PRINT_SINGLE)
	{
		for(i = 0; i < 128; i+=2)
		{
			char left[64], right[64];

			f_cvt(&pFpu[i], left, sizeof(left), 6, MODE_GENERIC);
			f_cvt(&pFpu[i+1], right, sizeof(right), 6, MODE_GENERIC);
			SHELL_PRINT("S%d%d%d: %-20s - S%d%d%d: %-20s\n", 
					MAT_NUM(i), COL_NUM(i), ROW_NUM(i), left, 
					MAT_NUM(i+1), COL_NUM(i+1), ROW_NUM(i+1),  right);
		}
	}
	else if(mode == VFPU_PRINT_COL)
	{
		for(i = 0; i < 128; i+=4)
		{
			print_vfpu_row('C', MAT_NUM(i), COL_NUM(i), 0, pFpu[i], pFpu[i+1], pFpu[i+2], pFpu[i+3]);
		}
	}
	else if(mode == VFPU_PRINT_ROW)
	{
		for(i = 0; i < 128; i+=16)
		{
			for(j = 0; j < 4; j++)
			{
				print_vfpu_row('R', MAT_NUM(i), 0, j, pFpu[i+j], pFpu[i+j+4], pFpu[i+j+8], pFpu[i+j+12]);
			}
		}
	}
	else if(mode == VFPU_PRINT_MATRIX)
	{
		for(i = 0; i < 128; i+=16)
		{
			print_vfpu_row('M', MAT_NUM(i), 0, 0, pFpu[i], pFpu[i+4], pFpu[i+8], pFpu[i+12]);
			for(j = 1; j < 4; j++)
			{
				print_vfpu_row(0, 0, 0, 0, pFpu[i+j], pFpu[i+j+4], pFpu[i+j+8], pFpu[i+j+12]);
			}
		}
	}
	else if(mode == VFPU_PRINT_TRANS)
	{
		for(i = 0; i < 128; i+=16)
		{
			print_vfpu_row('E', MAT_NUM(i), 0, 0, pFpu[i], pFpu[i+1], pFpu[i+2], pFpu[i+3]);
			for(j = 1; j < 4; j++)
			{
				print_vfpu_row(0, 0, 0, 0, pFpu[i+(j*4)], pFpu[i+(j*4)+1], pFpu[i+(j*4)+2], pFpu[i+(j*4)+3]);
			}
		}
	}
}

void exceptionVfpuPrint(struct PsplinkContext *ctx, int mode)
{
	if(ctx == NULL)
	{
		ctx = g_currex;
	}

	if(ctx)
	{
		if(ctx->regs.status & 0x40000000)
		{
			exceptionPrintVFPURegs(ctx->regs.vfpu, mode);
		}
		else
		{
			SHELL_PRINT("VFPU not enabled in context\n");
		}
	}
	else
	{
		SHELL_PRINT("No exception occurred\n");
	}
}

struct PsplinkContext* psplinkTrap(struct PsplinkContext *ctx)
{
	int skip = 0;

	/* Only set if we are not already messing with a context */
	if(g_currex == NULL)
	{
		g_currex = ctx;
	}

	if(sceKernelIsIntrContext())
	{
		Kprintf("Can't debug interrupt contexts (ATM)\n");
		for(;;) { ; }
	}
	else
	{
		ctx->thid = sceKernelGetThreadId();
	}
	/* Handle debug case, check whether we are in an interrupt context etc. */
	/* Also disable the current breakpoint if we hit it */

	debugClearException(ctx);

	/* Enable interrupts now */
	asm __volatile__ ( 
			"mfc0	$v0, $12\n"
			"ori	$v0, $v0, 1\n"
			"mtc0	$v0, $12\n"
			"nop\n"
			);

	debugHandleException(ctx);

	if(ctx->cont == PSP_EXCEPTION_EXIT)
	{
		sceKernelExitDeleteThread(0);
	}

	/* Disable interrupts now */
	asm __volatile__ ( 
			"mtic $0, $0\n"
			"nop\n"
			"nop\n"
			);

	switch(ctx->cont)
	{
		case PSP_EXCEPTION_SKIP: skip = 1;
		case PSP_EXCEPTION_STEP: debugStep(ctx, skip);
								 break;
		default: break;
	};

	if(g_currex == ctx)
	{
		g_currex = NULL;
	}

	return ctx;
}

void exceptionResume(struct PsplinkContext *ctx, int cont)
{
	if(ctx == NULL)
	{
		ctx = g_currex;
	}

	if(ctx)
	{
		ctx->cont = cont;
		sceKernelWakeupThread(ctx->thid);
	}
}

void exceptionSetCtx(int ex)
{
	if((ex >= 0) && (ex < PSPLINK_MAX_CONTEXT))
	{
		if((g_list) && (g_list[ex].valid))
		{
			g_currex = &g_list[ex];
		}
	}
}

void exceptionInit(void)
{
	int i;
	memset(g_psplinkContext, 0, sizeof(g_psplinkContext));

	for(i = 0; i < (PSPLINK_MAX_CONTEXT-1); i++)
	{
		g_psplinkContext[i].pNext = &g_psplinkContext[i+1];
	}

	g_list = g_psplinkContext;
	sceKernelRegisterDefaultExceptionHandler((void*) psplinkDefaultExHandler);
}
