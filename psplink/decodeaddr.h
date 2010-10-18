/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * decodeaddr.h - PSPLINK kernel module decode memory address code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/decodeaddr.h $
 * $Id: decodeaddr.h 2301 2007-08-26 13:48:05Z tyranid $
 */

#ifndef __DECODEADDR_H__
#define __DECODEADDR_H__

#define MEM_ATTRIB_READ	 (1 << 0)
#define MEM_ATTRIB_WRITE (1 << 1)
#define MEM_ATTRIB_EXEC	 (1 << 2)
#define MEM_ATTRIB_BYTE  (1 << 3)
#define MEM_ATTRIB_HALF  (1 << 4)
#define MEM_ATTRIB_WORD  (1 << 5)
#define MEM_ATTRIB_DBL   (1 << 6)
#define MEM_ATTRIB_ALL	 0xFFFFFFFF

int memDecode(const char *line, unsigned int *val);
int memValidate(unsigned int addr, unsigned int attrib);
void memPrintRegions(void);
void memSetProtoff(int protoff);

#endif
