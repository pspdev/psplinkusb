/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * remotejoy.h - PSPLINK PC remote joystick handler
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 */
#ifndef __REMOTE_JOY__
#define __REMOTE_JOY__

#include <stdint.h>

#define JOY_MAGIC 0x909ACCEF

#define TYPE_BUTTON_DOWN 1
#define TYPE_BUTTON_UP   2
#define TYPE_ANALOG_Y    3
#define TYPE_ANALOG_X    4
#define TYPE_SCREEN_CMD  5

/* RemoteJoy carries an unsigned 8-bit analog axis in its 32-bit value field. */
enum JoyAnalogAxis
{
	JOY_ANALOG_MIN = 0,
	JOY_ANALOG_MAX = UINT8_MAX,
	JOY_ANALOG_LEGACY_CENTER = JOY_ANALOG_MAX / 2,
	JOY_ANALOG_CENTER = JOY_ANALOG_LEGACY_CENTER + 1
};

/* Screen commands */
#define SCREEN_CMD_ACTIVE 1
#define SCREEN_CMD_HSIZE  2
#define SCREEN_CMD_FULLCOLOR  4
#define SCREEN_CMD_DROPRATE(x) ((x)<<24)
#define SCREEN_CMD_GETDROPRATE(x) ((x)>>24)

struct JoyEvent
{
	uint32_t magic;
	int32_t type;
	uint32_t value;
} __attribute__((packed));

struct JoyScrHeader
{
	uint32_t magic;
	int32_t mode; /* 0-3 */
	int32_t size;
	int32_t ref;
} __attribute__((packed));

#endif
