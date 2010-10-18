/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK USB Remote Joystick Driver
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/tools/remotejoy/main.c $
 * $Id: main.c 2204 2007-03-11 19:16:01Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspkdebug.h>
#include <pspsdk.h>
#include <pspctrl.h>
#include <pspctrl_kernel.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspdisplay_kernel.h>
#include <psputilsforkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usbasync.h>
#include <apihook.h>
#include "remotejoy.h"

PSP_MODULE_INFO("RemoteJoy", PSP_MODULE_KERNEL, 1, 1);

//#define ENABLE_TIMING 

#ifdef BUILD_PLUGIN
#define HOSTFSDRIVER_NAME "USBHostFSDriver"
#define HOSTFSDRIVER_PID  (0x1C9)
#include <pspusb.h>
#endif

#define SCREEN_WAIT_TIMEOUT (100*1000)

//#define DEBUG_PRINTF(x, ...) printf(x, ## __VA_ARGS__)
#define DEBUG_PRINTF(x, ...)

SceCtrlData g_currjoy;
struct AsyncEndpoint g_endp;
SceUID g_scrthid = -1;
SceUID g_scrsema = -1;
SceUID g_screvent = -1;
char  *g_scrptr = NULL;
int   g_enabled = 0;
int   g_droprate = 0;
int   g_halfsize = 0;
int   g_fullcolour = 0;
unsigned int g_lastframe = 0;

int scePowerVolatileMemLock(int, char**, int*);
unsigned int psplinkSetK1(unsigned int k1);
extern u32 sceKernelSetDdrMemoryProtection;
int (*g_ctrl_common)(SceCtrlData *, int count, int type);

#define ASYNC_JOY ASYNC_USER
#define ABS(x) ((x) < 0 ? -x : x)

#define GET_JUMP_TARGET(x) (0x80000000 | (((x) & 0x03FFFFFF) << 2))

int map_axis(int real, int new)
{
	int val1, val2;

	val1 = ((int) real) - 127;
	val2 = ((int) new) - 127;

	if(ABS(val1) > ABS(val2))
	{
		return real;
	}
	else
	{
		return new;
	}
}

unsigned int calc_mask(void)
{
	int i;
	unsigned int mask = 0;

	for(i = 0; i < 32; i++)
	{
		if(sceCtrlGetButtonMask(1 << i) == 1)
		{
			mask |= (1 << i);
		}
	}

	return mask;
}

void add_values(SceCtrlData *pad_data, int count, int neg)
{
	int i;
	int intc;
	unsigned int buttons;
	int k1;

	intc = pspSdkDisableInterrupts();

	buttons = g_currjoy.Buttons;
	asm __volatile__ ( "move %0, $k1" : "=r"(k1) );
	if(k1)
	{
		buttons &= ~calc_mask();
	}

	for(i = 0; i < count; i++)
	{
		if(neg)
		{
			pad_data[i].Buttons &= ~buttons;
		}
		else
		{
			pad_data[i].Buttons |= buttons;
		}

		pad_data[i].Lx = map_axis(pad_data[i].Lx, g_currjoy.Lx);
		pad_data[i].Ly = map_axis(pad_data[i].Ly, g_currjoy.Ly);
	}
	pspSdkEnableInterrupts(intc);
}

int ctrl_hook_func(SceCtrlData *pad_data, int count, int type)
{
	int ret;

	ret = g_ctrl_common(pad_data, count, type);
	if(ret <= 0)
	{
		return ret;
	}

	add_values(pad_data, ret, type & 1);

	return ret;
}

int hook_ctrl_function(unsigned int* jump)
{
	unsigned int target;
	unsigned int func;
	int inst;

	target = GET_JUMP_TARGET(*jump);
	inst = _lw(target+8);
	if((inst & ~0x03FFFFFF) != 0x0C000000)
	{
		return 1;
	}

	g_ctrl_common = (void*) GET_JUMP_TARGET(inst);

	func = (unsigned int) ctrl_hook_func;
	func = (func & 0x0FFFFFFF) >> 2;
	_sw(0x0C000000 | func, target+8);

	return 0;
}

void copy16to16(void *in, void *out);
void copy32to16(void *in, void *out);

