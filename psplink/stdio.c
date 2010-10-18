/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * stdio.c - PSPLINK kernel module tty code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/stdio.c $
 * $Id: stdio.c 2304 2007-08-26 17:21:11Z tyranid $
 */

/* Based off the pspsdk tty code */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspstdio.h>

static int g_initialised = 0;
static PspDebugInputHandler g_stdin_handler = NULL;
static PspDebugPrintHandler g_stdout_handler = NULL;
static PspDebugPrintHandler g_stderr_handler = NULL;
static PspDebugPrintHandler g_shell_handler = NULL;
static SceUID g_in_sema = 0;
/* Probably stdout and stderr should not be guarded by the same mutex */
static SceUID g_out_sema = 0;

extern int sceKernelStdin(void);


static int io_init(PspIoDrvArg *arg)
{
	return 0;
}

static int io_exit(PspIoDrvArg *arg)
{
	return 0;
}

static int io_open(PspIoDrvFileArg *arg, char *file, int mode, SceMode mask)
{
	if((arg->fs_num != STDIN_FILENO) && (arg->fs_num != STDOUT_FILENO) 
			&& (arg->fs_num != STDERR_FILENO))
	{
		return SCE_KERNEL_ERROR_NOFILE;
	}

	return 0;
}

static int io_read(PspIoDrvFileArg *arg, char *data, int len)
{
	int ret = 0;

	(void) sceKernelWaitSema(g_in_sema, 1, 0);
	if((arg->fs_num == STDIN_FILENO) && (g_stdin_handler != NULL))
	{
		ret = g_stdin_handler(data, len);
	}
	(void) sceKernelSignalSema(g_in_sema, 1);

	return ret;
}

static int io_write(PspIoDrvFileArg *arg, const char *data, int len)
{
	int ret = 0;

	(void) sceKernelWaitSema(g_out_sema, 1, 0);
	if((arg->fs_num == STDOUT_FILENO) && (g_stdout_handler != NULL))
	{
		ret = g_stdout_handler(data, len);
	}
	else if((arg->fs_num == STDERR_FILENO) && (g_stderr_handler != NULL))
	{
		ret = g_stderr_handler(data, len);
	}
	(void) sceKernelSignalSema(g_out_sema, 1);

	return ret;
}

static PspIoDrvFuncs tty_funcs = 
{
	io_init,
	io_exit,
	io_open,
	NULL,
	io_read,
	io_write,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static PspIoDrv tty_driver = 
{
	"tty", 0x10, 0x800, "TTY", &tty_funcs
};

int stdioTtyInit(void)
{
	int ret;
	(void) sceIoDelDrv("tty"); /* Ignore error */
	ret = sceIoAddDrv(&tty_driver);
	if(ret < 0)
	{
		return ret;
	}

	g_in_sema = sceKernelCreateSema("TtyInMutex", 0, 1, 1, NULL);
	if(g_in_sema < 0)
	{
		return g_in_sema;
	}

	g_out_sema = sceKernelCreateSema("TtyOutMutex", 0, 1, 1, NULL);
	if(g_out_sema < 0)
	{
		return g_out_sema;
	}

	ret = sceIoReopen("tty0:", PSP_O_RDONLY, 0777, sceKernelStdin());
	if(ret < 0)
	{
		return ret;
	}

	ret = sceKernelStdoutReopen("tty1:", PSP_O_WRONLY, 0777);
	if(ret < 0)
	{
		return ret;
	}

	ret = sceKernelStderrReopen("tty2:", PSP_O_WRONLY, 0777);
	if(ret < 0)
	{
		return ret;
	}

	g_initialised = 1;

	return 0;
}

int stdioInstallStdinHandler(PspDebugInputHandler handler)
{
	g_stdin_handler = handler;

	return 0;
}

int stdioInstallStdoutHandler(PspDebugPrintHandler handler)
{
	g_stdout_handler = handler;

	return 0;
}

int stdioInstallStderrHandler(PspDebugPrintHandler handler)
{
	g_stderr_handler = handler;

	return 0;
}

int stdioInstallShellHandler(PspDebugPrintHandler handler)
{
	g_shell_handler = handler;

	return 0;
}

