/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * apihook.c - User mode API hook code for psplink.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/apihook.c $
 * $Id: apihook.c 2301 2007-08-26 13:48:05Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsysmem_kernel.h>
#include <psputilsforkernel.h>
#include <pspmoduleexport.h>
#include <psploadcore.h>
#include <pspthreadman_kernel.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "psplink.h"
#include "libs.h"
#include "apihook.h"
#include "decodeaddr.h"

#define APIHOOK_MAXNAME 32
#define APIHOOK_MAXPARAM 6
#define APIHOOK_MAXIDS   16

/* Define the api hooks */
void _apiHook0(void);

struct SyscallHeader
{
	void *unk;
	unsigned int basenum;
	unsigned int topnum;
	unsigned int size;
};

#define PARAM_TYPE_INT 'i'
#define PARAM_TYPE_HEX 'x'
#define PARAM_TYPE_OCT 'o'
#define PARAM_TYPE_STR 's'
#define PARAM_TYPE_PTR 'p'

#define RET_TYPE_VOID  'v'
#define RET_TYPE_HEX32 'x'
#define RET_TYPE_HEX64 'X'
#define RET_TYPE_INT32 'i'

struct ApiHookGeneric
{
	/* Function name */
	char name[APIHOOK_MAXNAME];
	/* Parameter list */
	char param[APIHOOK_MAXPARAM];
	/* Return code */
	char ret;
	/* Pointer to the real function, if NULL then invalid */
	void *func;
	/* Pointer to the location in the syscall table */
	unsigned int  *syscall;
	/* Indicates if we should sleep the thread on the syscall */
	int  sleep;
	/* Indicates if this is an import hook or not */
	int  imphook;
};

static unsigned int *g_userdispatch = NULL;

static struct ApiHookGeneric g_apihooks[APIHOOK_MAXIDS] = {
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
	{ "", "", 'v', NULL, NULL, 0, 0 },
};

void *find_syscall_addr(unsigned int addr)
{
	struct SyscallHeader *head;
	unsigned int *syscalls;
	void **ptr;
	int size;
	int i;

	asm(
			"cfc0 %0, $12\n"
			: "=r"(ptr)
	   );

	if(!ptr)
	{
		return NULL;
	}

	head = (struct SyscallHeader *) *ptr;
	syscalls = (unsigned int*) (*ptr + 0x10);
	size = (head->size - 0x10) / sizeof(unsigned int);

	for(i = 0; i < size; i++)
	{
		if(syscalls[i] == addr)
		{
			return &syscalls[i];
		}
	}

	return NULL;
}

void apiHookRegisterUserDispatch(unsigned int *dispatch)
{
	g_userdispatch = dispatch;
}

