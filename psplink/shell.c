/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * shell.c - PSPLINK kernel module shell code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/shell.c $
 * $Id: shell.c 2334 2007-11-03 16:47:05Z tyranid $
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
#include <pspdisplay.h>
#include <pspdisplay_kernel.h>
#include <pspthreadman_kernel.h>
#include <pspintrman_kernel.h>
#include <psppower.h>
#include <stdint.h>
#include <usbhostfs.h>
#include <usbasync.h>
#include "memoryUID.h"
#include "psplink.h"
#include "psplinkcnf.h"
#include "util.h"
#include "bitmap.h"
#include "config.h"
#include "shell.h"
#include "version.h"
#include "exception.h"
#include "decodeaddr.h"
#include "debug.h"
#include "libs.h"
#include "thctx.h"
#include "apihook.h"
#include "tty.h"
#include "shellcmd.h"
#include "usbshell.h"

#define MAX_SHELL_VAR      128
#define MAX_CLI            4096

extern struct GlobalContext g_context;

#define COMMAND_EVENT_DONE 1

typedef int (*threadmanprint_func)(SceUID uid, int verbose);

struct call_frame 
{
	u64 (*func)(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e, unsigned int f);
	unsigned int args[6];
};

void psplinkPrintPrompt(void)
{
	SHELL_PRINT_CMD(SHELL_CMD_CWD, "%s", g_context.currdir);
	SHELL_PRINT_CMD(SHELL_CMD_SUCCESS, "0x%08X", 0);
}

static SceUID get_module_uid(const char *name)
{
	char *endp;
	SceUID uid = -1;

	if(name[0] == '@')
	{
		uid = refer_module_by_name(&name[1], NULL);
	}
	else
	{
		uid = strtoul(name, &endp, 0);
		if(*endp != 0)
		{
			SHELL_PRINT("ERROR: Invalid uid %s\n", name);
			uid = -1;
		}
	}

	return uid;
}

typedef int (*ReferFunc)(const char *, SceUID *, void *);

static SceUID get_thread_uid(const char *name, ReferFunc pRefer)
{
	char *endp;
	SceUID uid = -1;

	if(name[0] == '@')
	{
		if(pRefer(&name[1], &uid, NULL) < 0)
		{
			SHELL_PRINT("ERROR: Invalid name %s\n", name);
			return CMD_ERROR;
		}
	}
	else
	{
		uid = strtoul(name, &endp, 0);
		if(*endp != 0)
		{
			SHELL_PRINT("ERROR: Invalid uid %s\n", name);
			uid = -1;
		}
	}

	return uid;
}

static int threadmanlist_cmd(int argc, char **argv, enum SceKernelIdListType type, const char *name, threadmanprint_func pinfo)
{
	SceUID ids[100];
	int ret;
	int count;
	int i;
	int verbose = 0;

	if(argc > 0)
	{
		if(strcmp(argv[0], "v") == 0)
		{
			verbose = 1;
		}
	}

	memset(ids, 0, 100 * sizeof(SceUID));
	ret = sceKernelGetThreadmanIdList(type, ids, 100, &count);
	if(ret >= 0)
	{
		SHELL_PRINT("<%s List (%d entries)>\n", name, count);
		for(i = 0; i < count; i++)
		{
			if(pinfo(ids[i], verbose) < 0)
			{
				SHELL_PRINT("ERROR: Unknown %s 0x%08X\n", name, ids[i]);
			}
		}
	}

	return CMD_OK;
}

static int threadmaninfo_cmd(int argc, char **argv, const char *name, threadmanprint_func pinfo, ReferFunc pRefer)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_thread_uid(argv[0], pRefer);

	if(uid >= 0)
	{
		if(pinfo(uid, 1) < 0)
		{
			SHELL_PRINT("ERROR: Unknown %s 0x%08X\n", name, uid);
		}

		ret = CMD_OK;
	}

	return ret;
}

static const char* get_thread_status(int stat, char *str)
{
	str[0] = 0;
	if(stat & PSP_THREAD_RUNNING)
	{
		strcat(str, "RUNNING ");
	}
	
	if(stat & PSP_THREAD_READY)
	{
		strcat(str, "READY ");
	}

	if(stat & PSP_THREAD_WAITING)
	{
		strcat(str, "WAITING ");
	}
	
	if(stat & PSP_THREAD_SUSPEND)
	{
		strcat(str, "SUSPEND ");
	}

	if(stat & PSP_THREAD_STOPPED)
	{
		strcat(str, "STOPPED ");
	}

	if(stat & PSP_THREAD_KILLED)
	{
		strcat(str, "KILLED ");
	}

	return str;
}

static int print_threadinfo(SceUID uid, int verbose)
{
	SceKernelThreadInfo info;
	char status[256];
	char cwd[512];
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferThreadStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("Attr: 0x%08X - Status: %d/%s- Entry: 0x%p\n", info.attr, info.status, 
					get_thread_status(info.status, status), info.entry);
			SHELL_PRINT("Stack: 0x%p - StackSize 0x%08X - GP: 0x%08X\n", info.stack, info.stackSize,
					(unsigned int) info.gpReg);
			SHELL_PRINT("InitPri: %d - CurrPri: %d - WaitType %d\n", info.initPriority,
					info.currentPriority, info.waitType);
			SHELL_PRINT("WaitId: 0x%08X - WakeupCount: %d - ExitStatus: 0x%08X\n", info.waitId,
					info.wakeupCount, info.exitStatus);
			SHELL_PRINT("RunClocks: %d - IntrPrempt: %d - ThreadPrempt: %d\n", info.runClocks.low,
					info.intrPreemptCount, info.threadPreemptCount);
			SHELL_PRINT("ReleaseCount: %d, StackFree: %d\n", info.releaseCount, sceKernelGetThreadStackFreeSize(uid));
			if(sceIoGetThreadCwd(uid, cwd, sizeof(cwd)) > 0)
			{
				SHELL_PRINT("Current Dir: %s\n", cwd);
			}
		}
	}

	return ret;
}

static int thlist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Thread, "Thread", print_threadinfo);
}

static int thsllist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_SleepThread, "Sleep Thread", print_threadinfo);
}

static int thdelist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_DelayThread, "Delay Thread", print_threadinfo);
}

static int thsulist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_SuspendThread, "Suspend Thread", print_threadinfo);
}

static int thdolist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_DormantThread, "Dormant Thread", print_threadinfo);
}

static int thinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "Thread", print_threadinfo, (ReferFunc) pspSdkReferThreadStatusByName);
}

static int thread_do_cmd(const char *name, const char *type, ReferFunc refer, int (*fn)(SceUID uid))
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;

	uid = get_thread_uid(name, refer);

	if(uid >= 0)
	{
		err = fn(uid);
		if(err < 0)
		{
			SHELL_PRINT("Cannot %s uid 0x%08X (error: 0x%08X)\n", type, uid, err);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int thsusp_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "suspend", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelSuspendThread);
}

static int thspuser_cmd(int argc, char **argv, unsigned int *vRet)
{
	sceKernelSuspendAllUserThreads();

	return CMD_OK;
}

static int thresm_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "resume", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelResumeThread);
}

static int thwake_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "wakeup", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelWakeupThread);
}

static int thterm_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "terminate", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelTerminateThread);
}

static int thdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelDeleteThread);
}

static int thtdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "terminate delete", (ReferFunc) pspSdkReferThreadStatusByName, sceKernelTerminateDeleteThread);
}

static int thctx_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "get context", (ReferFunc) pspSdkReferThreadStatusByName, threadFindContext);
}

static int thpri_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;
	int err;
	unsigned int pri;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferThreadStatusByName);

	if((uid >= 0) && (strtoint(argv[1], &pri)))
	{
		err = sceKernelChangeThreadPriority(uid, pri);
		if(err < 0)
		{
			SHELL_PRINT("Cannot %s uid 0x%08X (error: 0x%08X)\n", "change priority", uid, err);
		}

		ret = CMD_OK;
	}

	return ret;
}

static int thcreat_cmd(int argc, char **argv, unsigned int *vRet)
{
	const char *name;
	unsigned int entry;
	int pri;
	int stack;
	unsigned int attr;
	SceUID uid;
	int ret;

	name = argv[0];
	if(!memDecode(argv[1], &entry))
	{
		SHELL_PRINT("Invalid entry address %s\n", argv[1]);
		return CMD_ERROR;
	}

	pri = strtoul(argv[2], NULL, 0);
	stack = strtoul(argv[3], NULL, 0);
	attr = strtoul(argv[4], NULL, 0);

	uid = sceKernelCreateThread(name, (void*) entry, pri, stack, attr, NULL);
	if(uid < 0)
	{
		SHELL_PRINT("Could not create thread 0x%08X\n", uid);
		return CMD_ERROR;
	}

	ret = sceKernelStartThread(uid, 0, NULL);
	if(ret < 0)
	{
		SHELL_PRINT("Could not start thread %s 0x%08X\n", name, uid);
		return CMD_ERROR;
	}

	SHELL_PRINT("Created/Started thread %s UID:0x%08X\n", name, uid);
	*vRet = uid;

	return CMD_OK;
}

static int print_eventinfo(SceUID uid, int verbose)
{
	SceKernelEventFlagInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferEventFlagStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("Attr: 0x%08X - initPattern 0x%08X - currPatten 0x%08X\n", info.attr, info.initPattern, 
					info.currentPattern);
			SHELL_PRINT("NumWaitThreads: 0x%08X\n", info.numWaitThreads);
		}
	}

	return ret;
}

static int evlist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_EventFlag, "EventFlag", print_eventinfo);
}

static int evinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "EventFlag", print_eventinfo, (ReferFunc) pspSdkReferEventFlagStatusByName);
}

static int evdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferEventFlagStatusByName, sceKernelDeleteEventFlag);
}

static int evset_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	unsigned int bits;
	int ret;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferEventFlagStatusByName);
	if(uid < 0)
	{
		return CMD_ERROR;
	}

	bits = strtoul(argv[1], NULL, 0);
	ret = sceKernelSetEventFlag(uid, bits);
	if(ret < 0)
	{
		SHELL_PRINT("Error setting EventFlag - 0x%08X\n", ret);
	}

	return CMD_OK;
}

static int evclr_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	unsigned int bits;
	int ret;

	uid = get_thread_uid(argv[0], (ReferFunc) pspSdkReferEventFlagStatusByName);
	if(uid < 0)
	{
		return CMD_ERROR;
	}

	bits = strtoul(argv[1], NULL, 0);
	ret = sceKernelClearEventFlag(uid, ~bits);
	if(ret < 0)
	{
		SHELL_PRINT("Error clearing EventFlag - 0x%08X\n", ret);
	}

	return CMD_OK;
}

static int print_semainfo(SceUID uid, int verbose)
{
	SceKernelSemaInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferSemaStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("Attr: 0x%08X - initCount: 0x%08X - currCount: 0x%08X\n", info.attr, info.initCount, 
					info.currentCount);
			SHELL_PRINT("maxCount: 0x%08X - NumWaitThreads: 0x%08X\n", info.maxCount, info.numWaitThreads);
		}
	}

	return ret;
}

static int smlist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Semaphore, "Semaphore", print_semainfo);
}

static int sminfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "Semaphore", print_semainfo, (ReferFunc) pspSdkReferSemaStatusByName);
}

static int smdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferSemaStatusByName, sceKernelDeleteSema);
}

