/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * debug.c - Debugger code for psplink.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/debug.c $
 * $Id: debug.c 2301 2007-08-26 13:48:05Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsysmem_kernel.h>
#include <psputilsforkernel.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "exception.h"
#include "util.h"
#include "psplink.h"
#include "shellcmd.h"
#include "debug.h"
#include "decodeaddr.h"
#include "thctx.h"

#define SW_BREAK_INST	0x0000000d

extern struct GlobalContext g_context;

struct HwDebugEnv
{
	unsigned int flags;
	unsigned int DRCNTL;
	unsigned int IBC;
	unsigned int DBC;
	unsigned int IBA;
	unsigned int IBAM;
	unsigned int DBA;
	unsigned int DBAM;
	unsigned int DBD;
	unsigned int DBDM;
} psplinkHwContext;

int debugGetEnv(struct HwDebugEnv *env);

typedef struct _DebugEvent
{
	SceKernelMsgPacket header;
	struct PsplinkContext *ctx;
} DebugEvent;


static struct Breakpoint g_bps[DEBUG_MAX_BPS];
static struct _DebugEventHandler* g_eventhead;
extern const char *regName[32];

static void debug_set_hwbreak(unsigned int addr);

int debugRegisterEventHandler(DebugEventHandler *handler)
{
	int intc;

	if(!handler || (handler->size != sizeof(DebugEventHandler)) || (handler->mbox < 0))
	{
		return -1;
	}

	if(handler->membase > handler->memtop)
	{
		return -1;
	}

	intc = pspSdkDisableInterrupts();
	handler->pNext = g_eventhead;
	g_eventhead = handler;
	pspSdkEnableInterrupts(intc);

	return 0;
}

int debugUnregisterEventHandler(DebugEventHandler *handler)
{
	DebugEventHandler *ev;
	int intc;

	intc = pspSdkDisableInterrupts();
	if(g_eventhead == handler)
	{
		g_eventhead = g_eventhead->pNext;
	}
	else
	{
		ev = g_eventhead;
		while(ev && (ev->pNext != handler))
		{
			ev = ev->pNext;
		}
		if(ev)
		{
			ev->pNext = ev->pNext->pNext;
		}
	}
	pspSdkEnableInterrupts(intc);

	return 0;
}

int debugWaitDebugEvent(DebugEventHandler *handler, struct PsplinkContext **ctx, SceUInt *timeout)
{
	int ret;
	DebugEvent *ev;
	void *p;

	if(!handler || !ctx)
	{
		return -1;
	}

	ret = sceKernelReceiveMbx(handler->mbox, &p, timeout);
	if(ret == 0)
	{
		ev = (DebugEvent*) p;
		*ctx = ev->ctx;
	}

	return ret;
}

/* Define some opcode stuff for the stepping function */
#define BEQ_OPCODE		0x4
#define BEQL_OPCODE		0x14
#define BGTZ_OPCODE 	0x7
#define BGTZL_OPCODE	0x17
#define BLEZ_OPCODE		0x6
#define BLEZL_OPCODE	0x16
#define BNE_OPCODE		0x5
#define BNEL_OPCODE		0x15

/* Reg Imm */
#define REGIMM_OPCODE 	0x1
#define BGEZ_OPCODE		0x1
#define BGEZAL_OPCODE	0x11
#define BGEZALL_OPCODE	0x13
#define BGEZL_OPCODE	0x3
#define BLTZ_OPCODE		0
#define BLTZAL_OPCODE	0x10
#define BLTZALL_OPCODE	0x12
#define BLTZL_OPCODE	0x2

#define J_OPCODE		0x2
#define JAL_OPCODE		0x3

/* Special opcode */
#define SPECIAL_OPCODE	0
#define JALR_OPCODE		0x9
#define JR_OPCODE		0x8
#define SYSCALL_OPCODE  0xc

/* Cop Branches (all the same) */
#define COP0_OPCODE		0x10
#define COP1_OPCODE		0x11
#define COP2_OPCODE		0x12
#define BCXF_OPCODE		0x100
#define BCXFL_OPCODE	0x102
#define BCXT_OPCODE		0x101
#define BCXTL_OPCODE	0x103

