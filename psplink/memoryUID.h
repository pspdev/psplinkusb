/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * memoryUID.h - Header file for UID dumper.
 *
 * Copyright (c) 2005 John_K
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/memoryUID.h $
 * $Id: memoryUID.h 2115 2006-12-30 14:50:17Z tyranid $
 */
#ifndef __MEMORYUID_H__
#define __MEMORYUID_H__

#include <pspsysmem_kernel.h>

#define UIDLIST_START_1_0 0x8800d030
#define UIDLIST_START_1_5 0x8800fc98

uidControlBlock* findUIDObject(SceUID uid, const char *name, const char *parent);
SceUID findUIDByName(const char *name);
void printUIDList(const char *name);
void printUIDEntry(uidControlBlock *entry);

#define findObjectByUID(uid) findUIDObject(uid, NULL, NULL)
#define findObjectByName(name) findUIDObject(0, name, NULL)
#define findObjectByUIDWithParent(uid, parent) findUIDObject(uid, NULL, parent)
#define findObjectByNameWithParent(name, parent) findUIDObject(0, name, parent)

#endif
