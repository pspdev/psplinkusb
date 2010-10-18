/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * remotejoy.c - PSPLINK PC remote joystick handler (SDL Version)
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.pspdev.org/psp/branches/psplinkusb/tools/remotejoy/pcsdl/remotejoy.c $
 * $Id: remotejoy.c 2187 2007-02-20 19:28:00Z tyranid $
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <SDL.h>
#include <SDL_thread.h>
#include "../remotejoy.h"

#define DEFAULT_PORT 10004
#define DEFAULT_IP   "localhost"

#define MAX_AXES_NUM 32767
#define DIGITAL_TOL   10000

#define PSP_SCREEN_W  480
#define PSP_SCREEN_H  272

#define EVENT_ENABLE_SCREEN   1
#define EVENT_RENDER_FRAME_1  2
#define EVENT_RENDER_FRAME_2  3
#define EVENT_DISABLE_SCREEN  4

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
	/** Music Note button. */
	PSP_CTRL_NOTE       = 0x800000,
	/** Screen button. */
	PSP_CTRL_SCREEN     = 0x400000,
	/** Volume up button. */
	PSP_CTRL_VOLUP      = 0x100000,
	/** Volume down button. */
	PSP_CTRL_VOLDOWN    = 0x200000,
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
	PSP_BUTTON_HOME  = 12,
	PSP_BUTTON_NOTE  = 13,
	PSP_BUTTON_SCREEN = 14,
	PSP_BUTTON_VOLUP  = 15,
	PSP_BUTTON_VOLDOWN = 16,
	PSP_BUTTON_MAX   = 17
};

unsigned int g_bitmap[PSP_BUTTON_MAX] = {
	PSP_CTRL_CROSS, PSP_CTRL_CIRCLE, PSP_CTRL_TRIANGLE, PSP_CTRL_SQUARE,
	PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER, PSP_CTRL_START, PSP_CTRL_SELECT,
	PSP_CTRL_UP, PSP_CTRL_DOWN, PSP_CTRL_LEFT, PSP_CTRL_RIGHT, PSP_CTRL_HOME,
	PSP_CTRL_NOTE, PSP_CTRL_SCREEN, PSP_CTRL_VOLUP, PSP_CTRL_VOLDOWN
};

const char *map_names[PSP_BUTTON_MAX] = {
	"cross", "circle", "triangle", "square",
	"ltrig", "rtrig", "start", "select",
	"up", "down", "left", "right", "home", 
	"note", "screen", "volup", "voldown"
};

/* Maps the buttons on the joystick to the buttons on the PSP controller */
unsigned int *g_buttmap = NULL;

struct Args
{
	const char *ip;
	unsigned short port;
	const char *dev;
	const char *mapfile;
	int verbose;
	int video;
	int fullscreen;
	int droprate;
	int fullcolour;
	int halfsize;
	int showfps;
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
	int scron;
};

struct GlobalContext g_context;

struct ScreenBuffer
{
	unsigned char buf[PSP_SCREEN_W * PSP_SCREEN_H * 4];
	struct JoyScrHeader head;
	/* Mutex? */
};

static struct ScreenBuffer g_buffers[2];

void init_font(void);
void print_text(SDL_Surface *screen, int x, int y, const char *fmt, ...);

/* Should have a mutex on each screen */

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

		ch = getopt(argc, argv, "vsfchldp:i:m:r:");

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
			case 'v': args->verbose = 1;
					  break;
			case 'd': args->video = 1;
					  break;
			case 'f': args->fullscreen = 1;
					  break;
			case 'c': args->fullcolour = 1;
					  break;
			case 'l': args->halfsize = 1;
					  break;
			case 's': args->showfps = 1;
					  break;
			case 'r': args->droprate = atoi(optarg);
					  if((args->droprate < 0) || (args->droprate > 59))
					  {
						  fprintf(stderr, "Invalid drop rate (0 <= r < 60)\n");
						  error = 1;
					  }
					  break;
			case 'h': 
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

	return 1;
}