/* Generic step command , if skip then will try to skip over jals */
static void step_generic(struct PsplinkContext *ctx, int skip)
{
	unsigned int opcode;
	unsigned int epc;
	unsigned int targetpc;
	int branch = 0;
	int cond   = 0;
	int link   = 0;

	epc = ctx->regs.epc;
	targetpc = epc + 4;

	opcode = _lw(epc);

	switch(opcode >> 26)
	{
		case BEQ_OPCODE:
		case BEQL_OPCODE:
		case BGTZ_OPCODE:
		case BGTZL_OPCODE:
		case BLEZ_OPCODE:
		case BLEZL_OPCODE: 
		case BNE_OPCODE:
		case BNEL_OPCODE:
						    {
							   short ofs;

							   ofs = (short) (opcode & 0xffff);
							   cond = 1;
							   branch = 1;
							   targetpc += ofs * 4;
						   }
						   break;
		case REGIMM_OPCODE: {
								switch((opcode >> 16) & 0x1f)
								{
									case BGEZ_OPCODE:
									case BGEZAL_OPCODE:
									case BGEZALL_OPCODE:	
									case BGEZL_OPCODE:
									case BLTZ_OPCODE:
									case BLTZAL_OPCODE:
									case BLTZALL_OPCODE:
									case BLTZL_OPCODE: {
														   short ofs;

														   ofs = (short) (opcode & 0xffff);
														   cond = 1;
														   branch = 1;
														   targetpc += ofs * 4;
													   }
													   break;
								}
						    }
							break;
		case JAL_OPCODE:	link = 1;
		case J_OPCODE: {
							 unsigned int ofs;
							 
							 ofs = opcode & 0x3ffffff;
							 targetpc = (ofs << 2) | (targetpc & 0xf0000000);
							 branch = 1;
							 cond = 0;
						 }
						 break;
		case SPECIAL_OPCODE:
						 {
							 switch(opcode & 0x3f)
							 {
								 case JALR_OPCODE: link = 1;
								 case JR_OPCODE:
												 {
													 unsigned int rs;

													 rs = (opcode >> 21) & 0x1f;
													 targetpc = ctx->regs.r[rs];
													 branch = 1;
													 cond = 0;
												 }
												 break;
							 };
						 }
						 break;
		case COP0_OPCODE:
		case COP1_OPCODE:
		case COP2_OPCODE:
						 {
							 switch((opcode >> 16) & 0x3ff)
							 {
								 case BCXF_OPCODE:
								 case BCXFL_OPCODE:
								 case BCXT_OPCODE:
								 case BCXTL_OPCODE:
									 				{
														short ofs;

														ofs = (short) (opcode & 0xffff);
														cond = 1;
														branch = 1;
														targetpc += ofs * 4;
													}
													break;
							 };
						 }
						 break;
	};

	if(link && skip)
	{
		debugSetBP(epc+8, DEBUG_BP_ONESHOT | DEBUG_BP_STEP, ctx->thid);
	}
	else if(branch)
	{
		struct Breakpoint *bp1;
		struct Breakpoint *bp2;

		bp1 = debugSetBP(targetpc, DEBUG_BP_ONESHOT | DEBUG_BP_STEP, ctx->thid);
			
		if((cond) && (targetpc != (epc + 8)))
		{
			bp2 = debugSetBP(epc+8, DEBUG_BP_ONESHOT | DEBUG_BP_STEP, ctx->thid);
			if(bp1)
			{
				bp1->step = bp2;
			}
			if(bp2)
			{
				bp2->step = bp1;
			}
		}
	}
	else
	{
		debugSetBP(targetpc, DEBUG_BP_ONESHOT | DEBUG_BP_STEP, ctx->thid);
	}
}

void debugStep(struct PsplinkContext *ctx, int skip)
{
	step_generic(ctx, skip);
	sceKernelDcacheWritebackInvalidateAll();
	sceKernelIcacheInvalidateAll();
}

static struct Breakpoint *find_bp(unsigned int address)
{
	int i;

	/* Mask out top nibble so we match whether we end up in kmem,
	 * user mem or cached mem */
	address &= 0x0FFFFFFF;
	for(i = 0; i < DEBUG_MAX_BPS; i++)
	{
		if((g_bps[i].flags & DEBUG_BP_ACTIVE) && ((g_bps[i].addr & 0x0FFFFFFF) == address))
		{
			return &g_bps[i];
		}
	}

	return NULL;
}

static struct Breakpoint *find_hwbp(void)
{
	int i;

	for(i = 0; i < DEBUG_MAX_BPS; i++)
	{
		if((g_bps[i].flags & DEBUG_BP_ACTIVE) && (g_bps[i].flags & DEBUG_BP_HARDWARE))
		{
			return &g_bps[i];
		}
	}

	return NULL;
}

