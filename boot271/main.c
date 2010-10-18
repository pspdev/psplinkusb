/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - PSPLINK bootstrap for 271
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <string.h>
#include "../psplink/version.h"

/* Define the module info section, note the 0x1000 flag to enable start in kernel mode */
PSP_MODULE_INFO("PSPLINKLOADER", 0, 1, 1);

/* Define the thread attribute as 0 so that the main thread does not get converted to user mode */
PSP_MAIN_THREAD_ATTR(0);

#define MODULE "psplink.prx"

int _main(SceSize args, void *argp)
{
	char path[1024];
	char *slash;

	do
	{
		pspDebugScreenInit();
		pspDebugScreenPrintf("PSPLink Bootstrap TyRaNiD (c) 2k7 Version %s\n", PSPLINK_VERSION);
		strcpy(path, argp);
		slash = strrchr(path, '/');
		if(slash == NULL)
		{
			pspDebugScreenPrintf("Could not find last slash\n");
			break;
		}
		slash++;
		*slash = 0;
		strcat(path, MODULE);

		SceUID mod = sceKernelLoadModule(path, 0, NULL);
		if (mod < 0)
		{
			pspDebugScreenPrintf("Error 0x%08X loading module.\n", mod);
			break;
		}

		mod = sceKernelStartModule(mod, args, argp, NULL, NULL);
		if (mod < 0)
		{
			pspDebugScreenPrintf("Error 0x%08X starting module.\n", mod);
			break;
		}

		sceKernelSelfStopUnloadModule(1, 0, NULL);
	}
	while(0);

	sceKernelDelayThread(2000000);
	sceKernelExitGame();

	return 0;
}

int module_start(SceSize args, void *argp)
{
	SceUID uid;

	uid = sceKernelCreateThread("PsplinkBoot", _main, 32, 0x10000, 0, 0);
	if(uid < 0)
	{
		return 1;
	}
	sceKernelStartThread(uid, args, argp);

	return 0;
}
