/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * remotejoy.h - PSPLINK PC remote joystick handler
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/tools/remotejoy/remotejoy.h $
 * $Id: remotejoy.h 2186 2007-02-20 04:09:44Z tyranid $
 */
#ifndef __REMOTE_JOY__
#define __REMOTE_JOY__

#define JOY_MAGIC 0x909ACCEF

#define TYPE_BUTTON_DOWN 1
#define TYPE_BUTTON_UP   2
#define TYPE_ANALOG_Y    3
#define TYPE_ANALOG_X    4
#define TYPE_SCREEN_CMD  5

/* Screen commands */
#define SCREEN_CMD_ACTIVE 1
#define SCREEN_CMD_HSIZE  2
#define SCREEN_CMD_FULLCOLOR  4
#define SCREEN_CMD_DROPRATE(x) ((x)<<24)
#define SCREEN_CMD_GETDROPRATE(x) ((x)>>24)

struct JoyEvent
{
	unsigned int magic;
	int type;
	unsigned int value;
} __attribute__((packed));

struct JoyScrHeader
{
	unsigned int magic;
	int mode; /* 0-3 */
	int size;
	int ref;
} __attribute__((packed));

#endif