static struct Breakpoint *find_freebp(void)
{
	int i;
	for(i = 0; i < DEBUG_MAX_BPS; i++)
	{
		if((g_bps[i].flags & DEBUG_BP_ACTIVE) == 0)
		{
			return &g_bps[i];
		}
	}

	return NULL;
}

struct Breakpoint* debugSetBP(unsigned int address, unsigned int flags, SceUID thid)
{
	if(find_bp(address) == NULL)
	{
		struct Breakpoint *pBp = NULL;

		if(flags & DEBUG_BP_HARDWARE)
		{
			/* Check for existing hardware breakpoint */
			if(find_hwbp())
			{
				return NULL;
			}
		}

		pBp = find_freebp();
		if(pBp != NULL)
		{
			memset(pBp, 0, sizeof(struct Breakpoint));
			pBp->inst = _lw(address);

			if(flags & DEBUG_BP_HARDWARE)
			{
				debug_set_hwbreak(address);
			}
			else
			{
				_sw(SW_BREAK_INST, address);
			}

			pBp->addr = address;
			pBp->flags = DEBUG_BP_ACTIVE | flags;
			pBp->thid = thid;
			sceKernelDcacheWritebackInvalidateAll();
			sceKernelIcacheInvalidateAll();

			return pBp;
		}
		else
		{
			SHELL_PRINT("Error, could not find a free breakpoint\n");
		}
	}

	return NULL;
}

struct Breakpoint* debugFindBPByIndex(int i)
{
	if((i >= 0) && (i < DEBUG_MAX_BPS))
	{
		if(g_bps[i].flags & DEBUG_BP_ACTIVE)
		{
			return &g_bps[i];
		}
	}

	return NULL;
}

int debugDeleteBP(unsigned int addr)
{
	int ret = 0;
	int intc;
	struct Breakpoint *pBp;

	intc = pspSdkDisableInterrupts();
	pBp = find_bp(addr);
	if(pBp)
	{
		if((pBp->flags & DEBUG_BP_HARDWARE) == 0)
		{
			_sw(pBp->inst, pBp->addr);
		}
		else
		{
			debug_set_hwbreak(0);
		}

		pBp->flags = 0;
		sceKernelDcacheWritebackInvalidateAll();
		sceKernelIcacheInvalidateAll();
		ret = 1;
	}
	pspSdkEnableInterrupts(intc);

	return ret;
}

int debugDisableBP(unsigned int addr)
{
	int ret = 0;
	int intc;
	struct Breakpoint *pBp;

	intc = pspSdkDisableInterrupts();
	pBp = find_bp(addr);
	if(pBp)
	{
		if((pBp->flags & DEBUG_BP_HARDWARE) == 0)
		{
			_sw(pBp->inst, pBp->addr);
		}
		else
		{
			debug_set_hwbreak(0);
		}

		pBp->flags |= DEBUG_BP_DISABLED;
		sceKernelDcacheWritebackInvalidateAll();
		sceKernelIcacheInvalidateAll();
		ret = 1;
	}
	pspSdkEnableInterrupts(intc);

	return ret;
}

int debugEnableBP(unsigned int addr)
{
	int ret = 0;
	int intc;
	struct Breakpoint *pBp;

	intc = pspSdkDisableInterrupts();
	pBp = find_bp(addr);
	if(pBp)
	{
		if((pBp->flags & DEBUG_BP_HARDWARE) == 0)
		{
			_sw(SW_BREAK_INST, pBp->addr);
		}
		else
		{
			debug_set_hwbreak(pBp->addr);
		}

		pBp->flags &= ~(DEBUG_BP_DISABLED | DEBUG_BP_NEXT_REENABLE);
		sceKernelDcacheWritebackInvalidateAll();
		sceKernelIcacheInvalidateAll();
		ret = 1;
	}
	pspSdkEnableInterrupts(intc);

	return ret;
}

void debugPrintBPs(void)
{
	int i;

	SHELL_PRINT("Breakpoint List:\n");
	for(i = 0; i < DEBUG_MAX_BPS; i++)
	{
		if(g_bps[i].flags & DEBUG_BP_ACTIVE)
		{
			SHELL_PRINT("%-2d: Addr:0x%08X Inst:0x%08X Flags:%c%c%c\n", i, g_bps[i].addr, g_bps[i].inst, 
					g_bps[i].flags & DEBUG_BP_ONESHOT ? 'O' : '-', g_bps[i].flags & DEBUG_BP_HARDWARE ? 'H' : '-',
					g_bps[i].flags & DEBUG_BP_DISABLED ? 'D' : '-');
		}
	}
}

