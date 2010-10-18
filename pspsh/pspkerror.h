/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * pspkerror.h - Definitions for error codes
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.pspdev.org/psp/branches/psplinkusb/pspsh/pspkerror.h $
 * $Id: pspkerror.h 2066 2006-11-15 21:09:34Z tyranid $
 */
#ifndef PSPKERROR_H
#define PSPKERROR_H

#include <stdlib.h>

struct PspErrorCode
{
	const char *name;
	unsigned int num;
};

extern struct PspErrorCode PspKernelErrorCodes[];

#endif /* PSPKERROR_H */
