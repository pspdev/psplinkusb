/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * debug.h - Debugger code for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/debug.h $
 * $Id: debug.h 2178 2007-02-15 17:30:31Z tyranid $
 */
#ifndef __DEBUGINC_H__
#define __DEBUGINC_H__

#include <pspkernel.h>
#include "exception.h"

#define DEBUG_MAX_BPS 32

enum DebugBreakpointFlags
{
	/* Indicates this breakpoint is active */
	DEBUG_BP_ACTIVE     = 1,
	/* Indicates once this breakpoint is hit we remove it */
	DEBUG_BP_ONESHOT    = 2,
	/* Indicates this is a hardware breakpoint */
	DEBUG_BP_HARDWARE   = 4,
	/* Indicates this breakpoint is current disabled by the user */
	DEBUG_BP_DISABLED  = 8,
	/* Used as a visual flag to indicate this breakpoint is a step */
	DEBUG_BP_STEP       = 16,
	/* Indicates this breakpoint should be reneabled on the next exception */
	DEBUG_BP_NEXT_REENABLE = 32,
};

struct Breakpoint
{
	/* Address of breakpoint */
	unsigned int addr;
	/* Original instruction at addr */
	unsigned int inst;
	/* Flags */
	unsigned int flags;
	/* Are we waiting for a specific thread id ? */
	SceUID thid;
	/* Link to the corresponding step breakpoint */
	struct Breakpoint* step;
};

typedef struct _DebugEventHandler
{
	union {
		int size;
		struct _DebugEventHandler *pNext;
	};
	unsigned int membase;
	unsigned int memtop;
	SceUID mbox;
} DebugEventHandler;

int debugRegisterEventHandler(DebugEventHandler *handler);
int debugUnregisterEventHandler(DebugEventHandler *handler);
int debugWaitDebugEvent(DebugEventHandler *handler, struct PsplinkContext **ctx, SceUInt *timeout);
void debugPrintBPs(void);
struct Breakpoint* debugFindBPByIndex(int i);
int debugDeleteBP(unsigned int addr);
int debugDisableBP(unsigned int addr);
int debugEnableBP(unsigned int addr);
struct Breakpoint *debugSetBP(unsigned int address, unsigned int flags, SceUID thid);
int debugBreakThread(SceUID uid);
void debugDisableHW();

/* For internal use only */
void debugStep(struct PsplinkContext *ctx, int skip);
void debugClearException(struct PsplinkContext *ctx);
int debugHandleException(struct PsplinkContext *ctx);
void debugHwInit(void);
void debugPrintHWRegs(void);

#endif