void debugClearException(struct PsplinkContext *ctx)
{
	unsigned int address;
	struct Breakpoint *pBp;

	if(ctx->regs.type == PSPLINK_EXTYPE_DEBUG)
	{
		ctx->drcntl = psplinkHwContext.DRCNTL;
	}

	address = ctx->regs.epc;

	/* Adjust for delay slot */
	if(ctx->regs.cause & 0x80000000)
	{
		address += 4;
	}
	ctx->error = 1;

	pBp = find_bp(address);
	if((pBp != NULL) && ((pBp->flags & DEBUG_BP_DISABLED) == 0))
	{
		if(pBp->flags & DEBUG_BP_ONESHOT)
		{
			struct Breakpoint *step;
	
			step = pBp->step;
			debugDeleteBP(pBp->addr);

			if(step)
			{
				debugDeleteBP(step->addr);
			}
		}

		ctx->error = 0;
		sceKernelDcacheWritebackInvalidateAll();
		sceKernelIcacheInvalidateAll();
	}
}

int debugCheckThread(struct PsplinkContext *ctx, unsigned int *addr)
{
	SceKernelThreadInfo thread;

	memset(&thread, 0, sizeof(thread));
	thread.size = sizeof(thread);
	if(!sceKernelReferThreadStatus(ctx->thid, &thread))
	{
		*addr = (unsigned int) thread.entry;

		if(strcmp(thread.name, "PspLink") == 0)
		{
			/* Indicate we are in the psplink parse thread */
			return 1;
		}
	}

	return 0;
}

int debugBreakThread(SceUID uid)
{
	int intc;
	unsigned int addr;
	int ret = -1;

	intc = pspSdkDisableInterrupts();
	addr = thGetCurrentEPC(uid);
	if(addr)
	{
		if(debugSetBP(addr, DEBUG_BP_ONESHOT, uid))
		{
			ret = 0;
		}
	}
	pspSdkEnableInterrupts(intc);

	return ret;
}

int debugHandleException(struct PsplinkContext *ctx)
{
	int ret = 0;
	unsigned int addr;
	int initex;
	struct _DebugEventHandler *ev;

	/* Should have an indication that this was a step */
	/* This is where we need to loop around our debug handlers and post messages */
	initex = debugCheckThread(ctx, &addr);

	/* Even if the exception happened in the psplink thread then at least see if any debuggers
	 * are interested in us (so we _could_ debug outselves at some point) */
	ctx->cont = PSP_EXCEPTION_NOT_HANDLED;

	ev = g_eventhead;
	while(ev)
	{
		DebugEvent post;
		
		if((addr >= ev->membase) && (addr <= ev->memtop))
		{
			memset(&post, 0, sizeof(post));
			post.ctx = ctx;
			if(sceKernelSendMbx(ev->mbox, &post) == 0)
			{
				sceKernelSleepThread();
				if(ctx->cont != PSP_EXCEPTION_NOT_HANDLED)
				{
					break;
				}
			}
		}
		ev = ev->pNext;
	}

	if(ctx->cont == PSP_EXCEPTION_NOT_HANDLED)
	{
		if(ctx->error)
		{
			exceptionPrint(ctx);
		}

		if(((ctx->regs.epc & 3) == 0) && (memValidate(ctx->regs.epc, MEM_ATTRIB_READ | MEM_ATTRIB_WORD)))
		{
			SHELL_PRINT_CMD(SHELL_CMD_DISASM, "0x%08X:0x%08X", ctx->regs.epc, _lw(ctx->regs.epc));
		}

		/* If this was not our parse thread */
		if(!initex)
		{
			sceKernelSleepThread();
		}
		else
		{
			/* Setup return context for our thread */
			ctx->regs.epc = (unsigned int) longjmp;
			ctx->regs.r[4] = (unsigned int) g_context.parseenv;
			ctx->regs.r[5] = 1;
			ctx->cont = PSP_EXCEPTION_CONTINUE;
		}
	}

	return ret;
}

void debugEnableHW(void)
{
	asm( 
			"mfc0 $t0, $12\n"
			"lui  $t1, 8\n"
			"or   $t1, $t1, $t0\n"
			"mtc0 $t1, $12\n"
	   );
}

void debugDisableHW(void)
{
	asm( 
			"mfc0 $t0, $12\n"
			"lui  $t1, 8\n"
			"not  $t1, $t1\n"
			"and  $t1, $t1, $t0\n"
			"mtc0 $t1, $12\n"
	   );
}

int debugHWEnabled(void)
{
	int ret = 0;
	asm( 
			"mfc0 $t0, $12\n"
			"lui  $t1, 8\n"
			"and  %0, $t1, $t0\n" 
			: "=r"(ret)
	   );

	return ret;
}

