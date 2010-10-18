/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * libs.h - Module library code for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/libs.h $
 * $Id: libs.h 2301 2007-08-26 13:48:05Z tyranid $
 */
#ifndef __LIBS_H__
#define __LIBS_H__

int libsPrintEntries(SceUID uid);
int libsPrintImports(SceUID uid);
unsigned int libsFindExportByName(SceUID uid, const char *library, const char *name);
unsigned int libsFindExportByNid(SceUID uid, const char *library, unsigned int nid);
void* libsFindExportAddrByName(SceUID uid, const char *library, const char *name);
void* libsFindExportAddrByNid(SceUID uid, const char *library, unsigned int nid);
int libsPatchFunction(SceUID uid, const char *library, unsigned int nid, u16 retval);
unsigned int libsNameToNid(const char *name);
unsigned int libsFindImportAddrByName(SceUID uid, const char *library, const char *name);
unsigned int libsFindImportAddrByNid(SceUID uid, const char *library, unsigned int nid);

#endif
