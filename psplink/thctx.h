/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * thctx.h - Thread context library code for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/thctx.h $
 * $Id: thctx.h 2174 2007-02-08 19:49:27Z tyranid $
 */
#ifndef __THCTX_H__
#define __THCTX_H__

#include "exception.h"

int threadFindContext(SceUID uid);
unsigned int thGetCurrentEPC(SceUID uid);
int psplinkGetFullThreadContext(SceUID uid, struct PsplinkContext *ctx);

#endif