static int print_mboxinfo(SceUID uid, int verbose)
{
	SceKernelMbxInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferMbxStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("Attr: 0x%08X - numWaitThreads: 0x%08X - numMessages: 0x%08X\n", info.attr, info.numWaitThreads, 
					info.numMessages);
			SHELL_PRINT("firstMessage 0x%p\n", info.firstMessage);
		}
	}

	return ret;
}

static int mxlist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Mbox, "Message Box", print_mboxinfo);
}

static int mxinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "Message Box", print_mboxinfo, (ReferFunc) pspSdkReferMboxStatusByName);
}

static int mxdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferMboxStatusByName, sceKernelDeleteMbx);
}

static int print_cbinfo(SceUID uid, int verbose)
{
	SceKernelCallbackInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferCallbackStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("threadId 0x%08X - callback 0x%p - common 0x%p\n", info.threadId, info.callback, info.common);
			SHELL_PRINT("notifyCount %d - notifyArg %d\n", info.notifyCount, info.notifyArg);
		}
	}

	return ret;
}

static int cblist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Callback, "Callback", print_cbinfo);
}

static int cbinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "Callback", print_cbinfo, (ReferFunc) pspSdkReferCallbackStatusByName);
}

static int cbdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferCallbackStatusByName, sceKernelDeleteCallback);
}

static int print_vtinfo(SceUID uid, int verbose)
{
	SceKernelVTimerInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferVTimerStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("active %d - base.hi %d - base.low %d - current.hi %d - current.low %d\n", 
				   info.active, info.base.hi, info.base.low, info.current.hi, info.current.low);	
			SHELL_PRINT("schedule.hi %d - schedule.low %d - handler 0x%p - common 0x%p\n", info.schedule.hi,
					info.schedule.low, info.handler, info.common);
		}
	}

	return ret;
}

static int vtlist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_VTimer, "VTimer", print_vtinfo);
}

static int vtinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "VTimer", print_vtinfo, (ReferFunc) pspSdkReferVTimerStatusByName);
}

static int vtdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferVTimerStatusByName, sceKernelDeleteVTimer);
}

static int print_vplinfo(SceUID uid, int verbose)
{
	SceKernelVplInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferVplStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("Attr 0x%08X - poolSize %d - freeSize %d - numWaitThreads %d\n",
					info.attr, info.poolSize, info.freeSize, info.numWaitThreads);
		}
	}

	return ret;
}

static int vpllist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Vpl, "Vpl", print_vplinfo);
}

static int vplinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "Vpl", print_vplinfo, (ReferFunc) pspSdkReferVplStatusByName);
}

static int vpldel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferVplStatusByName, sceKernelDeleteVpl);
}

static int print_fplinfo(SceUID uid, int verbose)
{
	SceKernelFplInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferFplStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("Attr 0x%08X - blockSize %d - numBlocks %d - freeBlocks %d - numWaitThreads %d\n",
					info.attr, info.blockSize, info.numBlocks, info.freeBlocks, info.numWaitThreads);
		}
	}

	return ret;
}

static int fpllist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Fpl, "Fpl", print_fplinfo);
}

static int fplinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "Fpl", print_fplinfo, (ReferFunc) pspSdkReferFplStatusByName);
}

static int fpldel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferFplStatusByName, sceKernelDeleteFpl);
}

static int print_mppinfo(SceUID uid, int verbose)
{
	SceKernelMppInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferMsgPipeStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("Attr 0x%08X - bufSize %d - freeSize %d\n", info.attr, info.bufSize, info.freeSize);
			SHELL_PRINT("numSendWaitThreads %d - numReceiveWaitThreads %d\n", info.numSendWaitThreads,
					info.numReceiveWaitThreads);
		}
	}

	return ret;
}

static int mpplist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_Mpipe, "Message Pipe", print_mppinfo);
}

static int mppinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "Message Pipe", print_mppinfo, (ReferFunc) pspSdkReferMppStatusByName);
}

static int mppdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferMppStatusByName, sceKernelDeleteMsgPipe);
}

static int print_thevinfo(SceUID uid, int verbose)
{
	SceKernelThreadEventHandlerInfo info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	ret = sceKernelReferThreadEventHandlerStatus(uid, &info);
	if(ret == 0)
	{
		SHELL_PRINT("UID: 0x%08X - Name: %s\n", uid, info.name);
		if(verbose)
		{
			SHELL_PRINT("threadId 0x%08X - mask %02X - handler 0x%p\n", info.threadId, info.mask, info.handler);
			SHELL_PRINT("common 0x%p\n", info.common);
		}
	}

	return ret;
}

static int thevlist_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmanlist_cmd(argc, argv, SCE_KERNEL_TMID_ThreadEventHandler, "Thread Event Handler", print_thevinfo);
}

static int thevinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	return threadmaninfo_cmd(argc, argv, "Thread Event Handler", print_thevinfo, (ReferFunc) pspSdkReferThreadEventHandlerStatusByName);
}

static int thevdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "delete", (ReferFunc) pspSdkReferThreadEventHandlerStatusByName, sceKernelReleaseThreadEventHandler);
}

int thread_event_handler(int mask, SceUID thid, void *common)
{
	const char *event = "Unknown";
	const char *thname = "Unknown";
	SceKernelThreadInfo thinfo;

	switch(mask)
	{
		case THREAD_CREATE: event = "Create";
							break;
		case THREAD_START: event = "Start";
						   break;
		case THREAD_EXIT: event = "Exit";
						  break;
		case THREAD_DELETE: event = "Delete";
							break;
		default: break;
	};

	memset(&thinfo, 0, sizeof(thinfo));
	thinfo.size = sizeof(thinfo);
	if(sceKernelReferThreadStatus(thid, &thinfo) == 0)
	{
		thname = thinfo.name;
	}

	SHELL_PRINT("Thread %-6s: thid 0x%08X name %s\n", event, thid, thname);

	return 0;
}

static int thmon_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID ev;
	SceUID uid;
	int mask = 0;

	if(g_context.thevent >= 0)
	{
		sceKernelReleaseThreadEventHandler(g_context.thevent);
		g_context.thevent = -1;
	}

	switch(argv[0][0])
	{
		case 'a': uid = THREADEVENT_ALL;
				  break;
		case 'u': uid = THREADEVENT_USER;
				  break;
		case 'k': uid = THREADEVENT_KERN;
				  break;
		default: return CMD_ERROR;
	};

	if(argc > 1)
	{
		int loop;

		for(loop = 0; argv[1][loop]; loop++)
		{
			switch(argv[1][loop])
			{
				case 'c': mask |= THREAD_CREATE;
						  break;
				case 's': mask |= THREAD_START;
						  break;
				case 'e': mask |= THREAD_EXIT;
						  break;
				case 'd': mask |= THREAD_DELETE;
						  break;
				default: /* Do nothing */
						  break;
			};
		}
	}
	else
	{
		mask = THREAD_CREATE | THREAD_START | THREAD_EXIT | THREAD_DELETE;
	}

	ev = sceKernelRegisterThreadEventHandler("PSPLINK_THEV", uid, mask, thread_event_handler, NULL);
	g_context.thevent = ev;

	return CMD_OK;
}

static int thmonoff_cmd(int argc, char **argv, unsigned int *vRet)
{
	if(g_context.thevent >= 0)
	{
		sceKernelReleaseThreadEventHandler(g_context.thevent);
		g_context.thevent = -1;
	}

	return CMD_OK;
}

static int sysstat_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceKernelSystemStatus stat;

	memset(&stat, 0, sizeof(stat));
	stat.size = sizeof(stat);

	if(!sceKernelReferSystemStatus(&stat))
	{
		SHELL_PRINT("System Status: 0x%08X\n", stat.status);
		SHELL_PRINT("Idle Clocks:   %08X%08X\n", stat.idleClocks.hi, stat.idleClocks.low);
		SHELL_PRINT("Resume Count:  %d\n", stat.comesOutOfIdleCount);
		SHELL_PRINT("Thread Switch: %d\n", stat.threadSwitchCount);
		SHELL_PRINT("VFPU Switch:   %d\n", stat.vfpuSwitchCount);
	}

	return CMD_OK;
}

static int uidlist_cmd(int argc, char **argv, unsigned int *vRet)
{
	const char *name = NULL;

	if(argc > 0)
	{
		name = argv[0];
	}
	printUIDList(name);

	return CMD_OK;
}

static int uidinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	uidControlBlock *entry;
	const char *parent = NULL;

	if(argc > 1)
	{
		parent = argv[1];
	}

	if(argv[0][0] == '@')
	{
		entry = findObjectByNameWithParent(&argv[0][1], parent);
	}
	else
	{
		SceUID uid;

		uid = strtoul(argv[0], NULL, 0);
		entry = findObjectByUIDWithParent(uid, parent);
	}

	if(entry)
	{
		printUIDEntry(entry);
		if(entry->type)
		{
			SHELL_PRINT("Parent:\n");
			printUIDEntry(entry->type);
		}
	}

	return CMD_OK;
}

static int cop0_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int regs[64];
	int i;

	psplinkGetCop0(regs);

	SHELL_PRINT("MXC0 Regs:\n");
	for(i = 0; i < 32; i += 2)
	{
		SHELL_PRINT("$%02d: 0x%08X  -  $%02d: 0x%08X\n", i, regs[i], i+1, regs[i+1]);
	}
	SHELL_PRINT("\n");

	SHELL_PRINT("CXC0 Regs:\n");
	for(i = 0; i < 32; i += 2)
	{
		SHELL_PRINT("$%02d: 0x%08X  -  $%02d: 0x%08X\n", i, regs[i+32], i+1, regs[i+33]);
	}

	return CMD_OK;
}

static void print_modthids(SceUID uid, int verbose, const char *name, int type, int (*print)(SceUID, int))
{
	SceUID thids[100];
	int count;

	count = psplinkReferThreadsByModule(type, uid, thids, 100);
	if(count > 0)
	{
		int i;
		SHELL_PRINT("Module %s (%d)\n", name, count);
		for(i = 0; i < count; i++)
		{
			(void) print(thids[i], verbose);
			SHELL_PRINT("\n");
		}
	}
}

static void print_modthreads(SceUID uid, int verbose)
{
	print_modthids(uid, verbose, "Thread", SCE_KERNEL_TMID_Thread, print_threadinfo);
}

static void print_modcallbacks(SceUID uid, int verbose)
{
	print_modthids(uid, verbose, "Callback", SCE_KERNEL_TMID_Callback, print_cbinfo);
}

static int print_modinfo(SceUID uid, int verbose, const char *opts)
{
	SceKernelModuleInfo info;
	int ret;

	enable_kprintf(0);
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	ret = g_QueryModuleInfo(uid, &info);
	if(ret >= 0)
	{
		int i;
		SHELL_PRINT("UID: 0x%08X Attr: %04X - Name: %s\n", uid, info.attribute, info.name);
		if(verbose)
		{
			SHELL_PRINT("Entry: 0x%08X - GP: 0x%08X - TextAddr: 0x%08X\n", info.entry_addr,
					info.gp_value, info.text_addr);
			SHELL_PRINT("TextSize: 0x%08X - DataSize: 0x%08X BssSize: 0x%08X\n", info.text_size,
					info.data_size, info.bss_size);
			for(i = 0; (i < info.nsegment) && (i < 4); i++)
			{
				SHELL_PRINT("Segment %d: Addr 0x%08X - Size 0x%08X\n", i, 
						(unsigned int) info.segmentaddr[i], (unsigned int) info.segmentsize[i]);
			}
			SHELL_PRINT("\n");
		}

		while(*opts)
		{
			switch(*opts)
			{
				case 't': /* Print threads for this module */
						  print_modthreads(uid, verbose);
						  break;
				case 'c': /* Print callbacks for this module */
						  print_modcallbacks(uid, verbose);
						  break;
				default: break;
			};
			opts++;
		}
	}
	else
	{
		SHELL_PRINT("Error querying module 0x%08X\n", ret);
	}

	enable_kprintf(1);

	return ret;
}

