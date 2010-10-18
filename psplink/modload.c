/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * modload.c - PSPLINK kernel module loader.
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/modload.c $
 * $Id: modload.c 2157 2007-01-29 22:50:26Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspumd.h>
#include <psputilsforkernel.h>
#include <pspsysmem_kernel.h>
#include <usbhostfs.h>
#include "memoryUID.h"
#include "psplink.h"
#include "util.h"

void modLoad(const char *bootpath)
{
	PspFile fmod;
	char buf[1024];
	char ini_path[1024];

	strcpy(ini_path, bootpath);
	strcat(ini_path, "modload.ini");

	if(openfile(ini_path, &fmod))
	{
		while(fdgets(&fmod, buf, sizeof(buf)))
		{
			strip_whitesp(buf);
			if(buf[0])
			{
				(void) load_start_module(buf, 0, NULL);
			}
		}
		closefile(&fmod);
	}
}
