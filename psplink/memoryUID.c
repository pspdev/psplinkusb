/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * memoryUID.c - Code to dump the UID table.
 *
 * Copyright (c) 2005 John_K
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/memoryUID.c $
 * $Id: memoryUID.c 2115 2006-12-30 14:50:17Z tyranid $
 */
#include <pspkernel.h>
#include <pspsysmem.h>
#include <stdio.h>
#include <string.h>
#include "psplink.h"
#include "memoryUID.h"

uidControlBlock* findUIDObject(SceUID uid, const char *name, const char *parent)
{
    uidControlBlock *entry;
    uidControlBlock *end;

	if(sceKernelDevkitVersion() == 0x01000300)
	{
    	entry = (uidControlBlock *) UIDLIST_START_1_0;
	}
	else
	{
		entry = SysMemForKernel_536AD5E1();
	}
	/*
    if(sceKernelDevkitVersion() == 0x01050001)
    	entry = (uidControlBlock *) UIDLIST_START_1_5;
    else
    	entry = (uidControlBlock *) UIDLIST_START_1_0;
	*/
    entry = entry->parent;
    end = entry;
    entry = entry->nextEntry;

    do {
		if(entry->UID == uid)
            return entry;
        if (entry->nextChild != entry) {
            do {
				uidControlBlock *ret = NULL;
                entry = entry->nextChild;
				if(name)
				{
					if (strcmp(entry->name, name) == 0)
						ret = entry;
				}
				else
				{
					if(entry->UID == uid)
						ret = entry;
				}

				if(ret)
				{
					if(parent && ret->type)
					{
						if(strcmp(parent, ret->type->name) == 0)
						{
							return ret;
						}
					}
					else
					{
						return ret;
					}
				}

            } while (entry->nextChild != entry->type);
            entry = entry->nextChild;
        }
        entry = entry->nextEntry;
    } while (entry->nextEntry != end);  //Stop at 'Basic' entry like Sysmem does, just not in the same way ;)
    return 0;
}

SceUID findUIDByName(const char *name)
{
	uidControlBlock *entry = findUIDObject(0, name, NULL);
	if(entry)
	{
		return entry->UID;
	}

	return -1;
}

void printUIDEntry(uidControlBlock *entry)
{
	if(entry)
	{
		SHELL_PRINT("(UID): 0x%08X, (entry): 0x%p, (size): %d, (attr): 0x%X, (Name): %s\n", entry->UID, entry, entry->size << 2, entry->attribute, entry->name);
	}
}

void printUIDList(const char *name)
{
    uidControlBlock *entry;
    uidControlBlock *end;
	int cmp = 0;

	if(sceKernelDevkitVersion() == 0x01000300)
	{
    	entry = (uidControlBlock *) UIDLIST_START_1_0;
	}
	else
	{
		entry = SysMemForKernel_536AD5E1();
	}

	/*
    if(sceKernelDevkitVersion() == 0x01050001)
    	entry = (uidControlBlock *) UIDLIST_START_1_5;
    else
    	entry = (uidControlBlock *) UIDLIST_START_1_0;
	*/
    entry = entry->parent;
    end = entry;
    entry = entry->nextEntry;
    //printf("************ MY UID LIST START ************\n");
    do {
		if(name)
		{
			cmp = strcmp(entry->name, name);
		}

		if(cmp == 0)
		{
			SHELL_PRINT("\n[%s]    UID 0x%08X (attr 0x%X entry 0x%p)\n", entry->name, entry->UID, entry->attribute, entry);
		}

		if (entry->nextChild == entry) {
			if(cmp == 0)
			{
				SHELL_PRINT("    <No UID objects>\n");
			}

		} else {
			do {
				entry = entry->nextChild;
				if(cmp == 0)
				{
					//printf("(Name): %31s, (UID): 0x%08X, (entry): 0x%p (attr): 0x%X \n", entry->name, entry->UID, entry, entry->attribute);
					printUIDEntry(entry);
				}
			} while (entry->nextChild != entry->type);
			entry = entry->nextChild;
		}

		entry = entry->nextEntry;
    } while (entry->nextEntry != end);  //Stop at 'Basic' entry like Sysmem does but not in the same way ;)
    //printf("************ MY UID LIST END ************\n");
}