static void debug_get_env(void)
{
	psplinkHwContext.flags = 1;
	asm(
		"dbreak\n"
		"nop\n"
	   );
}

static void debug_set_env(void)
{
	psplinkHwContext.flags = 2;
	asm(
		"dbreak\n"
		"nop\n"
	   );
}

extern unsigned int psplinkHwDebugTrap;
extern unsigned int psplinkHwDebugTrapEnd;
void psplinkHwDebugMain(void);

void debugHwInit(void)
{
	unsigned int* p = &psplinkHwDebugTrap;
	unsigned int base = 0xbfc01000;
	while(p < &psplinkHwDebugTrapEnd)
	{
		_sw(*p, base);
		base += 4;
		p++;
	}
	sceKernelDcacheWritebackInvalidateAll();
	sceKernelIcacheInvalidateAll();
	asm __volatile__ (
			"ctc0	%0, $10\n"
			: : "r"(psplinkHwDebugMain)
			);
	debugEnableHW();
	memset(&psplinkHwContext, 0, sizeof(psplinkHwContext));
	debug_set_env();
}

static void debug_set_hwbreak(unsigned int addr)
{
	struct HwDebugEnv *pEnv = &psplinkHwContext;
	
	debug_get_env();

	if(addr)
	{
		pEnv->IBC = 0x12;
		pEnv->IBA = addr;
		pEnv->IBAM = 0;
	}
	else
	{
		pEnv->IBC = 0x10;
		pEnv->IBA = 0;
		pEnv->IBAM = 0;
	}
	
	debug_set_env();
}

/*

void debugSetHWRegs(int argc, char **argv)
{
	int i;
	struct HwDebugEnv *pEnv;

	pEnv = debug_get_env();
	if(pEnv == NULL)
	{
		return;
	}

	for(i = 0; i < argc; i++)
	{
		char *pEquals;

		pEquals = strchr(argv[i], '=');
		if(pEquals)
		{
			unsigned int val;
			*pEquals = 0;
			pEquals++;
			if(memDecode(pEquals, &val))
			{
				if(strcmp(argv[i], "IBC") == 0)
				{
					pEnv->IBC = val;
				}
				else if(strcmp(argv[i], "DBC") == 0)
				{
					pEnv->DBC = val;
				}
				else if(strcmp(argv[i], "IBA") == 0)
				{
					pEnv->IBA = val;
				}
				else if(strcmp(argv[i], "IBAM") == 0)
				{
					pEnv->IBAM = val;
				}
				else if(strcmp(argv[i], "DBA") == 0)
				{
					pEnv->DBA = val;
				}
				else if(strcmp(argv[i], "DBAM") == 0)
				{
					pEnv->DBAM = val;
				}
				else if(strcmp(argv[i], "DBD") == 0)
				{
					pEnv->DBD = val;
				}
				else if(strcmp(argv[i], "DBDM") == 0)
				{
					pEnv->DBDM = val;
				}
				else
				{
					SHELL_PRINT("Unknown register %s\n", argv[i]);
					return;
				}

				debug_set_env();
			}
			else
			{
				SHELL_PRINT("Invalid memory specification %s\n", pEquals);
				return;
			}

		}
		else
		{
			SHELL_PRINT("Invalid register specification %s\n", argv[i]);
			return;
		}
	}
}

*/

void debugPrintHWRegs(void)
{
	struct HwDebugEnv *pEnv = &psplinkHwContext;

	debug_get_env();
	SHELL_PRINT("<HW Debug Registers>\n");
	SHELL_PRINT("%-6s: 0x%08X\n", "DRCNTL", pEnv->DRCNTL);
	SHELL_PRINT("%-6s: 0x%08X\n", "IBC", pEnv->IBC);
	SHELL_PRINT("%-6s: 0x%08X\n", "DBC", pEnv->DBC);
	SHELL_PRINT("%-6s: 0x%08X\n", "IBA", pEnv->IBA);
	SHELL_PRINT("%-6s: 0x%08X\n", "IBAM", pEnv->IBAM);
	SHELL_PRINT("%-6s: 0x%08X\n", "DBA", pEnv->DBA);
	SHELL_PRINT("%-6s: 0x%08X\n", "DBAM", pEnv->DBAM);
	SHELL_PRINT("%-6s: 0x%08X\n", "DBD", pEnv->DBD);
	SHELL_PRINT("%-6s: 0x%08X\n", "DBDM", pEnv->DBDM);
}
