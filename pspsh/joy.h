/* RemoteJoy controller events sent by pspsh over the host USB bridge. */
#ifndef PSPSH_JOY_H
#define PSPSH_JOY_H

#include <stdint.h>

enum JoyCommandDuration
{
	JOY_DURATION_MIN_MS = 1,
	JOY_DURATION_DEFAULT_MS = 120,
	JOY_DURATION_MAX_MS = 10000
};

enum JoyCommandType
{
	JOY_COMMAND_PRESS,
	JOY_COMMAND_ANALOG,
	JOY_COMMAND_RELEASE
};

struct JoyCommand
{
	enum JoyCommandType type;
	uint32_t buttons;
	unsigned int x;
	unsigned int y;
	unsigned int duration_ms;
};

/* Parse the args following "joy"; return 1 for a valid command. */
int joy_parse(int argc, char **argv, struct JoyCommand *command);

/* Send the command to an already connected RemoteJoy channel (0 or -1). */
int joy_send(int socket, const struct JoyCommand *command);

#endif
