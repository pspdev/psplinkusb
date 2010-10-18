/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * tty.c - PSPLINK kernel module tty code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/tty.c $
 * $Id: tty.c 2304 2007-08-26 17:21:11Z tyranid $
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <pspkerror.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspumd.h>
#include <psputilsforkernel.h>
#include "psplink.h"
#include "apihook.h"
#include "util.h"
#include "libs.h"
#include "pspstdio.h"

#define STDIN_BUFSIZE 4096

static PspDebugInputHandler g_usbStdinHandler = NULL;
static PspDebugPrintHandler g_usbStdoutHandler = NULL;
static PspDebugPrintHandler g_usbStderrHandler = NULL;

extern struct GlobalContext g_context;

static int stdoutHandler(const char *data, int size)
{
	if(g_usbStdoutHandler)
	{
		g_usbStdoutHandler(data, size);
	}

	return size;
}

static int stderrHandler(const char *data, int size)
{
	if(g_usbStderrHandler)
	{
		g_usbStderrHandler(data, size);
	}

	return size;
}

static int inputHandler(char *data, int size)
{
	if(g_usbStdinHandler)
	{
		return g_usbStdinHandler(data, size);
	}

	return 0;
}

void ttySetUsbHandler(PspDebugPrintHandler usbStdoutHandler, PspDebugPrintHandler usbStderrHandler, PspDebugInputHandler usbStdinHandler)
{
	g_usbStdoutHandler = usbStdoutHandler;
	g_usbStderrHandler = usbStderrHandler;
	g_usbStdinHandler = usbStdinHandler;
}

static int close_func(int fd)
{
	int ret = SCE_KERNEL_ERROR_FILEERR;

	if(fd > 2)
	{
		ret = sceIoClose(fd);
	}

	return ret;
}

void ttyInit(void)
{
	SceUID uid;

	if(stdioTtyInit() < 0)
	{
		Kprintf("Could not initialise tty\n");
		return;
	}

	stdioInstallStdoutHandler(stdoutHandler);
	stdioInstallStderrHandler(stderrHandler);
	stdioInstallStdinHandler(inputHandler);
	/* Install a patch to prevent a naughty app from closing stdout */
	uid = refer_module_by_name("sceIOFileManager", NULL);
	if(uid >= 0)
	{
		apiHookByNid(uid, "IoFileMgrForUser", 0x810c4bc3, close_func);
		libsPatchFunction(uid, "IoFileMgrForKernel", 0x3c54e908, 0xFFFF);
	}
}
