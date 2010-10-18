/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK USB Shell main code
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/usbshell/main.c $
 * $Id: main.c 2034 2006-10-18 17:02:37Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspkdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usbhostfs.h>
#include <usbasync.h>

PSP_MODULE_INFO("USBShell", PSP_MODULE_KERNEL, 1, 1);

#define MAX_CLI 4096

void ttySetUsbHandler(PspDebugPrintHandler usbShellHandler, PspDebugPrintHandler usbStdoutHandler, PspDebugPrintHandler usbStderrHandler);
int psplinkParseCommand(unsigned char *command);
void psplinkPrintPrompt(void);
void psplinkExitShell(void);

struct AsyncEndpoint g_endp;

int usbShellPrint(const char *data, int size)
{
	usbAsyncWrite(ASYNC_SHELL, data, size);

	return size;
}

int usbStdoutPrint(const char *data, int size)
{
	usbAsyncWrite(ASYNC_STDOUT, data, size);

	return size;
}

int usbStderrPrint(const char *data, int size)
{
	usbAsyncWrite(ASYNC_STDERR, data, size);

	return size;
}

int main_thread(SceSize args, void *argp)
{
	unsigned char cli[MAX_CLI];
	int cli_pos = 0;

	usbAsyncRegister(ASYNC_SHELL, &g_endp);
	ttySetUsbHandler(usbShellPrint, usbStdoutPrint, usbStderrPrint);
	usbWaitForConnect();
	psplinkPrintPrompt();

	while(1)
	{
		if(usbAsyncRead(ASYNC_SHELL, &cli[cli_pos], 1) < 1)
		{
			sceKernelDelayThread(250000);
			continue;
		}

		if(cli[cli_pos] == '\n')
		{
			cli[cli_pos] = 0;
			if(psplinkParseCommand(cli) == 1)
			{
				psplinkExitShell();
			}
			cli_pos = 0;
		}
		else
		{
			if(cli_pos < (MAX_CLI-1))
			{
				cli_pos++;
			}
		}
	}

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int thid;

	/* Create a high priority thread */
	thid = sceKernelCreateThread("USBShell", main_thread, 12, 0x2000, 0, NULL);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, args, argp);
	}
	return 0;
}

/* Module stop entry */
int module_stop(SceSize args, void *argp)
{
	return 0;
}
