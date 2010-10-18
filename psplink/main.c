/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK kernel module main code.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/main.c $
 * $Id: main.c 2322 2007-09-30 17:49:32Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspkdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspumd.h>
#include <psputilsforkernel.h>
#include <pspsysmem_kernel.h>
#include <pspthreadman_kernel.h>
#include <psploadexec_kernel.h>
#include <usbhostfs.h>
#include "memoryUID.h"
#include "psplink.h"
#include "psplinkcnf.h"
#include "debug.h"
#include "util.h"
#include "shell.h"
#include "config.h"
#include "exception.h"
#include "apihook.h"
#include "tty.h"
#include "libs.h"
#include "modload.h"
#include "decodeaddr.h"

PSP_MODULE_INFO("PSPLINK", 0x1000, 1, 1);

#define BOOTLOADER_NAME "PSPLINKLOADER"

struct GlobalContext g_context;

void save_execargs(int argc, char **argv);

int unload_loader(void)
{
	SceModule *mod;
	SceUID modid;
	int ret = 0;
	int status;

	mod = sceKernelFindModuleByName(BOOTLOADER_NAME);
	if(mod != NULL)
	{
		DEBUG_PRINTF("Loader UID: %08X\n", mod->modid);
		/* Stop module */
		modid = mod->modid;
		ret = sceKernelStopModule(modid, 0, NULL, &status, NULL);
		if(ret >= 0)
		{
			ret = sceKernelUnloadModule(modid);
		}
	}
	else
	{
		Kprintf("Couldn't find bootloader\n");
	}

	return 0;
}

void parse_sceargs(SceSize args, void *argp, int *argc, char **argv)
{
	int  loc = 0;
	char *ptr = argp;

	*argc = 0;
	while(loc < args)
	{
		argv[*argc] = &ptr[loc];
		loc += strlen(&ptr[loc]) + 1;
		(*argc)++;
		if(*argc == (MAX_ARGS-1))
		{
			break;
		}
	}

	argv[*argc] = NULL;
}

void load_psplink_user(const char *bootpath)
{
	char prx_path[MAXPATHLEN];

	strcpy(prx_path, bootpath);
	strcat(prx_path, "psplink_user.prx");
	load_start_module(prx_path, 0, NULL);
}

SceUID load_gdb(const char *bootpath, int argc, char **argv)
{
	char prx_path[MAXPATHLEN];

	strcpy(prx_path, bootpath);
	strcat(prx_path, "usbgdb.prx");
	g_context.gdb = 1;
	return load_start_module(prx_path, argc, argv);
}

int reset_thread(SceSize args, void *argp)
{
	psplinkReset();

	return 0;
}

void exit_reset(void)
{
	psplinkSetK1(0);

	if(g_context.resetonexit)
	{
		/* Create a new thread to do the reset */
		SceUID thid;

		thid = sceKernelCreateThread("PspLinkReset", reset_thread, 8, 4*1024, 0, NULL);
		if(thid >= 0)
		{
			sceKernelStartThread(thid, 0, NULL);
		}
	}
	else
	{
		SHELL_PRINT("\nsceKernelExitGame caught!\n");
		/* Kill the thread, bad idea to drop back to the program */
	}

	sceKernelExitThread(0);
}

void psplinkStop(void)
{
	if(g_context.thevent >= 0)
	{
		sceKernelReleaseThreadEventHandler(g_context.thevent);
	}
}

void psplinkReset(void)
{
#if _PSP_FW_VERSION >= 200
	{
		struct SceKernelLoadExecVSHParam param; 
		const char *rebootkey = NULL;
		char argp[256];
		int  args;

		args = 0;
		strcpy(argp, g_context.bootfile);
		args += strlen(g_context.bootfile)+1;
		strcpy(&argp[args], g_context.currdir);
		args += strlen(g_context.currdir)+1;
		
		memset(&param, 0, sizeof(param)); 
		param.size = sizeof(param); 
		param.args = args;
		param.argp = argp;
		switch(g_context.rebootkey)
		{
			case REBOOT_MODE_GAME: rebootkey = "game";
								   break;
			case REBOOT_MODE_VSH : rebootkey = "vsh";
								   break;
			case REBOOT_MODE_UPDATER : rebootkey = "updater";
									   break;
			default: rebootkey = NULL;
					 break;

		};
		param.key = rebootkey; 
		param.vshmain_args_size = 0; 
		param.vshmain_args = NULL; 

		debugDisableHW();
		psplinkSetK1(0);
		SHELL_PRINT("Resetting psplink\n");
		psplinkStop();

		sceKernelSuspendAllUserThreads();

		sceKernelLoadExecVSHMs2(g_context.bootfile, &param);
	}
#else
	{
		struct SceKernelLoadExecParam le;
		struct SavedContext *save = (struct SavedContext *) SAVED_ADDR;
		const char *rebootkey = NULL;

		save->magic = SAVED_MAGIC;
		strcpy(save->currdir, g_context.currdir);
		save->rebootkey = g_context.rebootkey;

		debugDisableHW();
		psplinkSetK1(0);
		SHELL_PRINT("Resetting psplink\n");
		psplinkStop();

		le.size = sizeof(le);
		le.args = strlen(g_context.bootfile) + 1;
		le.argp = (char *) g_context.bootfile;
		switch(g_context.rebootkey)
		{
			case REBOOT_MODE_GAME: rebootkey = "game";
								   break;
			case REBOOT_MODE_VSH : rebootkey = "vsh";
								   break;
			case REBOOT_MODE_UPDATER : rebootkey = "updater";
									   break;
			default: rebootkey = NULL;
					 break;

		};
		le.key = rebootkey;

		sceKernelSuspendAllUserThreads();

		sceKernelLoadExec(g_context.bootfile, &le);
	}
#endif
}

