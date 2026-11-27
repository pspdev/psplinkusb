/* RemoteJoy input support for pspsh. See tools/remotejoy/remotejoy.h. */
#include "joy.h"
#include "../tools/remotejoy/remotejoy.h"
#include "pspctrl.h"

#include <errno.h>
#include <limits.h>
#include <chrono>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

struct ButtonName
{
	const char *name;
	uint32_t mask;
};

static const struct ButtonName buttons[] = {
	{ "select",   PSP_CTRL_SELECT },
	{ "start",    PSP_CTRL_START },
	{ "up",       PSP_CTRL_UP },
	{ "right",    PSP_CTRL_RIGHT },
	{ "down",     PSP_CTRL_DOWN },
	{ "left",     PSP_CTRL_LEFT },
	{ "ltrigger", PSP_CTRL_LTRIGGER },
	{ "rtrigger", PSP_CTRL_RTRIGGER },
	{ "triangle", PSP_CTRL_TRIANGLE },
	{ "circle",   PSP_CTRL_CIRCLE },
	{ "cross",    PSP_CTRL_CROSS },
	{ "square",   PSP_CTRL_SQUARE }
};

static const unsigned int send_timeout_seconds = 2;
static const std::chrono::milliseconds interrupt_poll_interval(20);
static volatile sig_atomic_t interrupted;

static void interrupt_joy(int sig)
{
	(void)sig;
	interrupted = 1;
}

static void joy_usage(void)
{
	fprintf(stderr, "Usage: joy press buttons [ms] | analog x y [ms] | release\n");
	fprintf(stderr, "Buttons: cross circle square triangle up down left right "
			"ltrigger rtrigger start select (combine with +)\n");
	fprintf(stderr, "Duration: %u-%u ms; analog axes: %u-%u (%u is center)\n",
			(unsigned int)JOY_DURATION_MIN_MS, (unsigned int)JOY_DURATION_MAX_MS,
			(unsigned int)JOY_ANALOG_MIN, (unsigned int)JOY_ANALOG_MAX,
			(unsigned int)JOY_ANALOG_CENTER);
	fprintf(stderr, "Load remotejoy.prx on the PSP before sending input.\n");
}

static int parse_number(const char *text, unsigned int min, unsigned int max,
						unsigned int *value)
{
	char *end;
	unsigned long parsed;

	if(!text || !*text || *text == '-')
	{
		return 0;
	}

	errno = 0;
	parsed = strtoul(text, &end, 10);
	if(errno != 0 || *end != 0 || parsed < min || parsed > max)
	{
		return 0;
	}

	*value = (unsigned int) parsed;
	return 1;
}