static int modinfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;
	const char *opts = "";

	uid = get_module_uid(argv[0]);

	if(uid >= 0)
	{
		if(argc > 1)
		{
			opts = argv[1];
		}

		if(print_modinfo(uid, 1, opts) < 0)
		{
			SHELL_PRINT("ERROR: Unknown module 0x%08X\n", uid);
		}
		else
		{
			ret = CMD_OK;
		}
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid module %s\n", argv[0]);
	}

	return ret;
}

static int modlist_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID ids[100];
	int ret;
	int count;
	int i;
	int verbose = 0;
	const char *opts = "";

	if(argc > 0)
	{
		opts = argv[0];
		while(*opts)
		{
			if(*opts == 'v')
			{
				verbose = 1;
				break;
			}
			opts++;
		}
		opts = argv[0];
	}

	memset(ids, 0, 100 * sizeof(SceUID));
	ret = g_GetModuleIdList(ids, 100 * sizeof(SceUID), &count);
	if(ret >= 0)
	{
		SHELL_PRINT("<Module List (%d modules)>\n", count);
		for(i = 0; i < count; i++)
		{
			print_modinfo(ids[i], verbose, opts);
		}
	}

	return CMD_OK;
}

static int modstop_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		SceUID uid_ret;
		int status;

		uid_ret = sceKernelStopModule(uid, 0, NULL, &status, NULL);
		SHELL_PRINT("Module Stop 0x%08X Status 0x%08X\n", uid_ret, status);

		ret = CMD_OK;
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modunld_cmd(int argc, char **argv, unsigned int *vRet)
{

	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		SceUID uid_ret;

		uid_ret = sceKernelUnloadModule(uid);
		SHELL_PRINT("Module Unload 0x%08X\n", uid_ret);

		ret = CMD_OK;
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;

}

static int modstun_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		SceUID stop, unld;
		int status;

		stop = sceKernelStopModule(uid, 0, NULL, &status, NULL);
		unld = sceKernelUnloadModule(uid);
		SHELL_PRINT("Module Stop/Unload 0x%08X/0x%08X Status 0x%08X\n", stop, unld, status);

		ret = CMD_OK;
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modstart_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;
	char args[1024];
	int  len;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		SceUID uid_ret;
		int status;

		if(argc > 1)
		{
			len = build_args(args, argv[1], argc - 2, &argv[2]);
		}
		else
		{
			len = build_args(args, "unknown", 0, NULL);
		}

		uid_ret = sceKernelStartModule(uid, len, args, &status, NULL);
		SHELL_PRINT("Module Start 0x%08X Status 0x%08X\n", uid_ret, status);

		ret = CMD_OK;
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modexp_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		if(libsPrintEntries(uid))
		{
			ret = CMD_OK;
		}
		else
		{
			SHELL_PRINT("ERROR: Couldn't find module %s\n", argv[0]);
		}
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modimp_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		if(libsPrintImports(uid))
		{
			ret = CMD_OK;
		}
		else
		{
			SHELL_PRINT("ERROR: Couldn't find module %s\n", argv[0]);
		}
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int modfindx_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;
	unsigned int addr = 0;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		if(argv[2][0] == '@')
		{
			addr = libsFindExportByName(uid, argv[1], &argv[2][1]);
		}
		else
		{
			char *endp;
			unsigned int nid;

			nid = strtoul(argv[2], &endp, 16);
			if(*endp != 0)
			{
				SHELL_PRINT("ERROR: Invalid nid %s\n", argv[2]);
			}
			else
			{
				addr = libsFindExportByNid(uid, argv[1], nid);
			}
		}
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid argument %s\n", argv[0]);
	}

	if(addr != 0)
	{
		SHELL_PRINT("Library: %s, Exp %s, Addr: 0x%08X\n", argv[1], argv[2], addr);

		ret = CMD_OK;
	}
	else
	{
		SHELL_PRINT("Couldn't find module export\n");
	}

	return ret;
}

static int modfindi_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;
	unsigned int addr = 0;

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		if(argv[2][0] == '@')
		{
			addr = libsFindImportAddrByName(uid, argv[1], &argv[2][1]);
		}
		else
		{
			char *endp;
			unsigned int nid;

			nid = strtoul(argv[2], &endp, 16);
			if(*endp != 0)
			{
				SHELL_PRINT("ERROR: Invalid nid %s\n", argv[2]);
			}
			else
			{
				addr = libsFindImportAddrByNid(uid, argv[1], nid);
			}
		}
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid argument %s\n", argv[0]);
	}

	if(addr != 0)
	{
		SHELL_PRINT("Library: %s, Imp %s, Addr: 0x%08X\n", argv[1], argv[2], addr);

		ret = CMD_OK;
	}
	else
	{
		SHELL_PRINT("Couldn't find module import\n");
	}

	return ret;
}

static int apihook_common(int argc, char **argv, int sleep)
{
	SceUID uid;
	int ret = CMD_ERROR;
	const char *param = "";

	if(argc > 4)
	{
		param = argv[4];
	}

	uid = get_module_uid(argv[0]);
	if(uid >= 0)
	{
		if(argv[2][0] == '@')
		{
			if(apiHookGenericByName(uid, argv[1], &argv[2][1], argv[3][0], param, sleep))
			{
				ret = CMD_OK;
			}
		}
		else
		{
			char *endp;
			unsigned int nid;

			nid = strtoul(argv[2], &endp, 16);
			if(*endp != 0)
			{
				SHELL_PRINT("ERROR: Invalid nid %s\n", argv[2]);
			}
			else
			{
				if(apiHookGenericByNid(uid, argv[1], nid, argv[3][0], param, 0))
				{
					ret = CMD_OK;
				}
			}
		}
	}
	else
	{
		SHELL_PRINT("ERROR: Invalid argument %s\n", argv[0]);
	}

	return ret;
}

static int apihook_cmd(int argc, char **argv, unsigned int *vRet)
{
	return apihook_common(argc, argv, 0);
}

static int apihooks_cmd(int argc, char **argv, unsigned int *vRet)
{
	return apihook_common(argc, argv, 1);
}

static int apihp_cmd(int argc, char **argv, unsigned int *vRet)
{
	apiHookGenericPrint();
	return CMD_OK;
}

static int apihd_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int id;

	if(strtoint(argv[0], &id))
	{
		apiHookGenericDelete(id);
	}
	else
	{
		SHELL_PRINT("Invalid ID for delete\n");
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int modload_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID modid;
	SceKernelModuleInfo info;
	char path[1024];

	if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
	{
		modid = sceKernelLoadModule(path, 0, NULL);
		if(!psplinkReferModule(modid, &info))
		{
			SHELL_PRINT("Module Load '%s' UID: 0x%08X\n", path, modid);
		}
		else
		{
			SHELL_PRINT("Module Load '%s' UID: 0x%08X Name: %s\n", path, modid, info.name);
		}
		*vRet = modid;
	}
	else
	{
		SHELL_PRINT("Error invalid file %s\n", path);
	}

	return CMD_OK;
}

static int modexec_cmd(int argc, char **argv, unsigned int *vRet)
{
	char path[1024];
	char args[1024];
	char *file = NULL;
	char *key = NULL;
	int  len;
	struct SceKernelLoadExecParam le;

	if(argv[0][0] == '@')
	{
		key = &argv[0][1];
		if(argc < 2)
		{
			return CMD_ERROR;
		}
		file = argv[1];
	}
	else
	{
		file = argv[0];
	}

	if(handlepath(g_context.currdir, file, path, TYPE_FILE, 1))
	{
		len = build_args(args, path, argc - 1, &argv[1]);
		le.size = sizeof(le);
		le.args = len;
		le.argp = args;
		le.key = key;

		psplinkStop();

		sceKernelLoadExec(path, &le);
	}

	return CMD_OK;
}

static int ldstart_cmd(int argc, char **argv, unsigned int *vRet)
{
	char path[MAXPATHLEN];
	SceKernelModuleInfo info;
	int ret = CMD_ERROR;

	if(argc > 0)
	{
		SceUID modid;

		if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
		{
			modid = load_start_module(path, argc-1, &argv[1]);
			if(modid >= 0)
			{
				if(!psplinkReferModule(modid, &info))
				{
					SHELL_PRINT("Load/Start %s UID: 0x%08X\n", path, modid);
				}
				else
				{
					SHELL_PRINT("Load/Start %s UID: 0x%08X Name: %s\n", path, modid, info.name);
				}
			}
			else
			{
				SHELL_PRINT("Failed to Load/Start module '%s' Error: 0x%08X\n", path, modid);
			}
			*vRet = modid;

			ret = CMD_OK;
		}
		else
		{
			SHELL_PRINT("Error invalid file %s\n", path);
		}
	}

	return ret;
}

static int kill_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	int ret = CMD_ERROR;

	do
	{
		uid = get_module_uid(argv[0]);
		if(uid >= 0)
		{
			SceUID thids[100];
			int count;
			int i;
			int status;
			int error;

			error = sceKernelStopModule(uid, 0, NULL, &status, NULL);
			if(error < 0)
			{
				SHELL_PRINT("Error could not stop module 0x%08X\n", error);
				break;
			}

			SHELL_PRINT("Stop status %08X\n", status);

			count = psplinkReferThreadsByModule(SCE_KERNEL_TMID_Thread, uid, thids, 100);
			for(i = 0; i < count; i++)
			{
				sceKernelTerminateDeleteThread(thids[i]);
			}

			if(sceKernelUnloadModule(uid) < 0)
			{
				SHELL_PRINT("Error could not unload module\n");
				break;
			}

			ret = CMD_OK;
		}
	}
	while(0);

	return ret;
}

static int debug_cmd(int argc, char **argv, unsigned int *vRet)
{
	char path[1024];
	int  ret = CMD_ERROR;

	if(handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 1))
	{
		if(g_context.gdb == 0)
		{
			argv[0] = path;
			load_gdb(g_context.bootpath, argc, argv);
		}
		else
		{
			SHELL_PRINT("Error GDB already running, please reset\n");
		}

		ret = CMD_OK;
	}
	else
	{
		SHELL_PRINT("Error invalid file %s\n", path);
	}

	return ret;
}

static int calc_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int val;
	char disp;

	if(memDecode(argv[0], &val))
	{
		if(argc > 1)
		{
			disp = upcase(argv[1][0]);
		}
		else
		{
			disp = 'X';
		}

		switch(disp)
		{
			case 'D': SHELL_PRINT("%d\n", val);
					  break;
			case 'O': SHELL_PRINT("%o\n", val);
					  break;
			default :
			case 'X': SHELL_PRINT("0x%08X\n", val);
					  break;
		};
		*vRet = val;
	}
	else
	{
		SHELL_PRINT("Error could not calculate address\n");
	}

	return CMD_OK;
}

static int reset_cmd(int argc, char **argv, unsigned int *vRet)
{
	if(argc > 0)
	{
		if(strcmp(argv[0], "game") == 0)
		{
			g_context.rebootkey = REBOOT_MODE_GAME;
		}
		else if(strcmp(argv[0], "vsh") == 0)
		{
			g_context.rebootkey = REBOOT_MODE_VSH;
		}
		else if(strcmp(argv[0], "updater") == 0)
		{
			g_context.rebootkey = REBOOT_MODE_UPDATER;
		}
	}

	psplinkReset();

	return CMD_OK;
}

