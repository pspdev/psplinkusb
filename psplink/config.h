/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * config.h - PSPLINK kernel module configuration loader.
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 *
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CONFIG_MODE_ADD 1
#define CONFIG_MODE_DEL 2

struct ConfigContext
{
	/* Indicates whether to enable the psplink user module */
	int  enableuser;
	int  resetonexit;
	int  pid;
};

void configLoad(const char *bootpath, struct ConfigContext *ctx);
void configPrint(const char *bootpath);
void configChange(const char *bootpath, const char *name, const char *val, int mode);

#endif