int copy_32bpp_raw(void *topaddr)
{
	unsigned int *u;
	unsigned int *frame_addr;
	int y;

	u = (unsigned int*) (g_scrptr + sizeof(struct JoyScrHeader));
	frame_addr = topaddr;
	for(y = 0; y < 272; y++)
	{
		memcpy(u, frame_addr, 480*4);
		u += 480;
		frame_addr += 512;
	}

	return 480*272*4;
}

int copy_32bpp_vfpu(void *topaddr)
{
	struct JoyScrHeader *head = (struct JoyScrHeader*) (0x40000000 | (u32) g_scrptr);

	sceKernelDcacheWritebackInvalidateRange(g_scrptr, sizeof(struct JoyScrHeader));
	copy32to16(topaddr, (g_scrptr + sizeof(struct JoyScrHeader)));

	head->mode = 0;
	return 480*272*2;
}

int copy_16bpp_raw(void *topaddr)
{
	unsigned short *u;
	unsigned short *frame_addr;
	int y;

	u = (unsigned short*) (g_scrptr + sizeof(struct JoyScrHeader));
	frame_addr = topaddr;
	for(y = 0; y < 272; y++)
	{
		memcpy(u, frame_addr, 480*2);
		u += 480;
		frame_addr += 512;
	}

	return 480*272*2;
}

int copy_16bpp_vfpu(void *topaddr)
{
	sceKernelDcacheWritebackInvalidateRange(g_scrptr, sizeof(struct JoyScrHeader));
	copy16to16(topaddr, g_scrptr + sizeof(struct JoyScrHeader));

	return 480*272*2;
}

int (*copy_32bpp)(void *topaddr) = copy_32bpp_vfpu;
int (*copy_16bpp)(void *topaddr) = copy_16bpp_vfpu;

void reduce_16bpp(u16 *addr)
{
	int x, y;
	u16 *addrout = addr;

	for(y = 272; y > 0; y -= 2)
	{
		for(x = 480; x > 0; x -= 2)
		{
			*addrout++ = *addr;
			addr += 2;
		}
		addr += 480;
	}
}

void reduce_32bpp(u32 *addr)
{
	int x, y;
	u32 *addrout = addr;

	for(y = 272; y > 0; y -= 2)
	{
		for(x = 480; x > 0; x -= 2)
		{
			*addrout++ = *addr;
			addr += 2;
		}
		addr += 480;
	}
}

void set_frame_buf(void *topaddr, int bufferwidth, int pixelformat, int sync)
{
	unsigned int k1;

	k1 = psplinkSetK1(0);
	if(g_lastframe == 0)
	{
		if((topaddr) && (sceKernelPollSema(g_scrsema, 1) == 0))
		{
			/* We dont wait for this to complete, probably stupid ;) */
			sceKernelSetEventFlag(g_screvent, 1);
			g_lastframe = g_droprate;
		}
	}
	else
	{
		g_lastframe--;
	}
	psplinkSetK1(k1);

	sceDisplaySetFrameBufferInternal(2, topaddr, bufferwidth, pixelformat, sync);
}

inline int build_frame(void)
{
	struct JoyScrHeader *head;
	void *topaddr;
	int bufferwidth;
	int pixelformat;
	int sync;
	
	/* Get the top level frame buffer, else get the normal frame buffer */
	sceDisplayGetFrameBufferInternal(0, &topaddr, &bufferwidth, &pixelformat, &sync);
	if(topaddr == NULL)
	{
		sceDisplayGetFrameBufferInternal(2, &topaddr, &bufferwidth, &pixelformat, &sync);
	}

	if(topaddr)
	{
		head = (struct JoyScrHeader*) g_scrptr;
		head->magic = JOY_MAGIC;
		head->mode = pixelformat;
		head->ref  = sceDisplayGetVcount();

		switch(pixelformat)
		{
			case 3: head->size = copy_32bpp(topaddr);
					break;
			case 0:
			case 1:
			case 2: head->size = copy_16bpp(topaddr);
					break;
			default: head->size = 0; 
					break;
		};

		if(head->size > 0)
		{
			if(g_halfsize)
			{
				if(g_fullcolour && (pixelformat == 3))
				{
					reduce_32bpp((u32*) (g_scrptr + sizeof(struct JoyScrHeader)));
				}
				else
				{
					reduce_16bpp((u16*) (g_scrptr + sizeof(struct JoyScrHeader)));
				}
				head->size >>= 2;
			}
			return 1;
		}
	}

	return 0;
}

