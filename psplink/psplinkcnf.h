/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * psplinkcnf.c - Functions to manipulate the configuration file
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/psplinkcnf.h $
 * $Id: psplinkcnf.h 1818 2006-03-06 18:34:38Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include "psplink.h"
#include "util.h"

struct ConfigFile
{
	PspFile file;
	char str_buf[MAX_BUFFER];
	int  line;
};

int psplinkConfigOpen(const char *filename, struct ConfigFile *cnf);
const char* psplinkConfigReadNext(struct ConfigFile *cnf, const char **name);
void psplinkConfigClose(struct ConfigFile *cnf);
