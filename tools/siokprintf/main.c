/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK SIO Kprintf driver
 *
 * Copyright (c) 2007 James F <tyranid@gmail.com>
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

PSP_MODULE_INFO("SioKprintf", PSP_MODULE_KERNEL, 1, 1);

#define DEFAULT_BAUD 115200

/* Define some important parameters, not really sure on names. Probably doesn't matter */
#define PSP_UART4_FIFO 0xBE500000
#define PSP_UART4_STAT 0xBE500018
#define PSP_UART4_DIV1 0xBE500024
#define PSP_UART4_DIV2 0xBE500028
#define PSP_UART4_CTRL 0xBE50002C
#define PSP_UART_CLK   96000000
#define PSP_UART_TXFULL  0x20
#define PSP_UART_RXEMPTY 0x10

/* Some function prototypes we will need */
int sceHprmEnd(void);
int sceSysregUartIoEnable(int uart);
int sceSysconCtrlHRPower(int power);

void sioPutchar(int ch)
{
	while(_lw(PSP_UART4_STAT) & PSP_UART_TXFULL);
	_sw(ch, PSP_UART4_FIFO);
}

void sioSetBaud(int baud)
{
	int div1, div2;

	/* rate set using the rough formula div1 = (PSP_UART_CLK / baud) >> 6 and
	 * div2 = (PSP_UART_CLK / baud) & 0x3F
	 * The uart4 driver actually uses a slightly different formula for div 2 (it
	 * adds 32 before doing the AND, but it doesn't seem to make a difference
	 */
	div1 = PSP_UART_CLK / baud;
	div2 = div1 & 0x3F;
	div1 >>= 6;

	_sw(div1, PSP_UART4_DIV1);
	_sw(div2, PSP_UART4_DIV2);
	_sw(0x60, PSP_UART4_CTRL);
}

static void _sioInit(void)
{
	/* Shut down the remote driver */
	sceHprmEnd();
	/* Enable UART 4 */
	sceSysregUartIoEnable(4);
	/* Enable remote control power */
	sceSysconCtrlHRPower(1);
	/* Delay briefly */
	sceKernelDelayThread(2000000);
}

static void PutCharDebug(unsigned short *data, unsigned int type)
{
	if((type & 0xFF00) == 0)
	{
		if(type == '\n')
		{
			sioPutchar('\r');
		}

		sioPutchar(type);
	}
}

int main_thread(SceSize args, void *argp)
{
	_sioInit();
	sioSetBaud(115200);

	sceKernelRegisterDebugPutchar(PutCharDebug);

	sceKernelExitDeleteThread(0);

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int thid;

	/* Create a high priority thread */
	thid = sceKernelCreateThread("SioKprintf", main_thread, 7, 0x800, 0, NULL);
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