static int list_dir(const char *name)
{
	char buffer[512];
	char *p = buffer;
	int dfd;
	static SceIoDirent dir;

	dfd = sceIoDopen(name);
	if(dfd >= 0)
	{
		memset(&dir, 0, sizeof(dir));
		while(sceIoDread(dfd, &dir) > 0)
		{
			int ploop;
			p = buffer;

			if(FIO_SO_ISDIR(dir.d_stat.st_attr) || FIO_S_ISDIR(dir.d_stat.st_mode))
			{
				*p++ = 'd';
			}
			else
			{
				*p++ = '-';
			}

			for(ploop = 2; ploop >= 0; ploop--)
			{
				int bits;

				bits = (dir.d_stat.st_mode >> (ploop * 3)) & 0x7;
				if(bits & 4)
				{
					*p++ = 'r';
				}
				else
				{
					*p++ = '-';
				}

				if(bits & 2)
				{
					*p++ = 'w';
				}
				else
				{
					*p++ = '-';
				}

				if(bits & 1)
				{
					*p++ = 'x';
				}
				else
				{
					*p++ = '-';
				}
			}

			sprintf(p, " %8d ", (int) dir.d_stat.st_size);
			p += strlen(p);
			sprintf(p, "%02d-%02d-%04d %02d:%02d ", dir.d_stat.st_mtime.day, 
					dir.d_stat.st_mtime.month, dir.d_stat.st_mtime.year,
					dir.d_stat.st_mtime.hour, dir.d_stat.st_mtime.minute);
			p += strlen(p);
			sprintf(p, "%s", dir.d_name);
			SHELL_PRINT("%s\n", buffer);
			memset(&dir, 0, sizeof(dir));
		}

		sceIoDclose(dfd);
	}
	else
	{
		SHELL_PRINT("Could not open directory '%s'\n", name);
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int ls_cmd(int argc, char **argv, unsigned int *vRet)
{
	char path[1024];

	if(argc == 0)
	{
		SHELL_PRINT("Listing directory %s\n", g_context.currdir);
		list_dir(g_context.currdir);
	}
	else
	{
		int loop;
		/* Strip whitespace and append a final slash */

		for(loop = 0; loop < argc; loop++)
		{
			if(handlepath(g_context.currdir, argv[loop], path, TYPE_DIR, 1))
			{
				SHELL_PRINT("Listing directory %s\n", path);
				list_dir(path);
			}
		}
	}

	return CMD_OK;
}

static int chdir_cmd(int argc, char **argv, unsigned int *vRet)
{
	char *dir;
	int ret = CMD_ERROR;
	char path[1024];

	/* Get remainder of string */
	dir = argv[0];
	/* Strip whitespace and append a final slash */

	if(handlepath(g_context.currdir, dir, path, TYPE_DIR, 1) == 0)
	{
		SHELL_PRINT("'%s' not a valid directory\n", dir);
	}
	else
	{
		strcpy(g_context.currdir, path);
		SHELL_PRINT_CMD(SHELL_CMD_CWD, "%s", g_context.currdir);
		ret = CMD_OK;
	}

	return ret;
}

static int pwd_cmd(int argc, char **argv, unsigned int *vRet)
{
	SHELL_PRINT("%s\n", g_context.currdir);

	return CMD_OK;
}

static int usbstat_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int state;

	state = sceUsbGetState();
	SHELL_PRINT("USB Status:\n");
	SHELL_PRINT("Connection    : %s\n", state & PSP_USB_ACTIVATED ? "activated" : "deactivated");
	SHELL_PRINT("USB Cable     : %s\n", state & PSP_USB_CABLE_CONNECTED ? "connected" : "disconnected");
	SHELL_PRINT("USB Connection: %s\n", state & PSP_USB_ACTIVATED ? "established" : "notpresent");

	return CMD_OK;
}

static int rename_cmd(int argc, char **argv, unsigned int *vRet)
{
	char asrc[MAXPATHLEN], adst[MAXPATHLEN];
	char *src, *dst;

	src = argv[0];
	dst = argv[1];

	if( !handlepath(g_context.currdir, src, asrc, TYPE_FILE, 1) )
		return CMD_ERROR;

	if( !handlepath(g_context.currdir, dst, adst, TYPE_FILE, 0) )
		return CMD_ERROR;

	if( sceIoRename(asrc, adst) < 0)
		return CMD_ERROR;

	SHELL_PRINT("rename %s -> %s\n", asrc, adst);

	return CMD_OK;
}

static int rm_cmd(int argc, char **argv, unsigned int *vRet)
{
	char *file, afile[MAXPATHLEN];
	int i;

	for(i = 0; i < argc; i++)
	{
		file = argv[0];

		if( !handlepath(g_context.currdir, file, afile, TYPE_FILE, 1) )
			continue;

		if( sceIoRemove(afile) < 0 )
			continue;

		SHELL_PRINT("rm %s\n", afile);
	}

	return CMD_OK;
}

static int mkdir_cmd(int argc, char **argv, unsigned int *vRet)
{
	char *file, afile[MAXPATHLEN];

	file = argv[0];

	if( !handlepath(g_context.currdir, file, afile, TYPE_FILE, 0) )
		return CMD_ERROR;

	if( sceIoMkdir(afile, 0777) < 0 )
		return CMD_ERROR;

	SHELL_PRINT("mkdir %s\n", afile);

	return CMD_OK;
}

static int rmdir_cmd(int argc, char **argv, unsigned int *vRet)
{
	char *file, afile[MAXPATHLEN];

	file = argv[0];

	if( !handlepath(g_context.currdir, file, afile, TYPE_FILE, 0) )
		return CMD_ERROR;

	if( sceIoRmdir(afile) < 0 )
		return CMD_ERROR;

	SHELL_PRINT("rmdir %s\n", afile);

	return CMD_OK;
}

static int cp_cmd(int argc, char **argv, unsigned int *vRet)
{
	int in, out;
	int n;
	char *source;
	char *destination;
	char *slash;

	char fsrc[MAXPATHLEN];
	char fdst[MAXPATHLEN];
	char buff[2048];

	source = argv[0];
	destination = argv[1];

	if( !handlepath(g_context.currdir, source, fsrc, TYPE_FILE, 1) )
		return CMD_ERROR;
	
	if( !handlepath(g_context.currdir, destination, fdst, TYPE_ETHER, 0) )
		return CMD_ERROR;

	if(isdir(fdst))
	{
		int len;

		len = strlen(fdst);
		if((len > 0) && (fdst[len-1] != '/'))
		{
			strcat(fdst, "/");
		}

		slash = strrchr(fsrc, '/');
		strcat(fdst, slash+1);
	}

	SHELL_PRINT("cp %s -> %s\n", fsrc, fdst);

	in = sceIoOpen(fsrc, PSP_O_RDONLY, 0777);
	if(in < 0)
	{
		SHELL_PRINT("Couldn't open source file %s, 0x%08X\n", fsrc, in);
		return CMD_ERROR;
	}

	out = sceIoOpen(fdst, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if(out < 0)
	{
		sceIoClose(in);
		SHELL_PRINT("Couldn't open destination file %s, 0x%08X\n", fdst, out);
		return CMD_ERROR;
	}

	while(1) {
		n = sceIoRead(in, buff, 2048);

		if(n <= 0)
			break;
		
		sceIoWrite(out, buff, n);
	}
	
	sceIoClose(in);
	sceIoClose(out);

	return CMD_OK;
}

static int remap_cmd(int argc, char **argv, unsigned int *vRet)
{
	int ret;

	sceIoUnassign(argv[1]);

	ret = sceIoAssign(argv[1], argv[0], NULL, IOASSIGN_RDWR, NULL, 0);
	if(ret < 0)
	{
		SHELL_PRINT("Error remapping %s to %s, %08X\n", argv[0], argv[1], ret);
	}

	return CMD_OK;
}

static int meminfo_cmd(int argc, char **argv, unsigned int *vRet)
{
	int i;
	int pid = 1;
	int max = 11;

	if(argc > 0)
	{
		pid = atoi(argv[0]);
		SHELL_PRINT("pid: %d\n", pid);
		if((pid < 1) || (pid > max))
		{
			SHELL_PRINT("Error, invalid partition number %d\n", pid);
			return CMD_ERROR;
		}
		max = pid + 1;
	}

	SHELL_PRINT("Memory Partitions:\n");
	SHELL_PRINT("N  |    BASE    |   SIZE   | TOTALFREE |  MAXFREE  | ATTR |\n");
	SHELL_PRINT("---|------------|----------|-----------|-----------|------|\n");
	for(i = pid; i <= max; i++)
	{
		SceSize total;
		SceSize free;
		PspSysmemPartitionInfo info;

		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		if(sceKernelQueryMemoryPartitionInfo(i, &info) == 0)
		{
			free = sceKernelPartitionMaxFreeMemSize(i);
			total = sceKernelPartitionTotalFreeMemSize(i);
			SHELL_PRINT("%-2d | 0x%08X | %8d | %9d | %9d | %04X |\n", 
					i, info.startaddr, info.memsize, total, free, info.attr);
		}
	}

	return CMD_OK;
}

static int memreg_cmd(int argc, char **argv, unsigned int *vRet)
{
	memPrintRegions();
	return CMD_OK;
}

static int memblocks_cmd(int argc, char **argv, unsigned int *vRet)
{
	if(argc > 0)
	{
		switch(argv[0][0])
		{
			case 'f': sceKernelSysMemDump();
					  break;
			case 't': sceKernelSysMemDumpTail();
					  break;
			default: return CMD_ERROR;
					 
		};
	}
	else
	{
		sceKernelSysMemDumpBlock();
	}

	return CMD_OK;
}

static int malloc_cmd(int argc, char **argv, unsigned int *vRet)
{
	int pid;
	const char *name;
	int type;
	int size;
	SceUID uid;

	pid = strtoul(argv[0], NULL, 0);
	name = argv[1];
	if(argv[2][0] == 'h')
	{
		type = PSP_SMEM_High;
	}
	else
	{
		type = PSP_SMEM_Low;
	}
	size = strtoul(argv[3], NULL, 0);
	uid = sceKernelAllocPartitionMemory(pid, name, type, size, NULL);
	if(uid < 0)
	{
		SHELL_PRINT("Error allocating memory in pid:%d\n", pid);
		return CMD_ERROR;
	}

	SHELL_PRINT("Allocated %d bytes in pid:%d (UID:0x%08X, Head:0x%08X)\n", size, pid,
			uid, (unsigned int) sceKernelGetBlockHeadAddr(uid));
	*vRet = uid;

	return CMD_OK;
}

int mfree_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;

	uid = strtoul(argv[0], NULL, 0);
	if(sceKernelFreePartitionMemory(uid) < 0)
	{
		SHELL_PRINT("Could not free memory block 0x%08X\n", uid);
	}

	return CMD_OK;
}

int mhead_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID uid;
	void *p;

	uid = strtoul(argv[0], NULL, 0);
	p = sceKernelGetBlockHeadAddr(uid);
	if(p == NULL)
	{
		SHELL_PRINT("Could not get head of memory block 0x%08X\n", uid);
	}
	else
	{
		SHELL_PRINT("Head:0x%08X\n", (unsigned int) p);
		*vRet = (unsigned int) p;
	}

	return CMD_OK;
}