void *_apiHookHandle(int id, unsigned int *args)
{
	int intc;
	void *func = NULL;
	int k1;

	intc = pspSdkDisableInterrupts();
	if((id >= 0) && (id < APIHOOK_MAXIDS))
	{
		func = g_apihooks[id].func;
	}
	pspSdkEnableInterrupts(intc);

	k1 = psplinkSetK1(0);

	if(func)
	{
		int i;
		int j;
		char str[128];
		int strleft;

		SHELL_PRINT("Function %s called from thread 0x%08X (RA:0x%08X)\n", g_apihooks[id].name, sceKernelGetThreadId(), sceKernelGetSyscallRA());
		for(i = 0; i < APIHOOK_MAXPARAM; i++)
		{
			if(g_apihooks[id].param[i])
			{
				SHELL_PRINT("Arg %d: ", i);
				switch(g_apihooks[id].param[i])
				{
					case PARAM_TYPE_INT: SHELL_PRINT("%d\n", args[i]);
							  break;
					case PARAM_TYPE_HEX: SHELL_PRINT("0x%08X\n", args[i]);
							  break;
					case PARAM_TYPE_OCT: SHELL_PRINT("0%o\n", args[i]);
							  break;
					case PARAM_TYPE_STR: strleft = memValidate(args[i], MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
							  if(strleft == 0)
							  {
								  SHELL_PRINT("Invalid pointer 0x%08X\n", args[i]);
								  break;
							  }

							  if(strleft > (sizeof(str)-1))
							  {
								  strleft = sizeof(str) - 1;
							  }

							  strncpy(str, (const char *) args[i], strleft);
							  str[strleft] = 0;

							  SHELL_PRINT("\"%s\"\n", str);
							  break;
					case PARAM_TYPE_PTR: strleft = memValidate(args[i], MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
							  if(strleft == 0)
							  {
								  SHELL_PRINT("Invalid pointer 0x%08X\n", args[i]);
								  break;
							  }

							  strleft = strleft > 32 ? 32 : strleft;

							  SHELL_PRINT("0x%08X - ", args[i]);
							  for(j = 0; j < strleft; j++)
							  {
								  SHELL_PRINT("0x%02X ", _lb(args[i]+j));
							  }
							  SHELL_PRINT("\n");
							  break;
					default:  SHELL_PRINT("Unknown parameter type '%c'\n", g_apihooks[id].param[i]);
							  break;
				};
			}
			else
			{
				break;
			}
		}

		if(g_apihooks[id].sleep)
		{
			SHELL_PRINT("Sleeping thread 0x%08X\n", sceKernelGetThreadId());
			sceKernelSleepThread();
		}
	}

	psplinkSetK1(k1);

	return func;
}

void _apiHookReturn(int id, unsigned int* ret)
{
	int intc;
	void *func = NULL;
	int k1;

	intc = pspSdkDisableInterrupts();
	if((id >= 0) && (id < APIHOOK_MAXIDS))
	{
		func = g_apihooks[id].func;
	}
	pspSdkEnableInterrupts(intc);

	k1 = psplinkSetK1(0);

	if(func)
	{
		SHELL_PRINT("Function %s returned ", g_apihooks[id].name);
		switch(g_apihooks[id].ret)
		{
			case RET_TYPE_INT32: SHELL_PRINT("%d\n", ret[0]);
					  break;
			case RET_TYPE_HEX32: SHELL_PRINT("0x%08X\n", ret[0]);
								 break;
			case RET_TYPE_HEX64: SHELL_PRINT("0x%08X%08X\n", ret[1], ret[0]);
					  break;
			default: SHELL_PRINT("void\n");
					break;
		}

		if(g_apihooks[id].sleep)
		{
			SHELL_PRINT("Sleeping thread 0x%08X\n", sceKernelGetThreadId());
			sceKernelSleepThread();
		}
	}

	psplinkSetK1(k1);
}

void apiHookGenericPrint(void)
{
	int i;

	for(i = 0; i < APIHOOK_MAXIDS; i++)
	{
		if(g_apihooks[i].func)
		{
			SHELL_PRINT("Hook %2d: Name %s, Ret %c Param %.*s, Sleep %d, Syscall 0x%p Imp %d\n", i,
					g_apihooks[i].name, g_apihooks[i].ret, APIHOOK_MAXPARAM, g_apihooks[i].param, 
					g_apihooks[i].sleep, g_apihooks[i].syscall, g_apihooks[i].imphook);
		}
	}
}

static int find_free_hook(void)
{
	int i;

	for(i = 0; i < APIHOOK_MAXIDS; i++)
	{
		if(!g_apihooks[i].func)
		{
			break;
		}
	}
	if(i < APIHOOK_MAXIDS)
	{
		return i;
	}

	return -1;
}

static void *apiHookAddr(unsigned int *addr, void *func)
{
	int intc;

	if(!addr)
	{
		return NULL;
	}

	intc = pspSdkDisableInterrupts();
	*addr = (unsigned int) func;
	sceKernelDcacheWritebackInvalidateRange(addr, sizeof(addr));
	sceKernelIcacheInvalidateRange(addr, sizeof(addr));
	pspSdkEnableInterrupts(intc);

	return addr;
}

static void* apiHookImport(unsigned int *addr, void *func)
{
	int intc;

	if(!addr)
	{
		return NULL;
	}

	intc = pspSdkDisableInterrupts();
	*addr = 0x08000000 | ((unsigned int) func & 0x0FFFFFFF) >> 2;
	sceKernelDcacheWritebackInvalidateRange(addr, sizeof(addr));
	sceKernelIcacheInvalidateRange(addr, sizeof(addr));
	pspSdkEnableInterrupts(intc);

	return addr;
}

void apiHookGenericDelete(int id)
{
	int intc;

	if((id < 0) || (id >= APIHOOK_MAXIDS))
	{
		return;
	}

	intc = pspSdkDisableInterrupts();
	/* Restore original function */
	if(g_apihooks[id].func)
	{
		if(g_apihooks[id].imphook)
		{
			apiHookImport(g_apihooks[id].syscall, g_apihooks[id].func);
		}
		else
		{
			apiHookAddr(g_apihooks[id].syscall, g_apihooks[id].func);
		}
		g_apihooks[id].func = NULL;
	}

	pspSdkEnableInterrupts(intc);
}


static int _apiHookGenericCommon(unsigned int addr, int imphook, const char *library, const char *name, char ret, const char *format, int sleep)
{
	int id;
	unsigned int *syscall;
	unsigned int *target;
	unsigned int *hookaddr = (unsigned int*) _apiHook0;
	int result = 0;

	do
	{
		id = find_free_hook();
		if(id < 0)
		{
			SHELL_PRINT("No free API hooks left\n");
			break;
		}

		if(!addr)
		{
			SHELL_PRINT("Couldn't find export address\n");
			break;
		}

		if(imphook)
		{
			unsigned int *p = (unsigned int*) addr;
			int user;

			user = (addr & 0x80000000) ? 0 : 1;
			if(user)
			{
				if(g_userdispatch == NULL)
				{
					SHELL_PRINT("Cannot hook user calls without psplink_user\n");
					break;
				}
				hookaddr = g_userdispatch;
			}

			if((*p & ~0x03FFFFFF) != 0x08000000)
			{
				SHELL_PRINT("Cannot hook syscall imports\n");
				break;
			}

			syscall = (void*) addr;
			target  = (void*) (((*p & 0x03FFFFFF) << 2) | (addr & 0xF0000000));
		}
		else
		{
			syscall = find_syscall_addr(addr);
			if(!syscall)
			{
				SHELL_PRINT("Couldn't find syscall address\n");
				break;
			}
			target = (void*) addr;
		}

		g_apihooks[id].syscall = syscall;
		g_apihooks[id].func = target;
		g_apihooks[id].ret = ret;
		g_apihooks[id].sleep = sleep;
		strncpy(g_apihooks[id].param, format, APIHOOK_MAXPARAM);
		strncpy(g_apihooks[id].name, name, APIHOOK_MAXNAME);
		g_apihooks[id].name[APIHOOK_MAXNAME-1] = 0;
		g_apihooks[id].imphook = imphook;
		if(imphook)
		{
			apiHookImport(syscall, &hookaddr[id*2]);
		}
		else
		{
			apiHookAddr(syscall, &hookaddr[id*2]);
		}
		result = 1;
	}
	while(0);

	return result;
}

int apiHookGenericByName(SceUID uid, const char *library, const char *name, char ret, const char *format, int sleep)
{
	unsigned int addr;
	int imphook = 0;

	addr = libsFindExportByName(uid, library, name);
	if(!addr)
	{
		/* If not an export try and import */
		addr = libsFindImportAddrByName(uid, library, name);
		imphook = 1;
	}

	return _apiHookGenericCommon(addr, imphook, library, name, ret, format, sleep);
}

int apiHookGenericByNid(SceUID uid, const char *library, unsigned int nid, char ret, const char *format, int sleep)
{
	unsigned int addr;
	int imphook = 0;
	char name[APIHOOK_MAXNAME];

	addr = libsFindExportByNid(uid, library, nid);
	if(!addr)
	{
		addr = libsFindImportAddrByNid(uid, library, nid);
		imphook = 1;
	}

	sprintf(name, "Nid:0x%08X", nid);

	return _apiHookGenericCommon(addr, imphook, library, name, ret, format, sleep);
}


unsigned int apiHookByName(SceUID uid, const char *library, const char *name, void *func)
{
	unsigned int addr;

	addr = libsFindExportByName(uid, library, name);
	if(addr)
	{
		if(!apiHookAddr(find_syscall_addr(addr), func))
		{
			addr = 0;
		}
	}
	else
	{
		addr = libsFindImportAddrByName(uid, library, name);
		if(addr)
		{
			if(!apiHookImport((unsigned int*) addr, func))
			{
				addr = 0;
			}
		}
	}

	return addr;
}

unsigned int apiHookByNid(SceUID uid, const char *library, unsigned int nid, void *func)
{
	unsigned int addr;

	addr = libsFindExportByNid(uid, library, nid);
	if(addr)
	{
		if(!apiHookAddr(find_syscall_addr(addr), func))
		{
			addr = 0;
		}
	}
	else
	{
		addr = libsFindImportAddrByNid(uid, library, nid);
		if(addr)
		{
			if(!apiHookImport((unsigned int*) addr, func))
			{
				addr = 0;
			}
		}
	}

	return addr;
}
