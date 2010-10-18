/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * remotejoy.c - PSPLINK PC remote joystick handler
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/tools/remotejoy/pc/remotejoy.c $
 * $Id: remotejoy.c 2398 2008-06-07 18:57:39Z iwn $
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <linux/joystick.h>
#include "../remotejoy.h"

#define DEFAULT_PORT 10004
#define DEFAULT_IP   "localhost"

#define MAX_AXES_NUM 32767
#define DIGITAL_TOL   10000

#ifndef SOL_TCP 
   #define SOL_TCP IPPROTO_TCP 
#endif

#if defined BUILD_BIGENDIAN || defined _BIG_ENDIAN
uint16_t swap16(uint16_t i)
{
	uint8_t *p = (uint8_t *) &i;
	uint16_t ret;

	ret = (p[1] << 8) | p[0];

	return ret;
}

uint32_t swap32(uint32_t i)
{
	uint8_t *p = (uint8_t *) &i;
	uint32_t ret;

	ret = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];

	return ret;
}

uint64_t swap64(uint64_t i)
{
	uint8_t *p = (uint8_t *) &i;
	uint64_t ret;

	ret = (uint64_t) p[0] | ((uint64_t) p[1] << 8) | ((uint64_t) p[2] << 16) | ((uint64_t) p[3] << 24) 
		| ((uint64_t) p[4] << 32) | ((uint64_t) p[5] << 40) | ((uint64_t) p[6] << 48) | ((uint64_t) p[7] << 56);

	return ret;
}
#define LE16(x) swap16(x)
#define LE32(x) swap32(x)
#define LE64(x) swap64(x)
#else
#define LE16(x) (x)
#define LE32(x) (x)
#define LE64(x) (x)
#endif

enum PspCtrlButtons
{
	/** Select button. */
	PSP_CTRL_SELECT     = 0x000001,
	/** Start button. */
	PSP_CTRL_START      = 0x000008,
	/** Up D-Pad button. */
	PSP_CTRL_UP         = 0x000010,
	/** Right D-Pad button. */
	PSP_CTRL_RIGHT      = 0x000020,
	/** Down D-Pad button. */
	PSP_CTRL_DOWN      	= 0x000040,
	/** Left D-Pad button. */
	PSP_CTRL_LEFT      	= 0x000080,
	/** Left trigger. */
	PSP_CTRL_LTRIGGER   = 0x000100,
	/** Right trigger. */
	PSP_CTRL_RTRIGGER   = 0x000200,
	/** Triangle button. */
	PSP_CTRL_TRIANGLE   = 0x001000,
	/** Circle button. */
	PSP_CTRL_CIRCLE     = 0x002000,
	/** Cross button. */
	PSP_CTRL_CROSS      = 0x004000,
	/** Square button. */
	PSP_CTRL_SQUARE     = 0x008000,
	/** Home button. */
	PSP_CTRL_HOME       = 0x010000,
	/** Hold button. */
	PSP_CTRL_HOLD       = 0x020000,
	/** Music Note button. */
	PSP_CTRL_NOTE       = 0x800000,
};

enum PspButtons
{
	PSP_BUTTON_CROSS = 0,
	PSP_BUTTON_CIRCLE = 1,
	PSP_BUTTON_TRIANGLE = 2,
	PSP_BUTTON_SQUARE = 3,
	PSP_BUTTON_LTRIGGER = 4,
	PSP_BUTTON_RTRIGGER = 5,
	PSP_BUTTON_START = 6,
	PSP_BUTTON_SELECT = 7,
	PSP_BUTTON_UP = 8,
	PSP_BUTTON_DOWN = 9,
	PSP_BUTTON_LEFT = 10,
	PSP_BUTTON_RIGHT = 11,
};

unsigned int g_bitmap[12] = {
	PSP_CTRL_CROSS, PSP_CTRL_CIRCLE, PSP_CTRL_TRIANGLE, PSP_CTRL_SQUARE,
	PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER, PSP_CTRL_START, PSP_CTRL_SELECT,
	PSP_CTRL_UP, PSP_CTRL_DOWN, PSP_CTRL_LEFT, PSP_CTRL_RIGHT, 
};