/* Maximum memory dump size (per screen) */
#define MAX_MEMDUMP_SIZE 256
#define MEMDUMP_TYPE_BYTE 1
#define MEMDUMP_TYPE_HALF 2
#define MEMDUMP_TYPE_WORD 3

/* Print a row of a memory dump, up to row_size */
static void print_row(const unsigned int* row, s32 row_size, unsigned int addr, int type)
{
	char buffer[128];
	char *p = buffer;
	int i = 0;

	sprintf(p, "%08x - ", addr);
	p += strlen(p);

	if(type == MEMDUMP_TYPE_WORD)
	{
		for(i = 0; i < 16; i+=4)
		{
			if(i < row_size)
			{
				sprintf(p, "%02X%02X%02X%02X ", row[i+3], row[i+2], row[i+1], row[i]);
			}
			else
			{
				sprintf(p, "-------- ");
			}
			p += strlen(p);
		}
	}
	else if(type == MEMDUMP_TYPE_HALF)
	{
		for(i = 0; i < 16; i+=2)
		{
			if(i < row_size)
			{
				sprintf(p, "%02X%02X ", row[i+1], row[i]);
			}
			else
			{
				sprintf(p, "---- ");
			}

			p += strlen(p);
		}
	}
	else
	{
		for(i = 0; i < 16; i++)
		{
			if(i < row_size)
			{
				sprintf(p, "%02X ", row[i]);
			}
			else
			{
				sprintf(p, "-- ");
			}

			p += strlen(p);
		}
	}

	sprintf(p, "- ");
	p += strlen(p);

	for(i = 0; i < 16; i++)
	{
		if(i < row_size)
		{
			if((row[i] >= 32) && (row[i] < 127))
			{
				*p++ = row[i];
			}
			else
			{
				*p++ =  '.';
			}
		}
		else
		{
			*p++ = '.';
		}
	}
	*p = 0;

	SHELL_PRINT("%s\n", buffer);
}

/* Print a memory dump to SIO */
static void print_memdump(unsigned int addr, s32 size, int type)
{
	int size_left;
	unsigned int row[16];
	int row_size;
	u8 *p_addr = (u8 *) addr;

	if(type == MEMDUMP_TYPE_WORD)
	{
		SHELL_PRINT("         - 00       04       08       0c       - 0123456789abcdef\n");
		SHELL_PRINT("-----------------------------------------------------------------\n");
	}
	else if(type == MEMDUMP_TYPE_HALF)
	{
		SHELL_PRINT("         - 00   02   04   06   08   0a   0c   0e   - 0123456789abcdef\n");
		SHELL_PRINT("---------------------------------------------------------------------\n");
	}
	else 
	{
		SHELL_PRINT("         - 00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f - 0123456789abcdef\n");
		SHELL_PRINT("-----------------------------------------------------------------------------\n");
	}

	size_left = size > MAX_MEMDUMP_SIZE ? MAX_MEMDUMP_SIZE : size;
	row_size = 0;

	while(size_left > 0)
	{
		row[row_size] = p_addr[row_size];
		row_size++;
		if(row_size == 16)
		{
			// draw row
			print_row(row, row_size, (unsigned int) p_addr, type);
			p_addr += 16;
			row_size = 0;
		}

		size_left--;
	}
}

static int memdump_cmd(int argc, char **argv, unsigned int *vRet)
{
	static unsigned int addr = 0;
	static int type = MEMDUMP_TYPE_BYTE;
	s32 size_left;

	/* Get memory address */
	if(argc > 0)
	{
		if(argv[0][0] == '-')
		{
			addr -= MAX_MEMDUMP_SIZE;
		}
		else
		{
			if(!memDecode(argv[0], &addr))
			{
				SHELL_PRINT("Error, invalid memory address %s\n", argv[0]);
				return CMD_ERROR;
			}
		}

		if(argc > 1)
		{
			if(argv[1][0] == 'w')
			{
				type = MEMDUMP_TYPE_WORD;
			}
			else if(argv[1][0] == 'h')
			{
				type = MEMDUMP_TYPE_HALF;
			}
			else if(argv[1][0] == 'b')
			{
				type = MEMDUMP_TYPE_BYTE;
			}
		}
	}
	else if(addr == 0)
	{
		return CMD_ERROR;
	}
	else
	{
		addr += MAX_MEMDUMP_SIZE;
	}

	size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);

	if(size_left > 0)
	{
		print_memdump(addr, size_left, type);
	}
	else
	{
		SHELL_PRINT("Invalid memory address 0x%08X\n", addr);
	}

	return CMD_OK;
}

static int savemem_cmd(int argc, char **argv, unsigned int *vRet)
{
	char path[1024];
	unsigned int addr;
	int size;
	int written;
	char *endp;

	if(!handlepath(g_context.currdir, argv[2], path, TYPE_FILE, 0))
	{
		SHELL_PRINT("Error invalid path\n");
		return CMD_ERROR;
	}

	size = strtoul(argv[1], &endp, 0);
	if(*endp != 0)
	{
		SHELL_PRINT("Size parameter invalid '%s'\n", argv[1]);
		return CMD_ERROR;
	}

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int fd;

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size > size_left ? size_left : size;
		fd = sceIoOpen(path, PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
		if(fd < 0)
		{
			SHELL_PRINT("Could not open file '%s' for writing 0x%08X\n", path, fd);
		}
		else
		{
			written = 0;
			while(written < size)
			{
				int ret;

				ret = sceIoWrite(fd, (void *) (addr + written), size - written);
				if(ret <= 0)
				{
					SHELL_PRINT("Could not write out file\n");
					break;
				}

				written += ret;
			}
			sceIoClose(fd);
		}
	}
	else
	{
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int loadmem_cmd(int argc, char **argv, unsigned int *vRet)
{
	char path[1024];
	unsigned int addr;
	int maxsize = -1;
	char *endp;

	if(!handlepath(g_context.currdir, argv[1], path, TYPE_FILE, 1))
	{
		SHELL_PRINT("Error invalid path\n");
		return CMD_ERROR;
	}

	if(argc > 2)
	{
		maxsize = strtoul(argv[2], &endp, 0);
		if(*endp != 0)
		{
			SHELL_PRINT("Size parameter invalid '%s'\n", argv[2]);
			return CMD_ERROR;
		}
	}

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int readbytes;
		int fd;

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		fd = sceIoOpen(path, PSP_O_RDONLY, 0777);
		if(fd < 0)
		{
			SHELL_PRINT("Could not open file '%s' for reading 0x%08X\n", path, fd);
		}
		else
		{
			int size = 0;

			if(maxsize >= 0)
			{
				size = maxsize > size_left ? size_left : maxsize;
			}
			else
			{
				size = size_left;
			}

			readbytes = 0;
			while(readbytes < size)
			{
				int ret;

				ret = sceIoRead(fd, (void *) (addr + readbytes), size - readbytes);
				if(ret < 0)
				{
					SHELL_PRINT("Could not write out file\n");
					break;
				}
				else if(ret == 0)
				{
					break;
				}

				readbytes += ret;
			}
			sceIoClose(fd);
			SHELL_PRINT("Read %d bytes into memory\n", readbytes);
		}
	}
	else
	{
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int pokew_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int i;

		addr &= ~3;
		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_WORD);
		if(size_left >= sizeof(unsigned int))
		{
			for(i = 1; i < argc; i++)
			{
				unsigned int data;

				if(memDecode(argv[i], &data))
				{
					_sw(data, addr);
				}
				else
				{
					SHELL_PRINT("Invalid value %s\n", argv[i]);
				}

				addr += 4;
				size_left -= 4;
				if(size_left <= 0)
				{
					break;
				}
			}
		}
		else
		{
			SHELL_PRINT("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int pokeh_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int i;

		addr &= ~1;
		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_HALF);
		if(size_left >= sizeof(u16))
		{
			for(i = 1; i < argc; i++)
			{
				unsigned int data;

				if(memDecode(argv[i], &data))
				{
					_sh(data, addr);
				}
				else
				{
					SHELL_PRINT("Invalid value %s\n", argv[i]);
				}

				addr += 2;
				size_left -= 2;
				if(size_left <= 0)
				{
					break;
				}
			}
		}
		else
		{
			SHELL_PRINT("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int pokeb_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int i;

		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_BYTE);
		if(size_left > 0)
		{
			for(i = 1; i < argc; i++)
			{
				unsigned int data;

				if(memDecode(argv[i], &data))
				{
					_sb(data, addr);
				}
				else
				{
					SHELL_PRINT("Invalid value %s\n", argv[i]);
				}

				addr += 1;
				size_left -= 1;
				if(size_left <= 0)
				{
					break;
				}
			}
		}
		else
		{
			SHELL_PRINT("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int pokes_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		int str_len;

		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_WORD);
		str_len = strlen(argv[1]) + 1;
		if(size_left >= str_len)
		{
			strcpy((void*) addr, argv[1]);
		}
		else
		{
			SHELL_PRINT("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int peekw_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		char fmt = 'x';

		if(argc > 1)
		{
			fmt = argv[1][0];
		}

		addr &= ~3;
		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_WORD);
		if(size_left >= sizeof(unsigned int))
		{
			switch(fmt)
			{
				case 'f': {
							  char floatbuf[64];
							  float *pdata;

							  pspSdkDisableFPUExceptions();
							  pdata = (float *) addr;
							  f_cvt(pdata, floatbuf, sizeof(floatbuf), 6, MODE_GENERIC);
							  SHELL_PRINT("0x%08X: %s\n", addr, floatbuf);
						  }
						  break;
				case 'd': SHELL_PRINT("0x%08X: %d\n", addr, _lw(addr));
						  break;
				case 'o': SHELL_PRINT("0x%08X: %o\n", addr, _lw(addr));
						  break;
				case 'x':
				default:  SHELL_PRINT("0x%08X: 0x%08X\n", addr, _lw(addr));
						  break;
			};
		}
		else
		{
			SHELL_PRINT("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int peekh_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		char fmt = 'x';

		if(argc > 1)
		{
			fmt = argv[1][0];
		}

		addr &= ~1;
		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_HALF);
		if(size_left >= sizeof(u16))
		{
			switch(fmt)
			{
				case 'd': SHELL_PRINT("0x%08X: %d\n", addr, _lh(addr));
						  break;
				case 'o': SHELL_PRINT("0x%08X: %o\n", addr, _lh(addr));
						  break;
				case 'x':
				default:  SHELL_PRINT("0x%08X: 0x%04X\n", addr, _lh(addr));
						  break;
			};
		}
		else
		{
			SHELL_PRINT("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int peekb_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;

	if(memDecode(argv[0], &addr))
	{
		int size_left;
		char fmt = 'x';

		if(argc > 1)
		{
			fmt = argv[1][0];
		}

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		if(size_left > 0)
		{
			switch(fmt)
			{
				case 'd': SHELL_PRINT("0x%08X: %d\n", addr, _lb(addr));
						  break;
				case 'o': SHELL_PRINT("0x%08X: %o\n", addr, _lb(addr));
						  break;
				case 'x':
				default:  SHELL_PRINT("0x%08X: 0x%02X\n", addr, _lb(addr));
						  break;
			};
		}
		else
		{
			SHELL_PRINT("Invalid memory address 0x%08X\n", addr);
			return CMD_ERROR;
		}
	}

	return CMD_OK;
}

static int fillb_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	unsigned int size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		unsigned int size_left;
		unsigned int val;

		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_BYTE);
		size = size > size_left ? size_left : size;

		if(strtoint(argv[2], &val) == 0)
		{
			SHELL_PRINT("Invalid fill value %s\n", argv[2]);
			return CMD_ERROR;
		}

		memset((void *) addr, val, size);
	}

	return CMD_OK;
}

static int fillh_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	unsigned int size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		unsigned int size_left;
		unsigned int val;
		int i;
		u16 *ptr;

		addr &= ~1;

		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_HALF);
		size = size > size_left ? size_left : size;

		if(strtoint(argv[2], &val) == 0)
		{
			SHELL_PRINT("Invalid fill value %s\n", argv[2]);
			return CMD_ERROR;
		}

		ptr = (u16*) addr;

		for(i = 0; i < (size / 2); i++)
		{
			ptr[i] = (u16) val;
		}
	}

	return CMD_OK;
}

static int fillw_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	unsigned int size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		unsigned int size_left;
		unsigned int val;
		int i;
		unsigned int *ptr;

		addr &= ~3;

		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_WORD);
		size = size > size_left ? size_left : size;

		if(strtoint(argv[2], &val) == 0)
		{
			SHELL_PRINT("Invalid fill value %s\n", argv[2]);
			return CMD_ERROR;
		}

		ptr = (unsigned int*) addr;

		for(i = 0; i < (size / 4); i++)
		{
			ptr[i] = (unsigned int) val;
		}
	}

	return CMD_OK;
}

