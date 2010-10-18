/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * thctx.c - Thread context library code for psplink.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/thctx.c $
 * $Id: thctx.c 2301 2007-08-26 13:48:05Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsysmem_kernel.h>
#include <psputilsforkernel.h>
#include <pspmoduleexport.h>
#include <psploadcore.h>
#include <pspthreadman_kernel.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "psplink.h"
#include "libs.h"
#include "memoryUID.h"
#include "exception.h"

int threadFindContext(SceUID uid)
{
	SceKernelThreadKInfo info;
	struct SceThreadContext ctxCopy;
	int found = 0;
	int intc;

	memset(&ctxCopy, 0, sizeof(ctxCopy));
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	intc = pspSdkDisableInterrupts();
	if(ThreadManForKernel_2D69D086(uid, &info) == 0)
	{
		found = 1;
		if(info.thContext)
		{
			memcpy(&ctxCopy, info.thContext, sizeof(ctxCopy));
		}
	}
	pspSdkEnableInterrupts(intc);

	if(found)
	{
		SHELL_PRINT("kstack 0x%08X kstacksize 0x%08X\n", (unsigned int) info.kstack, info.kstackSize);
		SHELL_PRINT("stack  0x%08X stacksize  0x%08X\n", (unsigned int) info.stack,  info.stackSize);
		SHELL_PRINT("context 0x%08X, vfpu 0x%08X\n", (unsigned int) info.thContext, (unsigned int) info.vfpuContext);
		SHELL_PRINT("Context EPC 0x%08X, Real EPC 0x%08X\n", ctxCopy.EPC, info.retAddr);
		exceptionPrintCPURegs((unsigned int *) &ctxCopy);
		return 0;
	}

	return -1;
}

unsigned int thGetCurrentEPC(SceUID uid)
{
	SceKernelThreadKInfo info;
	unsigned int addr = 0;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if(ThreadManForKernel_2D69D086(uid, &info) == 0)
	{
		if(info.retAddr)
		{
			addr = (unsigned int) info.retAddr;
		}
		else
		{
			addr = info.thContext->EPC;
		}
	}
	
	return addr;
}

/* Get the thread context of a user thread, trys to infer the real address */
int psplinkGetFullThreadContext(SceUID uid, struct PsplinkContext *ctx)
{
	SceKernelThreadKInfo info;
	int intc;
	int ret = 0;

	if(ctx == NULL)
	{
		return 0;
	}

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	intc = pspSdkDisableInterrupts();
	if(ThreadManForKernel_2D69D086(uid, &info) == 0)
	{
		memset(ctx, 0, sizeof(struct PsplinkContext));
		ctx->thid = uid;
		memcpy(&ctx->regs.r[1], info.thContext->gpr, 31 * sizeof(unsigned int));
		memcpy(&ctx->regs.fpr[0], info.thContext->fpr, 32 * sizeof(float));
		ctx->regs.hi = info.thContext->hi;
		ctx->regs.lo = info.thContext->lo;

		if(info.retAddr)
		{
			ctx->regs.epc = info.scContext->epc;
			ctx->regs.status = info.scContext->status;
			ctx->regs.frame_ptr = info.scContext->sp;
			ctx->regs.r[29] = info.scContext->sp;
			ctx->regs.r[31] = info.scContext->ra;
			ctx->regs.r[27] = info.scContext->k1;
		}
		else
		{
			ctx->regs.epc = info.thContext->EPC;
			ctx->regs.status = info.thContext->SR;
			ctx->regs.frame_ptr = info.thContext->gpr[28];
		}

		ret = 1;
	}

	pspSdkEnableInterrupts(intc);

	return ret;
}