const char *map_names[8] = {
	"cross", "circle", "triangle", "square",
	"ltrig", "rtrig", "start", "select",
};

/* Maps the buttons on the joystick to the buttons on the PSP controller */
unsigned int *g_buttmap = NULL;

struct Args
{
	const char *ip;
	unsigned short port;
	const char *dev;
	const char *mapfile;
	const char *buildmap;
	int verbose;
	int daemon;
};

struct GlobalContext
{
	struct Args args;
	struct sockaddr_in serv;
	char name[128];
	unsigned int version;
	unsigned char axes;
	unsigned char buttons;
	int exit;
	int digital;
	int analog;
	int tol;
};

struct GlobalContext g_context;

#define VERBOSE (g_context.args.verbose)

int fixed_write(int s, const void *buf, int len)
{
	int written = 0;

	while(written < len)
	{
		int ret;

		ret = write(s, buf+written, len-written);
		if(ret < 0)
		{
			if(errno != EINTR)
			{
				perror("write");
				written = -1;
				break;
			}
		}
		else
		{
			written += ret;
		}
	}

	return written;
}

int parse_args(int argc, char **argv, struct Args *args)
{
	memset(args, 0, sizeof(*args));
	args->ip = DEFAULT_IP;
	args->port = DEFAULT_PORT;

	while(1)
	{
		int ch;
		int error = 0;

		ch = getopt(argc, argv, "vdp:i:m:b:");

		if(ch < 0)
		{
			break;
		}

		switch(ch)
		{
			case 'p': args->port = atoi(optarg);
					  break;
			case 'i': args->ip = optarg;
					  break;
			case 'm': args->mapfile = optarg;
					  break;
			case 'b': args->buildmap = optarg;
					  break;
			case 'd': args->daemon = 1;
					  break;
			case 'v': args->verbose = 1;
					  break;
			default : error = 1;
					  break;
		};

		if(error)
		{
			return 0;
		}
	}

	argc -= optind;
	argv += optind;

	if(argc < 1)
	{
		return 0;
	}
	else
	{
		args->dev = argv[0];
	}

	return 1;
}

void print_help(void)
{
	fprintf(stderr, "Remotejoy Help\n");
	fprintf(stderr, "Usage: remotejoy [options] device\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-p port     : Specify the port number\n");
	fprintf(stderr, "-i ip       : Specify the ip address (default %s)\n", DEFAULT_IP);
	fprintf(stderr, "-m mapfile  : Specify a file to map joystick buttons to the PSP\n");
	fprintf(stderr, "-b mapfile  : Build a map file\n");
	fprintf(stderr, "-v          : Verbose mode\n");
}

int init_sockaddr(struct sockaddr_in *name, const char *ipaddr, unsigned short port)
{
	struct hostent *hostinfo;

	name->sin_family = AF_INET;
	name->sin_port = htons(port);
	hostinfo = gethostbyname(ipaddr);
	if(hostinfo == NULL)
	{
		fprintf(stderr, "Unknown host %s\n", ipaddr);
		return 0;
	}
	name->sin_addr = *(struct in_addr *) hostinfo->h_addr;

	return 1;
}

int connect_to(const char *ipaddr, unsigned short port)
{
	struct sockaddr_in name;
	int sock = -1;
	int flag = 1;

	sock = socket(PF_INET, SOCK_STREAM, 0);

	if(sock < 0)
	{
		perror("socket");
		return -1;
	}

	if(!init_sockaddr(&name, ipaddr, port))
	{
		printf("Could not initialise socket address\n");
		close(sock);
		return -1;
	}

	if(connect(sock, (struct sockaddr *) &name, sizeof(name)) < 0)
	{
		perror("connect");
		close(sock);
		return -1;
	}

	/* Disable NAGLE's algorithm to prevent the packets being joined */
	setsockopt(sock, SOL_TCP, TCP_NODELAY, &flag, sizeof(int));

	return sock;
}