static int findstr_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	unsigned int size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		unsigned int size_left;
		int searchlen;
		void *curr, *found;

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size_left > size ? size : size_left;
		searchlen = strlen(argv[2]);
		curr = (void *) addr;
		
		do
		{
			found = memmem_mask(curr, NULL, size, argv[2], searchlen);
			if(found)
			{
				SHELL_PRINT("Found match at address 0x%p\n", found);
				found++;
				size -= (found - curr);
				curr = found;
			}
		}
		while((found) && (size > 0));
	}

	return CMD_OK;
}

static int findw_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	unsigned int size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		unsigned int size_left;
		int searchlen;
		void *curr, *found;
		uint8_t search[128];
		int i;

		searchlen = 0;
		for(i = 2; i < argc; i++)
		{
			unsigned int val;

			if(strtoint(argv[i], &val) == 0)
			{
				SHELL_PRINT("Invalid search value %s\n", argv[i]);
				return CMD_ERROR;
			}

			memcpy(&search[searchlen], &val, sizeof(val));
			searchlen += sizeof(val);
		}

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size_left > size ? size : size_left;
		curr = (void *) addr;
		
		do
		{
			found = memmem_mask(curr, NULL, size, search, searchlen);
			if(found)
			{
				SHELL_PRINT("Found match at address 0x%p\n", found);
				found++;
				size -= (found - curr);
				curr = found;
			}
		}
		while((found) && (size > 0));
	}

	return CMD_OK;
}

static int findh_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	unsigned int size;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		unsigned int size_left;
		int searchlen;
		void *curr, *found;
		uint8_t search[128];
		int i;

		searchlen = 0;
		for(i = 2; i < argc; i++)
		{
			unsigned int val;

			if(strtoint(argv[i], &val) == 0)
			{
				SHELL_PRINT("Invalid search value %s\n", argv[i]);
				return CMD_ERROR;
			}

			memcpy(&search[searchlen], &val, sizeof(u16));
			searchlen += sizeof(u16);
		}

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size_left > size ? size : size_left;
		curr = (void *) addr;
		
		do
		{
			found = memmem_mask(curr, NULL, size, search, searchlen);
			if(found)
			{
				SHELL_PRINT("Found match at address 0x%p\n", found);
				found++;
				size -= (found - curr);
				curr = found;
			}
		}
		while((found) && (size > 0));
	}

	return CMD_OK;
}

static int findhex_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	unsigned int size;
	uint8_t hex[128];
	uint8_t *mask = NULL;
	uint8_t mask_d[128];
	int hexsize;
	int masksize;

	if(memDecode(argv[0], &addr) && memDecode(argv[1], &size))
	{
		unsigned int size_left;
		void *curr, *found;

		hexsize = decode_hexstr(argv[2], hex, sizeof(hex));
		if(hexsize == 0)
		{
			SHELL_PRINT("Error in search string\n");
			return CMD_ERROR;
		}

		if(argc > 3)
		{
			masksize = decode_hexstr(argv[3], mask_d, sizeof(mask_d));
			if(masksize == 0)
			{
				SHELL_PRINT("Error in mask string\n");
				return CMD_ERROR;
			}

			if(masksize != hexsize)
			{
				SHELL_PRINT("Hex and mask do not match\n");
				return CMD_ERROR;
			}

			mask = mask_d;
		}

		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_BYTE);
		size = size_left > size ? size : size_left;
		curr = (void *) addr;
		
		do
		{
			found = memmem_mask(curr, mask, size, hex, hexsize);
			if(found)
			{
				SHELL_PRINT("Found match at address 0x%p\n", found);
				found++;
				size -= (found - curr);
				curr = found;
			}
		}
		while((found) && (size > 0));
	}

	return CMD_OK;
}

static int copymem_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int src;
	unsigned int dest;
	unsigned int size;

	if((memDecode(argv[0], &src)) && (memDecode(argv[1], &dest)) && memDecode(argv[2], &size))
	{
		unsigned int size_left;
		unsigned int srcsize;
		unsigned int destsize;

		srcsize = memValidate(src, MEM_ATTRIB_WRITE | MEM_ATTRIB_BYTE);
		destsize = memValidate(dest, MEM_ATTRIB_WRITE | MEM_ATTRIB_BYTE);
		size_left = srcsize > destsize ? destsize : srcsize;
		size = size > size_left ? size_left : size;

		memmove((void *) dest, (void *) src, size);
	}

	return CMD_OK;
}

static int disasm_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	unsigned int count = 1;
	int i;

	if(argc > 1)
	{
		if(!memDecode(argv[1], &count) || (count == 0))
		{
			SHELL_PRINT("Invalid count argument\n");
			return CMD_ERROR;
		}
	}

	if(memDecode(argv[0], &addr))
	{
		int size_left;

		addr &= ~3;
		size_left = memValidate(addr, MEM_ATTRIB_READ | MEM_ATTRIB_WORD | MEM_ATTRIB_EXEC);
		if((size_left / 4) < count)
		{
			count = size_left / 4;
		}

		for(i = 0; i < count; i++)
		{
			SHELL_PRINT_CMD(SHELL_CMD_DISASM, "0x%08X:0x%08X", addr, _lw(addr));
			addr += 4;
		}
	}

	return CMD_OK;
}

static int memprot_cmd(int argc, char **argv, unsigned int *vRet)
{
	if(strcmp(argv[0], "on") == 0)
	{
		memSetProtoff(0);
	}
	else if(strcmp(argv[0], "off") == 0)
	{
		memSetProtoff(1);
	}
	else
	{
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int scrshot_cmd(int argc, char **argv, unsigned int *vRet)
{
	char path[1024];
	SceUID block_id;
	void *block_addr;
	void *frame_addr;
	int frame_width;
	int pixel_format;
	int sync = 1;
	int pri = 2;
	unsigned int p;

	if(argc > 1)
	{
		pri = strtol(argv[1], NULL, 10);
	}

	if(!handlepath(g_context.currdir, argv[0], path, TYPE_FILE, 0))
	{
		SHELL_PRINT("Error invalid path\n");
		return CMD_ERROR;
	}

	if((sceDisplayGetFrameBufferInternal(pri, &frame_addr, &frame_width, &pixel_format, sync) < 0) || (frame_addr == NULL))
	{
		SHELL_PRINT("Invalid frame address\n");
		return CMD_ERROR;
	}

	block_id = sceKernelAllocPartitionMemory(4, "scrshot", PSP_SMEM_Low, 512*1024, NULL);
	if(block_id < 0)
	{
		SHELL_PRINT("Error could not allocate memory buffer 0x%08X\n", block_id);
		return CMD_ERROR;
	}
	 
	block_addr = sceKernelGetBlockHeadAddr(block_id);
	SHELL_PRINT("frame_addr 0x%p, frame_width %d, pixel_format %d output %s\n", frame_addr, frame_width, pixel_format, path);

	p = (unsigned int) frame_addr;
	if(p & 0x80000000)
	{
		p |= 0xA0000000;
	}
	else
	{
		p |= 0x40000000;
	}

	bitmapWrite((void *) p, block_addr, pixel_format, path);

	sceKernelFreePartitionMemory(block_id);

	return CMD_OK;
}

static int dcache_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr = 0;
	unsigned int size = 0;
	void (*cacheall)(void);
	void (*cacherange)(const void *addr, unsigned int size);

	if(argc == 2)
	{
		SHELL_PRINT("Must specify a size\n");
		return CMD_ERROR;
	}

	if(strcmp(argv[0], "w") == 0)
	{
		cacheall = sceKernelDcacheWritebackAll;
		cacherange = sceKernelDcacheWritebackRange;
	}
	else if(strcmp(argv[0], "i") == 0)
	{
		cacheall = sceKernelDcacheInvalidateAll;
		cacherange = sceKernelDcacheInvalidateRange;
	}
	else if(strcmp(argv[0], "wi") == 0)
	{
		cacheall = sceKernelDcacheWritebackInvalidateAll;
		cacherange = sceKernelDcacheWritebackInvalidateRange;
	}
	else
	{
		SHELL_PRINT("Invalid type specifier '%s'\n", argv[0]);
		return CMD_ERROR;
	}

	if(argc > 1)
	{
		if(!memDecode(argv[1], &addr))
		{
			SHELL_PRINT("Invalid address\n");
			return CMD_ERROR;
		}

		if(!strtoint(argv[2], &size))
		{
			SHELL_PRINT("Invalid size argument\n");
			return CMD_ERROR;
		}

		cacherange((void *) addr, size);
	}
	else
	{
		cacheall();
	}

	return CMD_OK;
}

static int icache_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr = 0;
	unsigned int size = 0;

	if(argc == 1)
	{
		SHELL_PRINT("Must specify a size\n");
		return CMD_ERROR;
	}

	if(argc > 0)
	{
		if(!memDecode(argv[0], &addr))
		{
			SHELL_PRINT("Invalid address\n");
			return CMD_ERROR;
		}

		if(!strtoint(argv[1], &size))
		{
			SHELL_PRINT("Invalid size argument\n");
			return CMD_ERROR;
		}

		sceKernelIcacheInvalidateRange((void *) addr, size);
	}
	else
	{
		sceKernelIcacheInvalidateAll();
	}

	return CMD_OK;
}

