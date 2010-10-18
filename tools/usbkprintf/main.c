/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK USB Kprintf driver
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspkdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("UsbKprintf", PSP_MODULE_KERNEL, 1, 1);

#define KPRINTF_BUF_SIZE 4096
#define KPRINTF_EVENT    1

SceUID g_eventflag;
unsigned char g_buf[KPRINTF_BUF_SIZE];
int g_writepos;
int g_readpos;
int g_total;

static int add_char(unsigned char ch)
{
	int intc;
	int total;

	intc = pspSdkDisableInterrupts();
	if(g_total < KPRINTF_BUF_SIZE)
	{
		g_buf[g_writepos++] = ch;
		g_writepos %= KPRINTF_BUF_SIZE;
		g_total++;
	}
	total = g_total;
	pspSdkEnableInterrupts(intc);

	return total;
}

static void PutCharDebug(unsigned short *data, unsigned int type)
{
	int total = 0;

	if((type & 0xFF00) == 0)
	{
		if((type == '\n') || (type == '\r') || (type >= 32))
		{
			total = add_char((unsigned char) type);
		}
	}

	/* If we receive end of string or the buffer is full signal */
	if((type == 0x201) || (total == KPRINTF_BUF_SIZE))
	{
		/* Set event flag */
		sceKernelSetEventFlag(g_eventflag, KPRINTF_EVENT);
	}
}

int main_thread(SceSize args, void *argp)
{
	int ret;
	int intc;
	unsigned char *plow, *phigh;
	int low, high;

	memset(g_buf, 0, KPRINTF_BUF_SIZE);
	g_writepos = 0;
	g_readpos = 0;
	g_total = 0;
	plow = NULL;
	phigh = NULL;

	g_eventflag = sceKernelCreateEventFlag("UsbKprintfEvent", 0, 0, 0);
	if(g_eventflag < 0)
	{
		printf("Error creating event flag %08X\n", g_eventflag);
	}

	sceKernelRegisterDebugPutchar(PutCharDebug);

	while(1)
	{
		ret = sceKernelWaitEventFlag(g_eventflag, KPRINTF_EVENT, 0x21, 0, 0);
		if(ret < 0)
		{
			sceKernelExitDeleteThread(0);
		}

		low = 0;
		high = 0;

		intc = pspSdkDisableInterrupts();
		if(g_total > 0)
		{
			plow = &g_buf[g_readpos];
			low = (KPRINTF_BUF_SIZE - g_readpos) < g_total ? (KPRINTF_BUF_SIZE - g_readpos) : g_total ;
			if(low < g_total)
			{
				phigh = g_buf;
				high = g_total - low;
			}
		}
		pspSdkEnableInterrupts(intc);

		if((low != 0) && (high == 0))
		{
			printf("%.*s", low, plow);
		}
		else if((low != 0) && (high != 0))
		{
			printf("%.*s%.*s", low, plow, high, phigh);
		}

		intc = pspSdkDisableInterrupts();
		g_total -= (low+high);
		g_readpos += (low+high);
		g_readpos %= KPRINTF_BUF_SIZE;
		pspSdkEnableInterrupts(intc);
	}

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int thid;

	/* Create a high priority thread */
	thid = sceKernelCreateThread("UsbKprintf", main_thread, 7, 0x800, 0, NULL);
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
