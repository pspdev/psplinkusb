/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * common.h - GDB stub for psplink
 *
 * Copyright (c) 2007 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/usbgdb/common.h $
 * $Id: common.h 2174 2007-02-08 19:49:27Z tyranid $
 */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <psptypes.h>
#include <exception.h>

struct GdbContext 
{
	SceSize args;
	void *argp;
	SceUID uid;
	SceKernelModuleInfo info;
	int started;
	struct PsplinkContext ctx;
	int elf;
};

extern struct GdbContext g_context;

/* Define ELF types */
typedef u32 Elf32_Addr; 
typedef u16 Elf32_Half;
typedef u32 Elf32_Off;
typedef s32 Elf32_Sword;
typedef u32 Elf32_Word;

#define ELF_MAGIC	0x464C457F

#define ELF_MIPS_TYPE 0x0002
#define ELF_PRX_TYPE  0xFFA0

/* ELF file header */
typedef struct { 
	Elf32_Word e_magic;
	u8 e_class;
	u8 e_data;
	u8 e_idver;
	u8 e_pad[9];
	Elf32_Half e_type; 
	Elf32_Half e_machine; 
	Elf32_Word e_version; 
	Elf32_Addr e_entry; 
	Elf32_Off e_phoff; 
	Elf32_Off e_shoff; 
	Elf32_Word e_flags; 
	Elf32_Half e_ehsize; 
	Elf32_Half e_phentsize; 
	Elf32_Half e_phnum; 
	Elf32_Half e_shentsize; 
	Elf32_Half e_shnum; 
	Elf32_Half e_shstrndx; 
} __attribute__((packed)) Elf32_Ehdr;

int GdbReadByte(unsigned char *address, unsigned char *dest);
int GdbWriteByte(char val, unsigned char *dest);
int GdbHandleException (struct PsplinkContext *ctx);
int putDebugChar(unsigned char ch);
int getDebugChar(unsigned char *ch);
int peekDebugChar(unsigned char *ch);
int  writeDebugData(void *data, int len);

//#define DEBUG

#ifdef DEBUG_PRINTF
#undef DEBUG_PRINTF
#endif

#ifdef DEBUG
#define DEBUG_PRINTF(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define DEBUG_PRINTF(fmt, ...)
#endif

#endif
