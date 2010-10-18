/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * psp_fileio.h - Some definitions of PSP specific stuff for USB HostFS
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/usbhostfs_pc/psp_fileio.h $
 * $Id: psp_fileio.h 1816 2006-02-26 14:09:15Z tyranid $
 */
#ifndef __PSP_FILEIO_H__
#define __PSP_FILEIO_H__

#include <stdint.h>

#define PSP_O_RDONLY	0x0001
#define PSP_O_WRONLY	0x0002
#define PSP_O_RDWR	(PSP_O_RDONLY | PSP_O_WRONLY)
#define PSP_O_NBLOCK	0x0004
#define PSP_O_DIROPEN	0x0008	// Internal use for dopen
#define PSP_O_APPEND	0x0100
#define PSP_O_CREAT		0x0200
#define PSP_O_TRUNC		0x0400
#define	PSP_O_EXCL		0x0800
#define PSP_O_NOWAIT	0x8000

#define PSP_SEEK_SET	0
#define PSP_SEEK_CUR	1
#define PSP_SEEK_END	2

/** Access modes for st_mode in SceIoStat (confirm?). */
enum IOAccessModes
{
	/** Format bits mask */
	FIO_S_IFMT		= 0xF000,
	/** Symbolic link */
	FIO_S_IFLNK		= 0x4000,
	/** Directory */
	FIO_S_IFDIR		= 0x1000,
	/** Regular file */
	FIO_S_IFREG		= 0x2000,

	/** Set UID */
	FIO_S_ISUID		= 0x0800,
	/** Set GID */
	FIO_S_ISGID		= 0x0400,
	/** Sticky */
	FIO_S_ISVTX		= 0x0200,

	/** User access rights mask */
	FIO_S_IRWXU		= 0x01C0,	
	/** Read user permission */
	FIO_S_IRUSR		= 0x0100,
	/** Write user permission */
	FIO_S_IWUSR		= 0x0080,
	/** Execute user permission */
	FIO_S_IXUSR		= 0x0040,	

	/** Group access rights mask */
	FIO_S_IRWXG		= 0x0038,	
	/** Group read permission */
	FIO_S_IRGRP		= 0x0020,
	/** Group write permission */
	FIO_S_IWGRP		= 0x0010,
	/** Group execute permission */
	FIO_S_IXGRP		= 0x0008,

	/** Others access rights mask */
	FIO_S_IRWXO		= 0x0007,	
	/** Others read permission */
	FIO_S_IROTH		= 0x0004,	
	/** Others write permission */
	FIO_S_IWOTH		= 0x0002,	
	/** Others execute permission */
	FIO_S_IXOTH		= 0x0001,	
};

/** File modes, used for the st_attr parameter in SceIoStat (confirm?). */
enum IOFileModes
{
	/** Format mask */
	FIO_SO_IFMT			= 0x0038,		// Format mask
	/** Symlink */
	FIO_SO_IFLNK		= 0x0008,		// Symbolic link
	/** Directory */
	FIO_SO_IFDIR		= 0x0010,		// Directory
	/** Regular file */
	FIO_SO_IFREG		= 0x0020,		// Regular file

	/** Hidden read permission */
	FIO_SO_IROTH		= 0x0004,		// read
	/** Hidden write permission */
	FIO_SO_IWOTH		= 0x0002,		// write
	/** Hidden execute permission */
	FIO_SO_IXOTH		= 0x0001,		// execute
};

typedef struct ScePspDateTime {
	uint16_t	year;
	uint16_t 	month;
	uint16_t 	day;
	uint16_t	hour;
	uint16_t	minute;
	uint16_t	second;
	uint32_t	microsecond;
} ScePspDateTime;

#define PSP_CHSTAT_MODE   0x01
#define PSP_CHSTAT_ATTR   0x02
#define PSP_CHSTAT_SIZE   0x04
#define PSP_CHSTAT_CTIME  0x08
#define PSP_CHSTAT_ATIME  0x10
#define PSP_CHSTAT_MTIME  0x20

/** Structure to hold the status information about a file */
typedef struct SceIoStat {
	uint32_t 	mode;
	uint32_t 	attr;
	/** Size of the file in bytes. */
	int64_t		size;
	/** Creation time. */
	ScePspDateTime 	ctime;
	/** Access time. */
	ScePspDateTime 	atime;
	/** Modification time. */
	ScePspDateTime 	mtime;
	/** Device-specific data. */
	uint32_t 	st_private[6];
} SceIoStat;

typedef struct SceIoDirent {
	/** File status. */
	SceIoStat 	stat;
	/** File name. */
	char 		name[256];
	/** Device-specific data. */
	uint32_t 	private;
	uint32_t	dummy;
} SceIoDirent;

#endif
