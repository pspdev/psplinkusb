/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * shell.h - PSPLINK kernel module shell code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/shell.h $
 * $Id: shell.h 2037 2006-10-22 17:05:29Z tyranid $
 */

/* Return values for the commands */
#define CMD_EXITSHELL 	1
#define CMD_OK		  	0
#define CMD_ERROR		-1

/* Parse a command string */
int shellParse(char *command);
void shellStart(void);
int shellInit(const char *init_dir);
int shellParseThread(SceSize args, void *argp);
