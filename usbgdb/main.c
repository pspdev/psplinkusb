/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK GDB stub
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/usbgdb/main.c $
 * $Id: main.c 2179 2007-02-15 17:38:02Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspkdebug.h>
#include <pspsdk.h>
#include <pspctrl.h>
#include <psppower.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exception.h>
#include <debug.h>
#include <util.h>
#include <usbhostfs.h>
#include <usbasync.h>
#include "common.h"

PSP_MODULE_INFO("GDBServer", PSP_MODULE_KERNEL, 1, 1);

#define GDB_POLL_TIMEOUT 1000000

struct AsyncEndpoint g_endp;
DebugEventHandler g_handler;
struct GdbContext g_context;
SceUID g_thid = -1;

int initialise(SceSize args, void *argp)
{
	int len;
	int fd;
	Elf32_Ehdr hdr;

	memset(&g_context, 0, sizeof(g_context));
	if(argp == NULL)
	{
		return 0;
	}

	len = strlen((char*) argp)+1;
	argp += len;
	args -= len;
	if(args <= 0)
	{
		return 0;
	}

	g_context.argp = argp;
	g_context.args = args;

	fd = sceIoOpen((char*) argp, PSP_O_RDONLY, 0777);
	if(fd < 0)
	{
		printf("%s does not exist\n", (char*) argp);
		return 0;
	}

	len = sceIoRead(fd, &hdr, sizeof(hdr));

	sceIoClose(fd);
	if(len != sizeof(hdr))
	{
		printf("Could not read in ELF header\n");
		return 0;
	}

	if(hdr.e_magic != ELF_MAGIC)
	{
		printf("Invalid ELF magic\n");
		return 0;
	}

	if(hdr.e_type == ELF_MIPS_TYPE)
	{
		g_context.elf = 1;
	}
	else if(hdr.e_type != ELF_PRX_TYPE)
	{
		printf("Invalid ELF type code\n");
		return 0;
	}

	g_context.uid = sceKernelLoadModule(argp, 0, NULL);
	sceIoClose(fd);

	if(g_context.uid < 0)
	{
		printf("Could not load module %s (0x%08X)\n", (char*) argp, g_context.uid);
		return 0;
	}


	if(!psplinkReferModule(g_context.uid, &g_context.info))
	{
		printf("Could not refer module info\n");
		return 0;
	}

	g_context.ctx.regs.epc = g_context.info.entry_addr;
	g_context.ctx.regs.cause = 9 << 2;

	printf("Loaded %s - UID 0x%08X, Entry 0x%08X\n", (char*)argp, g_context.uid, g_context.info.entry_addr);

	return 1;
}

/* These should be changed on for different remote methods */
int GdbReadByte(unsigned char *address, unsigned char *dest)
{
	u32 addr;
	int nibble;
	int valid = 0;

	addr = (u32) address;
	nibble = addr >> 28;
	addr &= 0x0FFFFFFF;

	if((addr >= 0x08800000) && (addr < 0x0A000000))
	{
		if((nibble == 0) || (nibble == 4) || (nibble == 8) || (nibble == 10))
		{
			valid = 1;
		}
	}

	if((addr >= 0x08000000) && (addr < 0x08400000))
	{
		if((nibble == 8) || (nibble == 10))
		{
			valid = 1;
		}
	}

	if(valid)
	{
		*dest = *address;
		return 1;
	}

	return 0;
}

int GdbWriteByte(char val, unsigned char *dest)
{
	u32 addr;
	int nibble;
	int valid = 0;

	addr = (u32) dest;
	nibble = addr >> 28;
	addr &= 0x0FFFFFFF;

	if((addr >= 0x08800000) && (addr < 0x0A000000))
	{
		if((nibble == 0) || (nibble == 4) || (nibble == 8) || (nibble == 10))
		{
			valid = 1;
		}
	}

	if((addr >= 0x08000000) && (addr < 0x08400000))
	{
		if((nibble == 8) || (nibble == 10))
		{
			valid = 1;
		}
	}

	if(valid)
	{
		*dest = val;
		return 1;
	}

	return 0;
}

int putDebugChar(unsigned char ch)
{
	return usbAsyncWrite(ASYNC_GDB, &ch, 1);
}