int get_joyinfo(int joy)
{
	if(ioctl(joy, JSIOCGNAME(sizeof(g_context.name)), g_context.name) < 0)
	{
		perror("ioctl");
		return 0;
	}

	if(ioctl(joy, JSIOCGVERSION, &g_context.version) < 0)
	{
		perror("ioctl");
		return 0;
	}

	if(ioctl(joy, JSIOCGAXES, &g_context.axes) < 0)
	{
		perror("ioctl");
		return 0;
	}

	if(ioctl(joy, JSIOCGBUTTONS, &g_context.buttons) < 0)
	{
		perror("ioctl");
		return 0;
	}

	return 1;
}

void remove_wsp(char *buf)
{
	int len = strlen(buf);
	int i = 0;

	while(isspace(buf[i]))
	{
		i++;
	}

	if(i > 0)
	{
		len -= i;
		memmove(buf, &buf[i], len + 1);
	}

	if(len <= 0)
	{
		return;
	}

	i = len-1;
	while(isspace(buf[i]))
	{
		buf[i--] = 0;
	}
}

int build_map(const char *mapfile, int buttons)
{
	int i;
	FILE *fp;

	g_context.analog = -1;
	g_context.tol = DIGITAL_TOL;

	g_buttmap = (unsigned int *) malloc(buttons * sizeof(unsigned int));
	if(g_buttmap == NULL)
	{
		return 0;
	}

	for(i = 0; i < buttons; i++)
	{
		/* Fill with mappings, repeat if more than 8 buttons */
		g_buttmap[i] = i % 8;
	}

	if(mapfile)
	{
		char buffer[512];
		int line = 0;

		fp = fopen(mapfile, "r");
		if(fp == NULL)
		{
			fprintf(stderr, "Could not open mapfile %s\n", mapfile);
			return 0;
		}

		while(fgets(buffer, sizeof(buffer), fp))
		{
			char *tok, *val;
			int butt;
			line++;
			remove_wsp(buffer);

			if((buffer[0] == '#') || (buffer[0] == 0)) /* Comment or empty line */
			{
				continue;
			}

			tok = strtok(buffer, ":");
			val = strtok(NULL, ""); 
			if((tok == NULL) || (val == NULL))
			{
				printf("Invalid mapping on line %d\n", line);
				continue;
			}

			butt = atoi(val);
			for(i = 0; i < 8; i++)
			{
				if(strcasecmp(map_names[i], tok) == 0)
				{
					g_buttmap[butt] = i;
					break;
				}
			}

			if(i == 8)
			{
				if(strcasecmp("analog", tok) == 0)
				{
					g_context.analog = butt;
				}
				else if(strcasecmp("digital", tok) == 0)
				{
					g_context.digital = butt;
				}
				else if(strcasecmp("tol", tok) == 0)
				{
					g_context.tol = atoi(val);
				}
				else
				{
					fprintf(stderr, "Unknown map type %s\n", tok);
				}
			}
		}
		fclose(fp);
	}

	return 1;
}

int read_event(int joy, int type, struct js_event *joydata)
{
	int len;

	while(1)
	{
		len = read(joy, joydata, sizeof(struct js_event));
		if(len < 0)
		{
			if(errno != EINTR)
			{
				perror("read");
				return 0;
			}
			else
			{
				continue;
			}
		}

		if(joydata->type & JS_EVENT_INIT)
		{
			continue;
		}

		if(joydata->type & type)
		{
			break;
		}
	}

	return 1;
}