int screen_thread(SceSize args, void *argp)
{
	struct JoyScrHeader *head;
	int size;
	u32* p;
	u32 baseaddr;
	u32 func;

	/* Should actually hook the memory correctly :/ */

	/* Enable free memory */
	_sw(0xFFFFFFFF, 0xBC00000C);
	g_scrptr = (char*) (0x08800000-(512*1024));

	g_scrsema = sceKernelCreateSema("ScreenSema", 0, 1, 1, NULL);
	if(g_scrsema < 0)
	{
		DEBUG_PRINTF("Could not create sema 0x%08X\n", g_scrsema);
		sceKernelExitDeleteThread(0);
	}

	if(sceKernelDevkitVersion() >= 0x01050001)
	{
		p = &sceKernelSetDdrMemoryProtection;

		baseaddr = GET_JUMP_TARGET(*p);
		_sw(0x03E00008, baseaddr);
		_sw(0x00001021, baseaddr+4);
		sceKernelDcacheWritebackInvalidateRange((void*) baseaddr, 8);
		sceKernelIcacheInvalidateRange((void*) baseaddr, 8);
	}

	p = (u32*) sceDisplaySetFrameBuf;

	baseaddr = GET_JUMP_TARGET(*p);
	func = (unsigned int) set_frame_buf;
	func = (func & 0x0FFFFFFF) >> 2;
	_sw(0x08000000 | func, baseaddr);
	_sw(0, baseaddr+4);

	sceKernelDcacheWritebackInvalidateAll();
	sceKernelIcacheInvalidateAll();

	/* Display current frame */

	if(build_frame())
	{
		head = (struct JoyScrHeader*) g_scrptr;
		size = head->size;

		usbWriteBulkData(ASYNC_JOY, g_scrptr, sizeof(struct JoyScrHeader) + size);
	}

	while(1)
	{
		int ret;
		unsigned int status;
		SceUInt timeout;

		timeout = SCREEN_WAIT_TIMEOUT;
		ret = sceKernelWaitEventFlag(g_screvent, 3, PSP_EVENT_WAITOR | PSP_EVENT_WAITCLEAR, &status, &timeout);
		if((ret < 0) && (ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT))
		{
			sceKernelExitDeleteThread(0);
		}

		if((status & 1) || (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT))
		{
#ifdef ENABLE_TIMING
			unsigned int fstart, fmid, fend;

			asm __volatile__  ( "mfc0  %0, $9\n" : "=r"(fstart) );
#endif
			_sw(0xFFFFFFFF, 0xBC00000C);
			if(build_frame())
			{
#ifdef ENABLE_TIMING
				asm __volatile__  ( "mfc0  %0, $9\n" : "=r"(fmid) );
#endif
				head = (struct JoyScrHeader*) g_scrptr;
				size = head->size;

				usbWriteBulkData(ASYNC_JOY, g_scrptr, sizeof(struct JoyScrHeader) + size);
#ifdef ENABLE_TIMING
				asm __volatile__  ( "mfc0  %0, $9\n" : "=r"(fend) );
				DEBUG_PRINTF("Total: 0x%08X Frame: 0x%08X Usb: 0x%08X\n", fend - fstart, fmid-fstart, fend-fmid);
#endif
			}

			if(ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			{
				sceKernelSignalSema(g_scrsema, 1);
			}
		}

		if(status & 2)
		{
			sceKernelSleepThread();
		}
	}

	return 0;
}

void do_screen_cmd(unsigned int value)
{
	if(value & SCREEN_CMD_FULLCOLOR)
	{
		copy_32bpp = copy_32bpp_raw;
		g_fullcolour = 1;
	}
	else
	{
		copy_32bpp = copy_32bpp_vfpu;
		g_fullcolour = 0;
	}

	if(value & SCREEN_CMD_HSIZE)
	{
		g_halfsize = 1;
	}
	else
	{
		g_halfsize = 0;
	}

	g_droprate = SCREEN_CMD_GETDROPRATE(value);

	if(value & SCREEN_CMD_ACTIVE)
	{
		/* Create a thread */
		if(g_scrthid < 0)
		{
			g_screvent = sceKernelCreateEventFlag("ScreenEvent", 0, 0, NULL);
			if(g_screvent < 0)
			{
				DEBUG_PRINTF("Could not create event 0x%08X\n", g_screvent);
				return;
			}

			g_scrthid = sceKernelCreateThread("ScreenThread", screen_thread, 16, 0x800, PSP_THREAD_ATTR_VFPU, NULL);
			if(g_scrthid >= 0)
			{
				sceKernelStartThread(g_scrthid, 0, NULL);
			}
			g_enabled = 1;
		}
		else
		{
			if(!g_enabled)
			{
				sceKernelWakeupThread(g_scrthid);
				g_enabled = 1;
			}
		}
	}
	else
	{
		/* Disable the screen display */
		/* Stop the thread at the next available opportunity */
		if(g_scrthid >= 0)
		{
			if(g_enabled)
			{
				sceKernelSetEventFlag(g_screvent, 2);
				g_enabled = 0;
			}
		}
	}
}

void send_screen_probe(void)
{
	struct JoyScrHeader head;

	head.magic = JOY_MAGIC;
	head.mode = -1;
	head.size = 0;
	head.ref = 0;

	usbAsyncWrite(ASYNC_JOY, &head, sizeof(head));
}

int main_thread(SceSize args, void *argp)
{
	struct JoyEvent joyevent;
	int intc;

#ifdef BUILD_PLUGIN
	int retVal = 0;

	retVal = sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
	if (retVal != 0) {
		Kprintf("Error starting USB Bus driver (0x%08X)\n", retVal);
		return 0;
	}
	retVal = sceUsbStart(HOSTFSDRIVER_NAME, 0, 0);
	if (retVal != 0) {
		Kprintf("Error starting USB Host driver (0x%08X)\n",
		   retVal);
		return 0;
	}

	retVal = sceUsbActivate(HOSTFSDRIVER_PID);
#endif

	if(hook_ctrl_function((unsigned int*) sceCtrlReadBufferPositive) || 
		hook_ctrl_function((unsigned int*) sceCtrlPeekBufferPositive) ||
		hook_ctrl_function((unsigned int*) sceCtrlReadBufferNegative) ||
		hook_ctrl_function((unsigned int*) sceCtrlPeekBufferNegative))
	{
		DEBUG_PRINTF("Could not hook controller functions\n");
		sceKernelExitDeleteThread(0);
	}
	sceKernelDcacheWritebackInvalidateAll();
	sceKernelIcacheInvalidateAll();

	if(usbAsyncRegister(ASYNC_JOY, &g_endp) < 0)
	{
		DEBUG_PRINTF("Could not register remotejoy provider\n");
		sceKernelExitDeleteThread(0);
	}

	usbWaitForConnect();

	/* Send a probe packet for screen display */
	send_screen_probe();

	while(1)
	{
		int len;
		len = usbAsyncRead(ASYNC_JOY, (void*) &joyevent, sizeof(joyevent));

		if((len != sizeof(joyevent)) || (joyevent.magic != JOY_MAGIC))
		{
			if(len < 0)
			{
				/* Delay thread, necessary to ensure that the kernel can reboot :) */
				sceKernelDelayThread(250000);
			}
			else
			{
				DEBUG_PRINTF("Invalid read size %d\n", len);
				usbAsyncFlush(ASYNC_JOY);
			}
			continue;
		}

		intc = pspSdkDisableInterrupts();
		switch(joyevent.type)
		{
			case TYPE_BUTTON_UP: g_currjoy.Buttons &= ~joyevent.value;
								 break;
			case TYPE_BUTTON_DOWN: g_currjoy.Buttons |= joyevent.value;
								 break;  
			case TYPE_ANALOG_Y: g_currjoy.Ly = joyevent.value;
								break;
			case TYPE_ANALOG_X: g_currjoy.Lx = joyevent.value;
								break;
			default: break;
		};
		pspSdkEnableInterrupts(intc);

		/* We do screen stuff outside the disabled interrupts */
		if(joyevent.type == TYPE_SCREEN_CMD)
		{
			do_screen_cmd(joyevent.value);
		}
		scePowerTick(0);
	}

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int thid;

	memset(&g_currjoy, 0, sizeof(g_currjoy));
	g_currjoy.Lx = 0x80;
	g_currjoy.Ly = 0x80;
	/* Create a high priority thread */
	thid = sceKernelCreateThread("RemoteJoy", main_thread, 15, 0x800, 0, NULL);
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