static int modaddr_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	SceModule *pMod;
	const char *opts = "";

	if(argc > 1)
	{
		opts = argv[1];
	}

	if(memDecode(argv[0], &addr))
	{
		pMod = sceKernelFindModuleByAddress(addr);
		if(pMod != NULL)
		{
			print_modinfo(pMod->modid, 1, opts);
		}
		else
		{
			SHELL_PRINT("Couldn't find module at address 0x%08X\n", addr);
		}
	}
	else
	{
		SHELL_PRINT("Invalid address %s\n", argv[0]);
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int exprint_cmd(int argc, char **argv, unsigned int *vRet)
{
	exceptionPrint(NULL);

	return CMD_OK;
}

static int exlist_cmd(int argc, char **argv, unsigned int *vRet)
{
	exceptionList();

	return CMD_OK;
}

static int exctx_cmd(int argc, char **argv, unsigned int *vRet)
{
	exceptionSetCtx(atoi(argv[0]));

	return CMD_OK;
}

static int exprfpu_cmd(int argc, char **argv, unsigned int *vRet)
{
	exceptionFpuPrint(NULL);

	return CMD_OK;
}

static int exprvfpu_cmd(int argc, char **argv, unsigned int *vRet)
{
	int type = VFPU_PRINT_SINGLE;

	if(argc > 0)
	{
		switch(argv[0][0])
		{
			case 's': break;
			case 'c': type = VFPU_PRINT_COL;
					  break;
			case 'r': type = VFPU_PRINT_ROW;
					  break;
			case 'm': type = VFPU_PRINT_MATRIX;
					  break;
			case 'e': type = VFPU_PRINT_TRANS;
					  break;
			default: SHELL_PRINT("Unknown format code '%c'\n", argv[0][0]);
					 return CMD_ERROR;
		}
	}

	exceptionVfpuPrint(NULL, type);

	return CMD_OK;
}

static int exresume_cmd(int argc, char **argv, unsigned int *vRet)
{
	if(argc > 0)
	{
		unsigned int addr;

		if(memDecode(argv[0], &addr))
		{
			unsigned int *epc;

			epc = exceptionGetReg("epc");
			if(epc != NULL)
			{
				*epc = addr;
			}
			else
			{
				SHELL_PRINT("Could not get EPC register\n");
			}
		}
		else
		{
			return CMD_ERROR;
		}
	}

	exceptionResume(NULL, PSP_EXCEPTION_CONTINUE);

	return CMD_OK;
}

static int setreg_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	unsigned int *reg;

	if(memDecode(argv[1], &addr))
	{
		if(argv[0][0] != '$')
		{
			SHELL_PRINT("Error register must start with a $\n");
			return CMD_ERROR;
		}

		reg = exceptionGetReg(&argv[0][1]);
		if(reg == NULL)
		{
			SHELL_PRINT("Error could not find register %s\n", argv[0]);
			return CMD_ERROR;
		}

		*reg = addr;
	}

	return CMD_OK;
}

static int bpth_cmd(int argc, char **argv, unsigned int *vRet)
{
	return thread_do_cmd(argv[0], "break", (ReferFunc) pspSdkReferThreadStatusByName, debugBreakThread);
}

static int bpset_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int addr;
	int ret = CMD_ERROR;
	unsigned int flags = 0;

	if(argc > 1)
	{
		int i = 0;
		while(argv[1][i])
		{
			if(argv[1][i] == '1')
			{
				flags |= DEBUG_BP_ONESHOT;
			}
			else if(argv[1][i] == 'h')
			{
				flags |= DEBUG_BP_HARDWARE;
			}

			i++;
		}
	}

	if(memDecode(argv[0], &addr))
	{
		int size_left;

		addr &= ~3;
		size_left = memValidate(addr, MEM_ATTRIB_WRITE | MEM_ATTRIB_WORD | MEM_ATTRIB_EXEC);
		if(size_left >= sizeof(unsigned int))
		{
			if(debugSetBP(addr, flags, 0))
			{
				ret = CMD_OK;
			}
			else
			{
				SHELL_PRINT("Could not set breakpoint\n");
			}
		}
		else
		{
			SHELL_PRINT("Error, invalidate memory address for breakpoint\n");
		}
	}

	return ret;
}

static int bpdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int id;
	struct Breakpoint *bp;

	if(memDecode(argv[0], &id))
	{
		bp = debugFindBPByIndex(id);
		if(bp)
		{
			debugDeleteBP(bp->addr);
		}
		else
		{
			debugDeleteBP(id);
		}
	}
	else
	{
		SHELL_PRINT("Invalid ID for delete\n");
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int bpdis_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int id;
	struct Breakpoint *bp;

	if(memDecode(argv[0], &id))
	{
		bp = debugFindBPByIndex(id);
		if(bp)
		{
			debugDisableBP(bp->addr);
		}
		else
		{
			debugDisableBP(id);
		}
	}
	else
	{
		SHELL_PRINT("Invalid ID for delete\n");
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int bpena_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int id;
	struct Breakpoint *bp;

	if(memDecode(argv[0], &id))
	{
		bp = debugFindBPByIndex(id);
		if(bp)
		{
			debugEnableBP(bp->addr);
		}
		else
		{
			debugEnableBP(id);
		}
	}
	else
	{
		SHELL_PRINT("Invalid ID for delete\n");
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int bpprint_cmd(int argc, char **argv, unsigned int *vRet)
{
	debugPrintBPs();

	return CMD_OK;
}

static int hwprint_cmd(int argc, char **argv, unsigned int *vRet)
{
	debugPrintHWRegs();

	return CMD_OK;
}

static int step_cmd(int argc, char **argv, unsigned int *vRet)
{
	exceptionResume(NULL, PSP_EXCEPTION_STEP);

	return CMD_OK;
}

static int skip_cmd(int argc, char **argv, unsigned int *vRet)
{
	exceptionResume(NULL, PSP_EXCEPTION_SKIP);

	return CMD_OK;
}

static int version_cmd(int argc, char **argv, unsigned int *vRet)
{
	SHELL_PRINT("PSPLink %s\n", PSPLINK_VERSION);

	return CMD_OK;
}

static int pspver_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int rev;

	rev = sceKernelDevkitVersion();

	SHELL_PRINT("Version: %d.%d.%d (0x%08X)\n", (rev >> 24) & 0xFF, (rev >> 16) & 0xFF, (rev >> 8) & 0xFF, rev);

	return CMD_OK;
}

static int config_cmd(int argc, char **argv, unsigned int *vRet)
{
	configPrint(g_context.bootpath);

	return CMD_OK;
}

static int confset_cmd(int argc, char **argv, unsigned int *vRet)
{
	if(argc > 1)
	{
		configChange(g_context.bootpath, argv[0], argv[1], CONFIG_MODE_ADD);
	}
	else
	{
		configChange(g_context.bootpath, argv[0], "", CONFIG_MODE_ADD);
	}

	return CMD_OK;
}

static int confdel_cmd(int argc, char **argv, unsigned int *vRet)
{
	configChange(g_context.bootpath, argv[0], "", CONFIG_MODE_DEL);

	return CMD_OK;
}

static int power_cmd(int argc, char **argv, unsigned int *vRet)
{
	int batteryLifeTime = 0;
	char fbuf[128];

	SHELL_PRINT("External Power: %s\n", scePowerIsPowerOnline()? "yes" : "no ");
	SHELL_PRINT("%-14s: %s\n", "Battery", scePowerIsBatteryExist()? "present" : "absent ");

	if (scePowerIsBatteryExist()) {
		float batvolt;
	    SHELL_PRINT("%-14s: %s\n", "Low Charge", scePowerIsLowBattery()? "yes" : "no ");
	    SHELL_PRINT("%-14s: %s\n", "Charging", scePowerIsBatteryCharging()? "yes" : "no ");
	    batteryLifeTime = scePowerGetBatteryLifeTime();
		batvolt = (float) scePowerGetBatteryVolt() / 1000.0;
	    SHELL_PRINT("%-14s: %d%% (%02dh%02dm)     \n", "Charge",
		   scePowerGetBatteryLifePercent(), batteryLifeTime/60, batteryLifeTime-(batteryLifeTime/60*60));
		f_cvt(&batvolt, fbuf, sizeof(fbuf), 3, MODE_GENERIC);
	    SHELL_PRINT("%-14s: %sV\n", "Volts", fbuf);
	    SHELL_PRINT("%-14s: %d deg C\n", "Battery Temp", scePowerGetBatteryTemp());
	} else
	    SHELL_PRINT("Battery stats unavailable\n");

	SHELL_PRINT("%-14s: %d MHz\n", "CPU Speed", scePowerGetCpuClockFrequency());
	SHELL_PRINT("%-14s: %d MHz\n", "Bus Speed", scePowerGetBusClockFrequency());

	return CMD_OK;
}

static int poweroff_cmd(int argc, char **argv, unsigned int *vRet)
{
	scePowerRequestStandby();

	return CMD_OK;
}

static int clock_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int val1, val2, val3;

	if((strtoint(argv[0], &val1)) && (strtoint(argv[1], &val2)) && (strtoint(argv[2], &val3)))
	{
		(void) scePowerSetClockFrequency(val1, val2, val3);
	}
	else
	{
		SHELL_PRINT("Invalid clock values\n");
		return CMD_ERROR;
	}

	return CMD_OK;
}

static int profmode_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int *debug;
	const char *mode;

	debug = get_debug_register();
	if(debug)
	{
		if(argc > 0)
		{
			switch(argv[0][0])
			{
				case 't': 
						*debug &= DEBUG_REG_PROFILER_MASK;
						*debug |= DEBUG_REG_THREAD_PROFILER;
						break;
				case 'g': 
						*debug &= DEBUG_REG_PROFILER_MASK;
						*debug |= DEBUG_REG_GLOBAL_PROFILER;
						break;
				case 'o': 
						*debug &= DEBUG_REG_PROFILER_MASK;
						break;
				default: SHELL_PRINT("Invalid profiler mode '%s'\n", argv[0]);
						 return CMD_ERROR;
			};
			SHELL_PRINT("Profiler mode set, you must now reset psplink\n");
		}
		else
		{
			if((*debug & DEBUG_REG_THREAD_PROFILER) == DEBUG_REG_THREAD_PROFILER)
			{
				mode = "Thread";
			}
			else if((*debug & DEBUG_REG_GLOBAL_PROFILER) == DEBUG_REG_GLOBAL_PROFILER)
			{
				mode = "Global";
			}
			else
			{
				mode = "Off";
			}

			SHELL_PRINT("Profiler Mode: %s\n", mode);
		}
	}

	
	return CMD_OK;
}

static int debugreg_cmd(int argc, char **argv, unsigned int *vRet)
{
	unsigned int *debug;

	debug = get_debug_register();
	if(debug)
	{
		if(argc > 0)
		{
			unsigned int val;

			if(strtoint(argv[0], &val))
			{
				*debug = val;
			}
			else
			{
				SHELL_PRINT("Invalid debug reg value '%s'\n", argv[0]);
				return CMD_ERROR;
			}
		}
		else
		{
			SHELL_PRINT("Debug Register: 0x%08X\n", *debug);
		}
	}

	return CMD_OK;
}

const char* PspInterruptNames[67] = {//67 interrupts
	0, 0, 0, 0, "GPIO", "ATA_ATAPI", "UmdMan", "MScm0",
	"Wlan", 0, "Audio", 0, "I2C", 0, "SIRCS_IrDA",
	"Systimer0", "Systimer1", "Systimer2", "Systimer3",
	"Thread0", "NAND", "DMACPLUS", "DMA0", "DMA1",
	"Memlmd", "GE", 0, 0, 0, 0, "Display", "MeCodec", 0,
	0, 0, 0, "HP_Remote", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "MScm1", "MScm2",
	0, 0, 0, "Thread1", "Interrupt"
};