void make_mapfile(void)
{
	int joy = -1;
	struct js_event joydata;
	FILE *fp = NULL;
	int i;

	do
	{
		joy = open(g_context.args.dev, O_RDONLY);
		if(joy < 0)
		{
			perror("open");
			break;
		}

		if(!get_joyinfo(joy))
		{
			break;
		}

		if(VERBOSE)
		{
			printf("name: %s, axes: %d, buttons: %d\n", g_context.name,
					g_context.axes, g_context.buttons);
		}

		
		fp = fopen(g_context.args.buildmap, "w");
		if(fp == NULL)
		{
			fprintf(stderr, "Could not open file %s\n", g_context.args.buildmap);
			break;
		}

		fprintf(fp, "tol:%d\n", DIGITAL_TOL);

		printf("Move the stick you want for digital input (can use an analogue stick)\n");
		do
		{
			if(!read_event(joy, JS_EVENT_AXIS, &joydata))
			{
				break;
			}
		}
		while(abs(joydata.value) < DIGITAL_TOL);

		fprintf(fp, "digital:%d\n", joydata.number / 2);
		for(i = 0; i < 8; i++)
		{
			fprintf(stderr, "Press button to map to %s\n", map_names[i]);
			while(1)
			{
				if(!read_event(joy, JS_EVENT_BUTTON, &joydata))
				{
					goto error;
				}

				if(joydata.value)
				{
					break;
				}
			}
			fprintf(fp, "%s:%d\n", map_names[i], joydata.number);
		}

		printf("Move the stick you want for analog input (or press a button to disable)\n");
		while(1)
		{
			if(!read_event(joy, JS_EVENT_AXIS | JS_EVENT_BUTTON, &joydata))
			{
				break;
			}

			if(joydata.type == JS_EVENT_AXIS)
			{
				if(abs(joydata.value) < DIGITAL_TOL)
				{
					continue;
				}

				fprintf(fp, "analog:%d\n", joydata.number / 2);
				break;
			}
			else if((joydata.type == JS_EVENT_BUTTON) && (joydata.value))
			{
				break;
			}
		}
	}
	while(0);

error:

	if(joy >= 0)
	{
		close(joy);
	}

	if(fp != NULL)
	{
		fclose(fp);
	}
}

int send_event(int sock, int type, unsigned int value)
{
	struct JoyEvent event;

	if(sock < 0)
	{
		return 0;
	}

	/* Note, should swap endian */
	event.magic = LE32(JOY_MAGIC);
	event.type = LE32(type);
	event.value = LE32(value);

	if(fixed_write(sock, &event, sizeof(event)) != sizeof(event))
	{
		fprintf(stderr, "Could not write out data to socket\n");
		return 0;
	}

	return 1;
}

