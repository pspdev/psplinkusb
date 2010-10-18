/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * asm.h - PSPLINK pc terminal simple MIPS assembler
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.pspdev.org/psp/branches/psplinkusb/pspsh/asm.h $
 * $Id: asm.h 2157 2007-01-29 22:50:26Z tyranid $
 */
#ifndef __ASM_H__
#define __ASM_H__

int asmAssemble(const char *str, unsigned int PC, unsigned int *inst);

#endif
