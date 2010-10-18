/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * bitmap.h - PSPLINK kernel module bitmap code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/bitmap.h $
 * $Id: bitmap.h 1622 2005-12-27 19:29:57Z tyranid $
 */

#ifndef __BITMAP_H__
#define __BITMAP_H__

int bitmapWrite(void *frame_addr, void *tmp_buf, int format, const char *file);

#endif