void mainloop(void)
{
	int joy = -1;
	int sock = -1;
	struct js_event joydata;
	unsigned int button_state = 0;

	do
	{
		joy = open(g_context.args.dev, O_RDONLY);
		if(joy < 0)
		{
			perror("open");
			break;
		}

		if(!get_joyinfo(joy))
		{
			break;
		}

		if(VERBOSE)
		{
			printf("name: %s, axes: %d, buttons: %d\n", g_context.name,
					g_context.axes, g_context.buttons);
		}

		if(!build_map(g_context.args.mapfile, g_context.buttons))
		{
			break;
		}

		sock = connect_to(g_context.args.ip, g_context.args.port);
		if(sock < 0)
		{
			break;
		}

		while(!g_context.exit)
		{
			int len;
			len = read(joy, &joydata, sizeof(joydata));
			if(len < 0)
			{
				if(errno != EINTR)
				{
					perror("read");
					break;
				}
				else
				{
					continue;
				}
			}

			joydata.type &= ~JS_EVENT_INIT;

			if(joydata.type == JS_EVENT_BUTTON)
			{
				unsigned int bitmap = g_bitmap[g_buttmap[joydata.number]];
				if(joydata.value)
				{
					button_state |= bitmap;
					if(!send_event(sock, TYPE_BUTTON_DOWN, bitmap))
					{
						break;
					}
				}
				else
				{
					button_state &= ~bitmap;
					if(!send_event(sock, TYPE_BUTTON_UP, bitmap))
					{
						break;
					}
				}

				if(VERBOSE)
				{
					printf("Button %d event\n", joydata.number);
					printf("Button state: %08X\n", button_state);
				}
			}
			else if(joydata.type == JS_EVENT_AXIS)
			{
				unsigned int curr_butt = 0;

				if(VERBOSE)
				{
					printf("Axis %d event\n", joydata.number/2);
				}

				if(g_context.digital == (joydata.number/2))
				{
					if(joydata.number & 1)
					{
						if(joydata.value > g_context.tol)
						{
							curr_butt = PSP_CTRL_DOWN;
						}
						else if(joydata.value < -g_context.tol)
						{
							curr_butt = PSP_CTRL_UP;
						}
						else
						{
							/* Do nothing */
						}

						if((button_state & PSP_CTRL_UP) && (curr_butt != PSP_CTRL_UP))
						{
							/* Send UP up */
							button_state &= ~PSP_CTRL_UP;
							if(!send_event(sock, TYPE_BUTTON_UP, PSP_CTRL_UP))
							{
								break;
							}
						}

						if((button_state & PSP_CTRL_DOWN) && (curr_butt != PSP_CTRL_DOWN))
						{
							/* Send DOWN up */
							button_state &= ~PSP_CTRL_DOWN;
							if(!send_event(sock, TYPE_BUTTON_UP, PSP_CTRL_DOWN))
							{
								break;
							}
						}

						if(((button_state & curr_butt) == 0) && (curr_butt != 0))
						{
							/* Send down */
							button_state |= curr_butt;
							if(!send_event(sock, TYPE_BUTTON_DOWN, curr_butt))
							{
								break;
							}
						}
					}
					else
					{
						if(joydata.value > g_context.tol)
						{
							curr_butt = PSP_CTRL_RIGHT;
						}
						else if(joydata.value < -g_context.tol)
						{
							curr_butt = PSP_CTRL_LEFT;
						}
						else
						{
							/* Do nothing */
						}

						if((button_state & PSP_CTRL_RIGHT) && (curr_butt != PSP_CTRL_RIGHT))
						{
							/* Send right up */
							button_state &= ~PSP_CTRL_RIGHT;
							if(!send_event(sock, TYPE_BUTTON_UP, PSP_CTRL_RIGHT))
							{
								break;
							}
						}

						if((button_state & PSP_CTRL_LEFT) && (curr_butt != PSP_CTRL_LEFT))
						{
							/* Send left up */
							button_state &= ~PSP_CTRL_LEFT;
							if(!send_event(sock, TYPE_BUTTON_UP, PSP_CTRL_LEFT))
							{
								break;
							}
						}

						if(((button_state & curr_butt) == 0) && (curr_butt != 0))
						{
							/* Send down */
							button_state |= curr_butt;
							if(!send_event(sock, TYPE_BUTTON_DOWN, curr_butt))
							{
								break;
							}
						}
					}
				}

				if(g_context.analog == (joydata.number / 2))
				{
					int val = joydata.value + MAX_AXES_NUM;
					if(joydata.number & 1)
					{
						/* Send Ly (val * 255) / (MAX_AXES_NUM * 2)) */
						if(!send_event(sock, TYPE_ANALOG_Y, (val * 255) / (MAX_AXES_NUM * 2)))
						{
							break;
						}
					}
					else
					{
						/* Send Lx (val * 255) / (MAX_AXES_NUM * 2)) */
						if(!send_event(sock, TYPE_ANALOG_X, (val * 255) / (MAX_AXES_NUM * 2)))
						{
							break;
						}
					}
				}
			}

		}
	}
	while(0);

	if(joy >= 0)
	{
		close(joy);
	}

	if(sock >= 0)
	{
		close(sock);
	}

	if(g_buttmap)
	{
		free(g_buttmap);
	}

	printf("Exiting\n");
}

void signal_handler(int sig)
{
	g_context.exit = 1;
}

void setup_signals(void)
{
	struct sigaction act;

	memset(&act, 0, sizeof(act));
	act.sa_handler = signal_handler;

	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
}

int main(int argc, char **argv)
{
	printf("Remote Joy for PSP (c) TyRaNiD 2k6\n");
	memset(&g_context, 0, sizeof(g_context));
	if(parse_args(argc, argv, &g_context.args))
	{
		if(g_context.args.buildmap)
		{
			make_mapfile();
		}
		else
		{
			if(g_context.args.daemon)
			{
				pid_t pid = fork();

				if(pid > 0)
				{
					/* Parent, just exit */
					return 0;
				}
				else if(pid < 0)
				{
					/* Error, print and exit */
					perror("fork:");
					return 1;
				}

				/* Child, dup stdio to /dev/null and set process group */
				int fd = open("/dev/null", O_RDWR);
				dup2(fd, 0);
				dup2(fd, 1);
				dup2(fd, 2);
				setsid();
			}

			setup_signals();
			mainloop();
		}
	}
	else
	{
		print_help();
	}

	return 0;
}