int getDebugChar(unsigned char *ch)
{
	int ret = 0;

	*ch = 0;

	do
	{
		ret = usbAsyncRead(ASYNC_GDB, ch, 1);
	}
	while(ret < 1);

	return ret;
}

int peekDebugChar(unsigned char *ch)
{
	int ret = 0;
	int intc;

	*ch = 0;
	intc = pspSdkDisableInterrupts();
	if(g_endp.size > 0)
	{
		*ch = g_endp.buffer[g_endp.read_pos];
		ret = 1;
	}
	pspSdkEnableInterrupts(intc);

	return ret;
}

int writeDebugData(void *data, int len)
{
	return usbAsyncWrite(ASYNC_GDB, data, len);
}

int main_thread(SceSize args, void *argp)
{
	struct PsplinkContext *ctx;
	int ret;
	SceUInt timeout;
	SceUID thids[20];
	int count;
	int intc;

	printf("PSPLink USB GDBServer (c) 2k7 TyRaNiD\n");
	if(!initialise(args, argp))
	{
		printf("Usage: usbgdb.prx program [args]\n");
		sceKernelExitDeleteThread(0);
	}

	if(usbAsyncRegister(ASYNC_GDB, &g_endp) < 0)
	{
		printf("Could not register GDB provider\n");
		sceKernelExitDeleteThread(0);
	}

	usbWaitForConnect();
	memset(&g_handler, 0, sizeof(g_handler));
	g_handler.size = sizeof(g_handler);
	g_handler.membase = g_context.info.text_addr;
	g_handler.memtop = g_context.info.text_addr + g_context.info.text_size;
	g_handler.mbox = sceKernelCreateMbx("GDBMbx", 0, NULL);
	if(g_handler.mbox < 0)
	{
		printf("Could not create message box\n");
		sceKernelExitDeleteThread(0);
	}

	if(debugRegisterEventHandler(&g_handler) < 0)
	{
		printf("Could not register event handler\n");
		sceKernelExitDeleteThread(0);
	}

	if(GdbHandleException(&g_context.ctx))
	{
		while(1)
		{
			timeout = GDB_POLL_TIMEOUT;
			ret = debugWaitDebugEvent(&g_handler, &ctx, &timeout);
			
			if(ret == 0)
			{
				DEBUG_PRINTF("ctx %p, epc 0x%08X\n", ctx, ctx->regs.epc);
				ret = GdbHandleException(ctx);
				sceKernelWakeupThread(ctx->thid);
				if(ret == 0)
				{
					break;
				}
			}
			else if(ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			{
				unsigned char ch;

				if(peekDebugChar(&ch) && (ch == 3))
				{
					DEBUG_PRINTF("Break Issued\n");
					intc = pspSdkDisableInterrupts();
					count = psplinkReferThreadsByModule(SCE_KERNEL_TMID_Thread, g_context.uid, thids, 20);
					if(count > 0)
					{
						/* We just break the first thread */
						/* Could in theory break on the thread which we are interested in ? */
						debugBreakThread(thids[0]);
					}
					pspSdkEnableInterrupts(intc);

					/* Should have a fallback if it just wont stop
						GdbHandleException(&g_context.ctx);
					*/
				}
				continue;
			}
			else
			{
				printf("Error waiting for debug event 0x%08X\n", ret);
				break;
			}
		}
	}

	debugUnregisterEventHandler(&g_handler);
	sceKernelExitDeleteThread(0);

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	/* Create a high priority thread */
	g_thid = sceKernelCreateThread("GDBServer", main_thread, 15, 0x4000, 0, NULL);
	if(g_thid >= 0)
	{
		sceKernelStartThread(g_thid, args, argp);
	}

	return 0;
}

void stop_gdb(void)
{
	if(g_thid > 0)
	{
		sceKernelSuspendThread(g_thid);
	}

	/* Cancel mbx receive, should then force the thread to exit */
	sceKernelCancelReceiveMbx(g_handler.mbox, NULL);
}

/* Module stop entry */
int module_stop(SceSize args, void *argp)
{
	stop_gdb();

	return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
	stop_gdb();

	return 0;
}