static int parse_buttons(const char *text, uint32_t *mask)
{
	const char *part = text;
	uint32_t result = 0;

	if(!text || !*text)
	{
		return 0;
	}

	while(*part)
	{
		const char *end = strchr(part, '+');
		size_t length = end ? (size_t)(end - part) : strlen(part);
		size_t i;

		if(length == 0)
		{
			return 0;
		}

		for(i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
		{
			if(strlen(buttons[i].name) == length &&
			   strncmp(part, buttons[i].name, length) == 0)
			{
				result |= buttons[i].mask;
				break;
			}
		}

		if(i == sizeof(buttons) / sizeof(buttons[0]))
		{
			fprintf(stderr, "Unknown RemoteJoy button: %.*s\n", (int)length, part);
			return 0;
		}

		if(!end)
		{
			break;
		}

		part = end + 1;
		if(!*part)
		{
			return 0;
		}
	}

	*mask = result;
	return 1;
}

int joy_parse(int argc, char **argv, struct JoyCommand *command)
{
	if(!command || argc <= 0 || !argv)
	{
		joy_usage();
		return 0;
	}

	memset(command, 0, sizeof(*command));

	if(strcmp(argv[0], "press") == 0 && (argc == 2 || argc == 3))
	{
		command->type = JOY_COMMAND_PRESS;
		command->duration_ms = JOY_DURATION_DEFAULT_MS;
		if(!parse_buttons(argv[1], &command->buttons) ||
		   (argc == 3 && !parse_number(argv[2], JOY_DURATION_MIN_MS, JOY_DURATION_MAX_MS,
									   &command->duration_ms)))
		{
			joy_usage();
			return 0;
		}
	}
	else if(strcmp(argv[0], "analog") == 0 && (argc == 3 || argc == 4))
	{
		command->type = JOY_COMMAND_ANALOG;
		command->duration_ms = JOY_DURATION_DEFAULT_MS;
		if(!parse_number(argv[1], JOY_ANALOG_MIN, JOY_ANALOG_MAX, &command->x) ||
		   !parse_number(argv[2], JOY_ANALOG_MIN, JOY_ANALOG_MAX, &command->y) ||
		   (argc == 4 && !parse_number(argv[3], JOY_DURATION_MIN_MS, JOY_DURATION_MAX_MS,
									   &command->duration_ms)))
		{
			joy_usage();
			return 0;
		}
	}
	else if(strcmp(argv[0], "release") == 0 && argc == 1)
	{
		command->type = JOY_COMMAND_RELEASE;
	}
	else
	{
		joy_usage();
		return 0;
	}

	return 1;
}

static int send_event(int sock, uint32_t type, uint32_t value)
{
	static_assert(sizeof(struct JoyEvent) == 12, "Unexpected RemoteJoy packet size");
	unsigned char packet[sizeof(struct JoyEvent)];
	const uint32_t fields[] = { JOY_MAGIC, type, value };
	size_t offset = 0;
	size_t i;

	/* The PSP RemoteJoy protocol uses little-endian 32-bit fields. */
	for(i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
	{
		unsigned int j;
		for(j = 0; j < sizeof(fields[i]); ++j)
		{
			packet[offset++] = (unsigned char)(fields[i] >> (j * CHAR_BIT));
		}
	}

	offset = 0;
	while(offset < sizeof(packet))
	{
		ssize_t sent = send(sock, packet + offset, sizeof(packet) - offset, 0);
		if(sent < 0 && errno == EINTR)
		{
			continue;
		}
		if(sent <= 0)
		{
			if(sent == 0)
			{
				errno = EPIPE;
			}
			perror("RemoteJoy send");
			fprintf(stderr, "Input may remain active; reconnect and run joy release.\n");
			return -1;
		}
		offset += (size_t)sent;
	}

	return 0;
}

static int delay_ms(unsigned int duration)
{
	using namespace std::chrono;
	struct timespec now;
	if(clock_gettime(CLOCK_MONOTONIC, &now) < 0)
	{
		perror("RemoteJoy clock");
		return -1;
	}
	const nanoseconds deadline = seconds(now.tv_sec) + nanoseconds(now.tv_nsec) + milliseconds(duration);
	while(!interrupted)
	{
		if(clock_gettime(CLOCK_MONOTONIC, &now) < 0)
		{
			perror("RemoteJoy clock");
			return -1;
		}
		const nanoseconds remaining = deadline - (seconds(now.tv_sec) + nanoseconds(now.tv_nsec));
		if(remaining <= nanoseconds::zero())
		{
			return 0;
		}
		/* Bound the signal/sleep race without accumulating scheduler delays. */
		const nanoseconds max_slice = interrupt_poll_interval;
		struct timespec slice = { 0, (long)(remaining < max_slice ? remaining : max_slice).count() };
		if(nanosleep(&slice, NULL) < 0 && errno != EINTR)
		{
			perror("RemoteJoy delay");
			return -1;
		}
	}
	return -1;
}

static int send_command(int sock, const struct JoyCommand *command)
{
	int ret = 0;
	struct timeval timeout;

	if(sock < 0 || !command)
	{
		return -1;
	}

	timeout.tv_sec = send_timeout_seconds;
	timeout.tv_usec = 0;
	if(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
	{
		perror("RemoteJoy send timeout");
		return -1;
	}

	switch(command->type)
	{
		case JOY_COMMAND_PRESS:
			if(send_event(sock, TYPE_BUTTON_DOWN, command->buttons) < 0)
			{
				return -1;
			}
			ret = delay_ms(command->duration_ms);
			if(send_event(sock, TYPE_BUTTON_UP, command->buttons) < 0)
			{
				return -1;
			}
			return ret;

		case JOY_COMMAND_ANALOG:
			if(send_event(sock, TYPE_ANALOG_X, command->x) < 0)
			{
				return -1;
			}
			ret = send_event(sock, TYPE_ANALOG_Y, command->y);
			if(ret == 0)
			{
				ret = delay_ms(command->duration_ms);
			}
			if(send_event(sock, TYPE_ANALOG_X, JOY_ANALOG_CENTER) < 0)
			{
				ret = -1;
			}
			if(send_event(sock, TYPE_ANALOG_Y, JOY_ANALOG_CENTER) < 0)
			{
				return -1;
			}
			return ret;

		case JOY_COMMAND_RELEASE:
		{
			uint32_t mask = 0;
			size_t i;
			for(i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
			{
				mask |= buttons[i].mask;
			}
			if(send_event(sock, TYPE_BUTTON_UP, mask) < 0 ||
			   send_event(sock, TYPE_ANALOG_X, JOY_ANALOG_CENTER) < 0 ||
			   send_event(sock, TYPE_ANALOG_Y, JOY_ANALOG_CENTER) < 0)
			{
				return -1;
			}
			return 0;
		}
	}

	return -1;
}

int joy_send(int sock, const struct JoyCommand *command)
{
	struct sigaction action, old_int, old_term, old_pipe;
	int ret;

	/* Let a catchable interruption finish the release before returning. */
	memset(&action, 0, sizeof(action));
	action.sa_handler = interrupt_joy;
	sigemptyset(&action.sa_mask);
	interrupted = 0;
	if(sigaction(SIGINT, &action, &old_int) < 0)
	{
		perror("RemoteJoy SIGINT handler");
		return -1;
	}
	if(sigaction(SIGTERM, &action, &old_term) < 0)
	{
		perror("RemoteJoy SIGTERM handler");
		sigaction(SIGINT, &old_int, NULL);
		return -1;
	}
	/* Portable across the native targets, including those without MSG_NOSIGNAL. */
	action.sa_handler = SIG_IGN;
	if(sigaction(SIGPIPE, &action, &old_pipe) < 0)
	{
		perror("RemoteJoy SIGPIPE handler");
		ret = -1;
	}
	else
	{
		ret = interrupted ? -1 : send_command(sock, command);
		sigaction(SIGPIPE, &old_pipe, NULL);
	}
	sigaction(SIGTERM, &old_term, NULL);
	sigaction(SIGINT, &old_int, NULL);
	if(interrupted)
	{
		fprintf(stderr, "RemoteJoy command interrupted\n");
		return -1;
	}
	return ret;
}
