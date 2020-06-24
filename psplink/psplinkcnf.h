/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * psplinkcnf.c - Functions to manipulate the configuration file
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
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
