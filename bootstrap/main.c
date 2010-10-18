/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK bootstrap
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/bootstrap/main.c $
 * $Id: main.c 2200 2007-03-08 21:21:20Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <string.h>
#include <stdio.h>
#include "../psplink/version.h"

PSP_MODULE_INFO("PSPLINKLOADER", 0x1000, 1, 1);
/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(0);

/* Define printf, just to make typing easier */
#define printf	pspDebugScreenPrintf

SceUID load_module(const char *path, int flags, int type)
{
	SceKernelLMOption option;
	SceUID mpid;

	/* If the type is 0, then load the module in the kernel partition, otherwise load it
	   in the user partition. */
	if (type == 0) {
		mpid = 1;
	} else {
		mpid = 2;
	}

	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.mpidtext = mpid;
	option.mpiddata = mpid;
	option.position = 0;
	option.access = 1;

	return sceKernelLoadModule(path, flags, type > 0 ? &option : NULL);
}

int main_thread(SceSize args, void *argp)
{
	char *argv0;
	char prx_path[256];
	char *path;
	SceUID modid;
	int ret;

	pspDebugScreenInit();
	sceDisplayWaitVblankStart();

	pspSdkInstallNoDeviceCheckPatch();
	pspSdkInstallNoPlainModuleCheckPatch();
	pspSdkInstallKernelLoadModulePatch();

	argv0 = (char*) argp;
	path = strrchr(argv0, '/');
	if(path != NULL)
	{
		memcpy(prx_path, argv0, path - argv0 + 1);
		prx_path[path - argv0 + 1] = 0;
		strcat(prx_path, "psplink.prx");
	}
	else
	{
		/* Well try for a default */
		strcpy(prx_path, "ms0:/psplink.prx");
	}

	/* Start mymodule.prx and dump its information */
	printf("PSPLink Bootstrap TyRaNiD (c) 2k5 Version %s\n", PSPLINK_VERSION);
	modid = load_module(prx_path, 0, 0);
	if(modid >= 0)
	{
		int status;

		printf("Starting psplink module\n");
		ret = sceKernelStartModule(modid, args, argp, &status, NULL);
		printf("Done\n");
	}
	else
	{
		printf("Error loading psplink module %08X\n", modid);
	}

	/* Let's bug out */
	sceKernelExitDeleteThread(0);

	return 0;
}

int module_start(SceSize args, void *argp) __attribute__((alias("_start")));

/* Entry point */
int _start(SceSize args, void *argp)
{
	int thid;
	u32 func;

	func = (u32) main_thread;
	func |= 0x80000000;

	/* Create a high priority thread */
	thid = sceKernelCreateThread("main_thread", (void *) func, 0x20, 0x10000, 0, NULL);
	if(thid >= 0)
	{
		sceKernelStartThread(thid, args, argp);
	}

	return 0;
}

int module_stop(void)
{
	return 0;
}
