/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - Main code for PSPLINK user module.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink_user/main.c $
 * $Id: main.c 2173 2007-02-07 18:51:48Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "psplink_user.h"

void _apiHook0(void);
void apiHookRegisterUserDispatch(unsigned int *);

PSP_MODULE_INFO("PSPLINK_USER", 0, 1, 1);
PSP_MAIN_THREAD_PARAMS(0x20, 64, PSP_THREAD_ATTR_USER);
PSP_MAIN_THREAD_NAME("PsplinkUser");

int module_start(int args, void *argp)
{
	apiHookRegisterUserDispatch((unsigned int *) _apiHook0);

	return 0;
}

int module_stop(int args, void *argp)
{
	return 0;
}
