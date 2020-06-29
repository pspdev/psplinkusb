/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * pspkerror.h - Definitions for error codes
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
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