void print_help(void)
{
	fprintf(stderr, "Remotejoy Help\n");
	fprintf(stderr, "Usage: remotejoy [options]\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-p port     : Specify the port number\n");
	fprintf(stderr, "-i ip       : Specify the ip address (default %s)\n", DEFAULT_IP);
	fprintf(stderr, "-m mapfile  : Specify a file to map joystick buttons to the PSP\n");
	fprintf(stderr, "-d          : Auto enable display support\n");
	fprintf(stderr, "-f          : Full screen mode\n");
	fprintf(stderr, "-r drop     : Frame Skip, 0 (auto), 1 (1/2), 2 (1/3), 3(1/4) etc.\n");
	fprintf(stderr, "-c          : Full colour mode\n");
	fprintf(stderr, "-l          : Half size mode (both X and Y)\n");
	fprintf(stderr, "-s          : Show fps\n");
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

int get_joyinfo(SDL_Joystick *stick)
{
	const char *name;

	name = SDL_JoystickName(0);
	if(!name)
	{
		return 0;
	}

	strcpy(g_context.name, name);
	
	g_context.axes = SDL_JoystickNumAxes(stick);
	g_context.buttons = SDL_JoystickNumButtons(stick);

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
	g_context.digital = -1;
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
			for(i = 0; i < PSP_BUTTON_MAX; i++)
			{
				if(strcasecmp(map_names[i], tok) == 0)
				{
					g_buttmap[butt] = i;
					break;
				}
			}

			if(i == PSP_BUTTON_MAX)
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

void post_event(int no)
{
	SDL_Event event;

	event.type = SDL_USEREVENT;
	event.user.code = no;
	event.user.data1 = NULL;
	event.user.data2 = NULL;

	SDL_PushEvent(&event);
}

int flush_socket(int sock)
{
	/* If we encounter some horrible error which means we are desynced
	 * then send a video off packet to remotejoy, wait around for a second sucking up
	 * any more data from the socket and then reenable */

	return 0;
}

void update_fps(SDL_Surface *screen)
{
#define FRAME_VALUES 32
	static Uint32 times[FRAME_VALUES];
	static Uint32 lastticks = 0;
	static int index = 0;
	Uint32 ticks;
	int i;
	double fps;

	ticks = SDL_GetTicks();
	times[index] = ticks - lastticks;
	index = (index + 1) % FRAME_VALUES;
	lastticks = ticks;

	fps = 0.0;
	for(i = 0; i < FRAME_VALUES; i++)
	{
		fps += (double) times[i];
	}
	fps /= (double) FRAME_VALUES;
	/* Fps is now average frame time */
	fps = 1000.0 / fps;
	/* Now frame frequency in Hz */

	print_text(screen, 0, 0, "Fps: %.2f", fps);
}

int read_thread(void *p)
{
	int err = 0;
	int frame = 0;
	fd_set saveset, readset;
	int count;
	int sock = *(int *) p;
	struct JoyScrHeader head;

	FD_ZERO(&saveset);
	FD_SET(sock, &saveset);

	while(!err)
	{
		readset = saveset;

		count = select(FD_SETSIZE, &readset, NULL, NULL, NULL);

		if(count > 0)
		{
			int ret;
			int mode;
			int size;

			if(FD_ISSET(sock, &readset))
			{
				ret = read(sock, &head, sizeof(head));
				if((ret != sizeof(head)) || (LE32(head.magic) != JOY_MAGIC))
				{
					fprintf(stderr, "Error in socket %d, magic %08X\n", ret, head.magic);
					flush_socket(sock);
					break;
				}

				mode = LE32(head.mode);
				size = LE32(head.size);
				g_buffers[frame].head.mode = mode;
				g_buffers[frame].head.size = size;

				if(mode < 0)
				{
					if(g_context.args.video)
					{
						post_event(EVENT_ENABLE_SCREEN);
					}
					else
					{
						g_context.scron = 0;
					}
				}
				else if(mode > 3)
				{
					/* Flush socket */
					flush_socket(sock);
				}
				else
				{
					/* Try and read in screen */
					/* If we do not get a full frame read and we timeout in quater second or so then
					 * reset sync as it probably means the rest isn't coming */
					int loc = 0;

					//fprintf(stderr, "Size %d\n", size);
					while(1)
					{
						readset = saveset;

						/* Should have a time out */
						count = select(FD_SETSIZE, &readset, NULL, NULL, NULL);
						if(count > 0)
						{
							ret = read(sock, &(g_buffers[frame].buf[loc]), size-loc);
							if(ret < 0)
							{
								if(errno != EINTR)
								{
									perror("read:");
									err = 1;
									break;
								}
							}
							else if(ret == 0)
							{
								fprintf(stderr, "EOF\n");
								break;
							}

							//fprintf(stderr, "Read %d\n", loc);
							loc += ret;
							if(loc == size)
							{
								break;
							}
						}
						else if(count < 0)
						{
							if(errno != EINTR)
							{
								perror("select:");
								err = 1;
								break;
							}
						}
					}

					if(!err)
					{
						if(frame)
						{
							post_event(EVENT_RENDER_FRAME_2);
						}
						else
						{
							post_event(EVENT_RENDER_FRAME_1);
						}
						frame ^= 1;
					}
				}
			}
		}
		else if(count < 0)
		{
			if(errno != EINTR)
			{
				perror("select:");
				err = 1;
			}
		}
	}

	return 0;
}

SDL_Surface *create_surface(void *buf, int mode)
{
	unsigned int rmask, bmask, gmask, amask;
	int currw, currh;
	int bpp;

	currw = PSP_SCREEN_W;
	currh = PSP_SCREEN_H;
	if(g_context.args.halfsize)
	{
		currw >>= 1;
		currh >>= 1;
	}

	if(VERBOSE)
	{
		printf("Mode %d\n", mode);
	}

	switch(mode)
	{
		case 3: 
			rmask = LE32(0x000000FF);
			gmask = LE32(0x0000FF00);
			bmask = LE32(0x00FF0000);
			amask = 0;
			bpp = 32;
			break;
		case 2: 
			rmask = LE16(0x000F);
			gmask = LE16(0x00F0);
			bmask = LE16(0x0F00);
			amask = 0;
			bpp = 16;
			break;
		case 1: 
			rmask = LE16(0x1F);
			gmask = LE16(0x1F << 5);
			bmask = LE16(0x1F << 10);
			amask = 0;
			bpp = 16;
			break;
		case 0: 
			rmask = LE16(0x1F);
			gmask = LE16(0x3F << 5);
			bmask = LE16(0x1F << 11);
			amask = 0;
			bpp = 16;
			break;
		default: return NULL;
	};

	return SDL_CreateRGBSurfaceFrom(buf, currw, currh, bpp, currw*(bpp/8), 
			rmask, gmask, bmask, amask);	
}

void save_screenshot(SDL_Surface *surface)
{
	int i;
	char path[PATH_MAX];
	struct stat s;

	/* If we cant find one in the next 1000 then dont bother */
	for(i = 0; i < 1000; i++)
	{
		snprintf(path, PATH_MAX, "scrshot%03d.bmp", i);
		if(stat(path, &s) < 0)
		{
			break;
		}
	}

	if(i == 1000)
	{
		return;
	}

	if(SDL_SaveBMP(surface, path) == 0)
	{
		printf("Saved screenshot to %s\n", path);
	}
	else
	{
		printf("Error saving screenshot\n");
	}
}

void mainloop(void)
{
	SDL_Joystick *stick = NULL;
	SDL_Surface *screen = NULL;
	SDL_Surface *buf1 = NULL;
	SDL_Surface *buf2 = NULL;
	SDL_Thread *thread = NULL;
	int currw, currh;
	int sdl_init = 0;
	int sock = -1;
	unsigned int button_state = 0;
	int currmode[2] = { 3, 3 };
	int flags = SDL_HWSURFACE;
	int pspflags = 0;
	int showfps = 0;

	if(g_context.args.fullscreen)
	{
		flags |= SDL_FULLSCREEN;
	}

	if(g_context.args.fullcolour)
	{
		pspflags = SCREEN_CMD_FULLCOLOR;
	}

	if(g_context.args.halfsize)
	{
		pspflags |= SCREEN_CMD_HSIZE;
	}

	if(g_context.args.droprate)
	{
		pspflags |= SCREEN_CMD_DROPRATE(g_context.args.droprate);
	}

	showfps = g_context.args.showfps;

	do
	{
		if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK))
		{
			fprintf(stderr, "Could not initialise SDL\n");
			break;
		}

		currw = PSP_SCREEN_W;
		currh = PSP_SCREEN_H;
		if(g_context.args.halfsize)
		{
			currw >>= 1;
			currh >>= 1;
		}

		screen = SDL_SetVideoMode(currw, currh, 0, flags);
		if(screen == NULL)
		{
			break;
		}
		init_font();

		SDL_ShowCursor(SDL_DISABLE);

		/* Pre-build buffer, we might change these later */
		buf1 = create_surface(g_buffers[0].buf, 3);
		if(buf1 == NULL)
		{
			break;
		}

		buf2 = create_surface(g_buffers[1].buf, 3);
		if(buf2 == NULL)
		{
			break;
		}

		SDL_WM_SetCaption("RemoteJoySDL - Press any ESC key to exit", NULL);

		sdl_init = 1;

		if(SDL_NumJoysticks() > 0)
		{
			stick = SDL_JoystickOpen(0);
			if(!stick)
			{
				break;
			}

			if(!get_joyinfo(stick))
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
		}

		sock = connect_to(g_context.args.ip, g_context.args.port);
		if(sock < 0)
		{
			break;
		}

		thread = SDL_CreateThread(read_thread, (void*) &sock);
		if(thread == NULL)
		{
			break;
		}

		while(!g_context.exit)
		{
			SDL_Event event;

			if(!SDL_WaitEvent(&event))
			{
				break;
			}

			if(event.type == SDL_VIDEORESIZE)
			{
				SDL_FreeSurface(screen);
				screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 0, flags);
				currw = event.resize.w;
				currh = event.resize.h;
			}

			if(event.type == SDL_USEREVENT)
			{
				switch(event.user.code)
				{
					case EVENT_ENABLE_SCREEN:
						send_event(sock, TYPE_SCREEN_CMD, SCREEN_CMD_ACTIVE | pspflags);
						g_context.scron = 1;
						break;
					case EVENT_DISABLE_SCREEN:
						send_event(sock, TYPE_SCREEN_CMD, 0);
						g_context.scron = 0;
						break;
					case EVENT_RENDER_FRAME_1:
						if(currmode[0] != g_buffers[0].head.mode)
						{
							SDL_FreeSurface(buf1);
							buf1 = create_surface(g_buffers[0].buf, g_buffers[0].head.mode);
							currmode[0] = g_buffers[0].head.mode;
						}
						SDL_BlitSurface(buf1, NULL, screen, NULL);
						if(showfps)
						{
							update_fps(screen);
						}
						SDL_UpdateRect(screen, 0, 0, currw, currh);
						break;
					case EVENT_RENDER_FRAME_2:
						if(currmode[1] != g_buffers[1].head.mode)
						{
							SDL_FreeSurface(buf2);
							buf2 = create_surface(g_buffers[1].buf, g_buffers[1].head.mode);
							currmode[1] = g_buffers[1].head.mode;
						}
						SDL_BlitSurface(buf2, NULL, screen, NULL);
						if(showfps)
						{
							update_fps(screen);
						}
						SDL_UpdateRect(screen, 0, 0, currw, currh);
						break;
					default:
						fprintf(stderr, "Error, invalid event type\n");
				};
				continue;
			}

			if((event.type == SDL_KEYDOWN) || (event.type == SDL_KEYUP))
			{
				unsigned int bitmap = 0;
				SDL_KeyboardEvent *key = (SDL_KeyboardEvent *) &event;

				if(key->keysym.sym == SDLK_ESCAPE)
				{
					break;
				}

				if(key->keysym.sym == SDLK_F8)
				{
					if(event.type == SDL_KEYDOWN)
					{
						SDL_WM_ToggleFullScreen(screen);
					}

					continue;
				}

				if(key->keysym.sym == SDLK_F3)
				{
					if(event.type == SDL_KEYDOWN)
					{
						if(VERBOSE)
						{
							printf("Switch FullColour Mode\n");
						}
						pspflags ^= SCREEN_CMD_FULLCOLOR;
						send_event(sock, TYPE_SCREEN_CMD, SCREEN_CMD_ACTIVE | pspflags);
						g_context.scron = 1;
						g_context.args.fullcolour ^= 1;
					}
				}

				if(key->keysym.sym == SDLK_F4)
				{
					if(event.type == SDL_KEYDOWN)
					{
						if(VERBOSE)
						{
							printf("Switch Halfsize Mode\n");
						}
						currw = PSP_SCREEN_W;
						currh = PSP_SCREEN_H;
						if((pspflags & SCREEN_CMD_HSIZE) == 0)
						{
							currw >>= 1;
							currh >>= 1;
						}
						pspflags ^= SCREEN_CMD_HSIZE;
						SDL_FreeSurface(screen);
						screen = SDL_SetVideoMode(currw, currh, 0, flags);
						send_event(sock, TYPE_SCREEN_CMD, SCREEN_CMD_ACTIVE | pspflags);
						g_context.scron = 1;
						g_context.args.halfsize ^= 1;
						/* Force change of buffers */
						currmode[0] = -1;
						currmode[1] = -1;
					}
				}

				if(key->keysym.sym == SDLK_F5)
				{
					if(event.type == SDL_KEYDOWN)
					{
						if(g_context.scron)
						{
							if(VERBOSE)
							{
								printf("Disable Screen\n");
							}
							send_event(sock, TYPE_SCREEN_CMD, 0);
							g_context.scron = 0;
						}
						else
						{
							if(VERBOSE)
							{
								printf("Enable Screen\n");
							}
							send_event(sock, TYPE_SCREEN_CMD, SCREEN_CMD_ACTIVE | pspflags);
							g_context.scron = 1;
						}
					}

					continue;
				}

				if(key->keysym.sym == SDLK_F9)
				{
					if(event.type == SDL_KEYDOWN)
					{
						showfps ^= 1;
					}
				}

				if(key->keysym.sym == SDLK_F10)
				{
					if(event.type == SDL_KEYDOWN)
					{
						save_screenshot(screen);
					}
				}

				switch(key->keysym.sym)
				{
					case SDLK_LEFT: bitmap = PSP_CTRL_LEFT;
									break;
					case SDLK_RIGHT: bitmap = PSP_CTRL_RIGHT;
									 break;
					case SDLK_UP: bitmap = PSP_CTRL_UP;
								  break;
					case SDLK_DOWN: bitmap = PSP_CTRL_DOWN;
									break;
					case SDLK_a: bitmap = PSP_CTRL_SQUARE;
								 break;
					case SDLK_s: bitmap = PSP_CTRL_TRIANGLE;
								 break;
					case SDLK_z: bitmap = PSP_CTRL_CROSS;
								 break;
					case SDLK_x: bitmap = PSP_CTRL_CIRCLE;
								 break;
					case SDLK_q: bitmap = PSP_CTRL_LTRIGGER;
								 break;
					case SDLK_w: bitmap = PSP_CTRL_RTRIGGER;
								 break;
					case SDLK_RETURN: bitmap = PSP_CTRL_START;
									 break;
					case SDLK_SPACE: bitmap = PSP_CTRL_SELECT;
									 break;
					case SDLK_y: bitmap = PSP_CTRL_HOME;
								 break;
					case SDLK_u: bitmap = PSP_CTRL_VOLDOWN;
								 break;
					case SDLK_i: bitmap = PSP_CTRL_VOLUP;
								 break;
					case SDLK_o: bitmap = PSP_CTRL_SCREEN;
								 break;
					case SDLK_p: bitmap = PSP_CTRL_NOTE;
								 break;
					default: break;
				};

				if(event.type == SDL_KEYDOWN)
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
			}
			   
			if(event.type == SDL_QUIT)
			{
				break;
			}

			if((event.type == SDL_JOYBUTTONDOWN) || (event.type == SDL_JOYBUTTONUP))
			{
				unsigned int bitmap = g_bitmap[g_buttmap[event.jbutton.button]];
				if(event.type == SDL_JOYBUTTONDOWN)
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
					printf("Button %d event\n", event.jbutton.button);
					printf("Button state: %08X\n", button_state);
				}
			}
			else if(event.type == SDL_JOYAXISMOTION)
			{
				unsigned int curr_butt = 0;

				if(VERBOSE)
				{
					printf("Axis %d event\n", event.jaxis.axis/2);
				}

				if(g_context.digital == (event.jaxis.axis/2))
				{
					if(event.jaxis.axis & 1)
					{
						if(event.jaxis.value > g_context.tol)
						{
							curr_butt = PSP_CTRL_DOWN;
						}
						else if(event.jaxis.value < -g_context.tol)
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
						if(event.jaxis.value > g_context.tol)
						{
							curr_butt = PSP_CTRL_RIGHT;
						}
						else if(event.jaxis.value < -g_context.tol)
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

				if(g_context.analog == (event.jaxis.axis / 2))
				{
					int val = event.jaxis.value + MAX_AXES_NUM;
					if(event.jaxis.axis & 1)
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

	if(stick)
	{
		SDL_JoystickClose(stick);
	}

	if(buf1)
	{
		SDL_FreeSurface(buf1);
		buf1 = NULL;
	}

	if(buf2)
	{
		SDL_FreeSurface(buf2);
		buf2 = NULL;
	}

	if(thread)
	{
		SDL_KillThread(thread);
	}

	if(sdl_init)
	{
		SDL_Quit();
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

int main(int argc, char **argv)
{
	fprintf(stderr, "Remote Joy SDL for PSP (c) TyRaNiD 2k6\n");
	fprintf(stderr, "Built %s %s - $Revision: 2398 $\n", __DATE__, __TIME__);
	memset(&g_context, 0, sizeof(g_context));
	if(parse_args(argc, argv, &g_context.args))
	{
		mainloop();
	}
	else
	{
		print_help();
	}

	return 0;
}