void psplinkExitShell(void)
{
#if _PSP_FW_VERSION >= 200
	{
		sceKernelExitVSHVSH(NULL);
	}
#else
	{
		sceKernelExitGame();
	}
#endif
}

int psplinkPresent(void)
{
	return 1;
}

int RegisterExceptionDummy(void)
{
	return 0;
}

/* Patch out the exception handler setup call for apps which come after us ;P */
int psplinkPatchException(void)
{
	unsigned int *addr;
	int intc;

	intc = pspSdkDisableInterrupts();
	addr = libsFindExportAddrByNid(refer_module_by_name("sceExceptionManager", NULL), "ExceptionManagerForKernel", 0x565C0B0E);
	if(addr)
	{
		*addr = (unsigned int) RegisterExceptionDummy;
		sceKernelDcacheWritebackInvalidateRange(addr, 4);
		sceKernelIcacheInvalidateRange(addr, 4);
	}
	pspSdkEnableInterrupts(intc);

	return 0;
}

void initialise(SceSize args, void *argp)
{
	struct ConfigContext ctx;
	const char *init_dir = "host0:/";
	int (*g_sceUmdActivate)(int, const char *);
	int argc;
	char *argv[MAX_ARGS];

	memset(&g_context, 0, sizeof(g_context));
	map_firmwarerev();
	exceptionInit();
	g_context.thevent = -1;
	parse_sceargs(args, argp, &argc, argv);

	if(argc > 0)
	{
		char *lastdir;

		g_context.bootfile = argv[0];
		lastdir = strrchr(argv[0], '/');
		if(lastdir != NULL)
		{
			memcpy(g_context.bootpath, argv[0], lastdir - argv[0] + 1);
		}
	}

	configLoad(g_context.bootpath, &ctx);

	if(ctx.pid)
	{
		g_context.pid = ctx.pid;
	}
	else
	{
		g_context.pid = HOSTFSDRIVER_PID;
	}

	ttyInit();
	init_usbhost(g_context.bootpath);

#if _PSP_FW_VERSION >= 200
	if(argc > 1)
	{
		init_dir = argv[1];
	}
#else
	{
		struct SavedContext *save = (struct SavedContext *) SAVED_ADDR;
		if(save->magic == SAVED_MAGIC)
		{
			init_dir = save->currdir;
			save->magic = 0;
			g_context.rebootkey = save->rebootkey;
		}
	}
#endif

	if(shellInit(init_dir) < 0)
	{
		sceKernelExitGame();
	}

	g_sceUmdActivate = (void*) libsFindExportByNid(refer_module_by_name("sceUmd_driver", NULL), 
			"sceUmdUser", 0xC6183D47);
	if(g_sceUmdActivate)
	{
		g_sceUmdActivate(1, "disc0:");
	}

	/* Hook sceKernelExitGame */
	apiHookByNid(refer_module_by_name("sceLoadExec", NULL), "LoadExecForUser", 0x05572A5F, exit_reset);

	unload_loader();

	psplinkPatchException();

	if(ctx.enableuser)
	{
		load_psplink_user(g_context.bootpath);
	}

	g_context.resetonexit = ctx.resetonexit;

	sceKernelRegisterDebugPutchar(NULL);
	enable_kprintf(1);
	debugHwInit();
	modLoad(g_context.bootpath);
}

/* Simple thread */
int main_thread(SceSize args, void *argp)
{
	initialise(args, argp);

	shellParseThread(0, NULL);
	sceKernelSleepThread();

	return 0;
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
	int thid;

	/* Create a high priority thread */
	thid = sceKernelCreateThread("PspLink", main_thread, 8, 64*1024, 0, NULL);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, args, argp);
	}

	return 0;
}
