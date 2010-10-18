/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * psplink.h - PSPLINK global header file.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/psplink.h $
 * $Id: psplink.h 2303 2007-08-26 17:10:21Z tyranid $
 */

#ifndef __PSPLINK_H
#define __PSPLINK_H

#include <stdint.h>
#include "pspstdio.h"

/* Event flags */
#define EVENT_SIO       0x01
#define EVENT_INIT      0x10
#define EVENT_RESUMESH  0x100
#define EVENT_RESET     0x200

#define MAXPATHLEN      1024
#define MAX_ARGS 16

#define DEFAULT_BAUDRATE 115200

#ifdef DEBUG
#define DEBUG_START { int fd; fd = sceIoOpen("ms0:/debug.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666); sceIoClose(fd); }
#define DEBUG_PRINTF(fmt, ...) \
{ \
	int fd; \
	fd = sceIoOpen("ms0:/debug.txt", PSP_O_WRONLY | PSP_O_APPEND, 0666); \
	fdprintf(fd, fmt, ## __VA_ARGS__); \
	sceIoClose(fd); \
}
#else
#define DEBUG_START
#define DEBUG_PRINTF(fmt, ...)
#endif

typedef unsigned int jmp_buf[12];
int setjmp(jmp_buf jmp);
int longjmp(jmp_buf jmp, int ret);

int shprintf(const char *fmt, ...);

#define SHELL_PRINT(fmt, ...) shprintf("\xff" fmt "\xfe", ## __VA_ARGS__)
#define SHELL_PRINT_CMD(cmd, fmt, ...) shprintf("\xff" "%c" fmt "\xfe", cmd, ## __VA_ARGS__)

void psplinkReset(void);
void psplinkStop(void);
unsigned int  psplinkSetK1(unsigned int k1);
void psplinkGetCop0(unsigned int *regs);
int psplinkParseComamnd(char *command);
void psplinkExitShell(void);
SceUID load_gdb(const char *bootpath, int argc, char **argv);

struct ConfigContext;
struct GlobalContext;

#define SAVED_MAGIC 0xBAA1A11C
#define SAVED_ADDR  0x883F0000

#define REBOOT_MODE_GAME    0
#define REBOOT_MODE_VSH     1
#define REBOOT_MODE_UPDATER 2

struct SavedContext
{
	uint32_t magic;
	char currdir[MAXPATHLEN];
	int rebootkey;
};

struct GlobalContext
{
	/* The filename of the bootstrap */
	const char *bootfile;
	/* The boot path */
	char bootpath[MAXPATHLEN];
	/* Indicates the current directory */
	char currdir[MAXPATHLEN];
	int resetonexit;
	SceUID thevent;
	int gdb;
	int pid;
	int rebootkey;
	jmp_buf parseenv;
};

#endif