static void print_irq(PspIntrHandlerOptionParam *data, int intno, int sub)
{
	const char *name = PspInterruptNames[intno];

	if(name == NULL)
	{
		name = "Unknown";
	}

	if(data->entry)
	{
		if(sub >= 0)
		{
			SHELL_PRINT("Subintr %d/%d/%s - Calls %d\n", intno, sub, name, data->calls);
		}
		else
		{
			SHELL_PRINT("Interrupt %d/%s - Calls %d\n", intno, name, data->calls);
		}
		SHELL_PRINT("Entry 0x%08X - Common 0x%08X\n", data->entry, data->common);
		SHELL_PRINT("GP 0x%08X - Intrcode %d\n", data->gp, data->intr_code);
		SHELL_PRINT("SubCount %d - IntrLevel %d\n", data->sub_count, data->intr_level);
		SHELL_PRINT("Enabled %d - field_1c 0x%08X\n", data->enabled, data->field_1C);
		SHELL_PRINT("totalclk_lo 0x%08X - totalclk_hi 0x%08X\n", data->total_clock_lo, data->total_clock_hi);
		SHELL_PRINT("minclk_lo 0x%08X - minclk_hi 0x%08X\n", data->min_clock_lo, data->min_clock_hi);
		SHELL_PRINT("maxclk_lo 0x%08X - maxclk_hi 0x%08X\n\n", data->max_clock_lo, data->max_clock_hi);
	}
}

static int irqs_cmd(int argc, char **argv, unsigned int *vRet)
{
	PspIntrHandlerOptionParam data;
	int i;
	
	if(argc > 0)
	{
		int intno;
		int ret;

		intno = strtoul(argv[0], NULL, 0);
		memset(&data, 0, sizeof(data));
		data.size = sizeof(data);
		ret = QueryIntrHandlerInfo(intno, -1, &data);
		if(ret == 0)
		{
			print_irq(&data, intno, -1);
			if(data.sub_count > 0)
			{
				int subs = data.sub_count;
				for(i = 0; i < subs; i++)
				{
					memset(&data, 0, sizeof(data));
					data.size = sizeof(data);
					ret = QueryIntrHandlerInfo(intno, i, &data);
					if(ret == 0)
					{
						print_irq(&data, intno, i);
					}
					else
					{
						SHELL_PRINT("Arg: %08X\n", ret);
					}
				}
			}
		}
	}
	else
	{
		for(i = 0; i < 67; i++)
		{
			int ret;
			memset(&data, 0, sizeof(data));
			data.size = sizeof(data);
			ret = QueryIntrHandlerInfo(i, -1, &data);
			print_irq(&data, i, -1);
		}
	}

	return CMD_OK;
}

static int iena_cmd(int argc, char **argv, unsigned int *vRet)
{
	int disable = 0;
	int intno = 0;
	int subno = -1;

	if(argv[0][0] == 'e')
	{
		disable = 0;
	}
	else if(argv[0][0] == 'd')
	{
		disable = 1;
	}
	else
	{
		return CMD_ERROR;
	}

	intno = strtoul(argv[1], NULL, 0);
	if(argc > 2)
	{
		subno = strtoul(argv[2], NULL, 0);
	}

	if(subno < 0)
	{
		if(disable)
		{
			sceKernelDisableIntr(intno);
		}
		else
		{
			sceKernelEnableIntr(intno);
		}
	}
	else
	{
		if(disable)
		{
			sceKernelDisableSubIntr(intno, subno);
		}
		else
		{
			sceKernelEnableSubIntr(intno, subno);
		}
	}
	
	return CMD_OK;
}

static int irel_cmd(int argc, char **argv, unsigned int *vRet)
{
	int intno = 0;
	int subno = -1;

	intno = strtoul(argv[0], NULL, 0);
	if(argc > 1)
	{
		subno = strtoul(argv[1], NULL, 0);
	}
	if(subno < 0)
	{
		sceKernelReleaseIntrHandler(intno);
	}
	else
	{
		sceKernelReleaseSubIntrHandler(intno, subno);
	}

	return CMD_OK;
}

static int tonid_cmd(int argc, char **argv, unsigned int *vRet)
{
	SHELL_PRINT("Name: %s, Nid: 0x%08X\n", argv[0], libsNameToNid(argv[0]));

	return CMD_OK;
}

int call_thread(SceSize args, void *argp)
{
	if(args == sizeof(struct call_frame))
	{
		struct call_frame *s = (struct call_frame *) argp;
		u64 ret;

		ret = s->func(s->args[0], s->args[1], s->args[2], s->args[3], s->args[4], s->args[5]);
		SHELL_PRINT("Return: 0x%08X:0x%08X\n", ret >> 32, ret & 0xFFFFFFFF);
	}

	sceKernelExitDeleteThread(0);

	return 0;
}

static int call_cmd(int argc, char **argv, unsigned int *vRet)
{
	struct call_frame frame;
	SceUID uid;
	SceUInt timeout;

	memset(&frame, 0, sizeof(frame));
	if(memDecode(argv[0], (void*) &frame.func))
	{
		int i;
		for(i = 1; i < argc; i++)
		{
			if(!memDecode(argv[i], &frame.args[i-1]))
			{
				break;
			}
		}
		if(i != argc)
		{
			SHELL_PRINT("Invalid parameter %s\n", argv[i]);
			return CMD_ERROR;
		}

		SHELL_PRINT("Func 0x%p ", frame.func);
		for(i = 0; i < argc-1; i++)
		{ 
			SHELL_PRINT("arg%d 0x%08x ", i, frame.args[i]);
		}
		SHELL_PRINT("\n");

		uid = sceKernelCreateThread("CallThread", call_thread, 0x18, 0x1000, 0, NULL);
		if(uid >= 0)
		{
			timeout = 1000000;
			sceKernelStartThread(uid, sizeof(frame), &frame);
			sceKernelWaitThreadEnd(uid, &timeout);
		}
	}
	else
	{
		SHELL_PRINT("Invalid function address %s\n", argv[0]);
		return CMD_ERROR;
	}

	return CMD_OK;
}

static void tab_do_uid(int argc, char **argv)
{
	SceUID ids[100];
	const char *name;
	int ret;
	int count;
	int i;
	int namelen;
	name = &argv[0][1];
	namelen = strlen(name);

	enable_kprintf(0);
	memset(ids, 0, 100 * sizeof(SceUID));
	ret = g_GetModuleIdList(ids, 100 * sizeof(SceUID), &count);
	if(ret >= 0)
	{
		SceKernelModuleInfo info;

		for(i = 0; i < count; i++)
		{
			if(psplinkReferModule(ids[i], &info) == 0)
			{
				if(strncmp(info.name, name, namelen) == 0)
				{
					SHELL_PRINT_CMD(SHELL_CMD_TAB, "@%s", info.name);
				}
			}
		}
	}
	enable_kprintf(1);
}

static void tab_do_dir(int argc, char **argv)
{
	char *dir;
	char path[1024];

	dir = argv[0];
	if(handlepath(g_context.currdir, dir, path, TYPE_DIR, 1) == 0)
	{
		/* We dont care about sending an error, shouldn't even print */
	}
	else
	{
		int did;
		int namelen;
		const char *name;
		SceIoDirent entry;

		if(argc > 1)
		{
			name = argv[1];
		}
		else
		{
			name = "";
		}
		namelen = strlen(name);
		did = sceIoDopen(path);
		if(did >= 0)
		{
			while(1)
			{
				memset(&entry, 0, sizeof(entry));
				if(sceIoDread(did, &entry) <= 0)
				{
					break;
				}
				/* We eliminate . and .. */
				if(strcmp(entry.d_name, ".") && strcmp(entry.d_name, ".."))
				{
					if(strncmp(entry.d_name, name, namelen) == 0)
					{
						if(FIO_SO_ISDIR(entry.d_stat.st_attr) || FIO_S_ISDIR(entry.d_stat.st_mode))
						{
							SHELL_PRINT_CMD(SHELL_CMD_TAB, "%s/", entry.d_name);
						}
						else
						{
							SHELL_PRINT_CMD(SHELL_CMD_TAB, "%s", entry.d_name);
						}
					}
				}
			}
			sceIoDclose(did);
		}
	}
}

static int tab_cmd(int argc, char **argv, unsigned int *vRet)
{
	if(argv[0][0] == '@')
	{
		tab_do_uid(argc, argv);
	}
	else
	{
		tab_do_dir(argc, argv);
	}

	return CMD_OK;
}

static void print_sym_info(SceUID uid)
{
	SceKernelModuleInfo info;
	int ret;

	enable_kprintf(0);
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);

	ret = g_QueryModuleInfo(uid, &info);
	if(ret >= 0)
	{
		SHELL_PRINT_CMD(SHELL_CMD_SYMLOAD, "%08X%08X%s", info.text_addr, sceKernelDevkitVersion(), info.name);
	}
	enable_kprintf(1);

}

static int symload_cmd(int argc, char **argv, unsigned int *vRet)
{
	SceUID ids[100];
	SceUID uid;
	int ret;
	int count;
	int i;

	if(argc > 0)
	{

		uid = get_module_uid(argv[0]);
		if(uid >= 0)
		{
			print_sym_info(uid);
		}
		else
		{
			return CMD_ERROR;
		}
	}
	else
	{
		memset(ids, 0, 100 * sizeof(SceUID));
		ret = g_GetModuleIdList(ids, 100 * sizeof(SceUID), &count);
		if(ret >= 0)
		{
			for(i = 0; i < count; i++)
			{
				print_sym_info(ids[i]);
			}
		}
	}

	return CMD_OK;
}

static int exit_cmd(int argc, char **argv, unsigned int *vRet)
{
	return CMD_EXITSHELL;
}

/* Define the list of commands */
const struct sh_command commands[] = {
	SHELL_COMMANDS
};

/* Find a command from the command list */
static const struct sh_command* find_command(const char *cmd)
{
	const struct sh_command* found_cmd = NULL;
	int cmd_loop;

	for(cmd_loop = 0; commands[cmd_loop].name != NULL; cmd_loop++)
	{
		if(strcmp(cmd, commands[cmd_loop].name) == 0)
		{
			found_cmd = &commands[cmd_loop];
			break;
		}

		if(commands[cmd_loop].syn)
		{
			if(strcmp(cmd, commands[cmd_loop].syn) == 0)
			{
				found_cmd = &commands[cmd_loop];
				break;
			}
		}
	}

	return found_cmd;
}

int shellExecute(int argc, char **argv, unsigned int *vRet)
{
	int ret = CMD_OK;
	char *cmd;

	scePowerTick(0);

	if((argc > 0) && (argv[0][0] != '#'))
	{
		const struct sh_command *found_cmd;

		cmd = argv[0];
		/* Check for a completion function */
		found_cmd = find_command(cmd);
		if((found_cmd) && (found_cmd->func) && (found_cmd->min_args <= (argc-1)))
		{
			ret = found_cmd->func(argc-1, &argv[1], vRet);
		}
		else
		{
			ret = CMD_ERROR;
		}
	}

	return ret;
}

int shellParseThread(SceSize args, void *argp)
{
	int ret;
	unsigned char cli[MAX_CLI];
	int argc;
	char *argv[16];
	unsigned int vRet;

	usbShellInit();

	while(1)
	{
		argc = usbShellReadInput(cli, argv, MAX_CLI, 16);
		if(argc > 0)
		{
			vRet = 0;
			if(setjmp(g_context.parseenv) == 0)
			{
				ret = shellExecute(argc, argv, &vRet);
			}
			else
			{
				ret = CMD_ERROR;
			}

			if(ret == CMD_OK)
			{
				SHELL_PRINT_CMD(SHELL_CMD_SUCCESS, "0x%08X", vRet);
			}
			else if(ret == CMD_ERROR)
			{
				SHELL_PRINT_CMD(SHELL_CMD_ERROR, "0x%08X", vRet);
			}
			else if(ret == CMD_EXITSHELL)
			{
				psplinkExitShell();
			}
		}
	}

	return 0;
}

int shellInit(const char *init_dir)
{
	strcpy(g_context.currdir, init_dir);

	return 0;
}
