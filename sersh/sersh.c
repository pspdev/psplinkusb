/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * sersh.c - PSPLINK pc serial shell
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/sersh/sersh.c $
 * $Id: sersh.c 2163 2007-02-01 21:25:53Z tyranid $
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <string.h>

#define DEFAULT_SERIAL "/dev/ttyS0"
#define BAUD_RATE	   115200

struct Args
{
	const char *ip;
	unsigned int baud;
	unsigned int realbaud;
};

struct GlobalContext
{
	struct Args args;
	int exit;
	fd_set readsave;
	fd_set writesave;
	int sock;
};

struct GlobalContext g_context;

speed_t map_int_to_speed(int baud)
{
	speed_t ret = 0;

	switch(baud)
	{
		case 4800: ret = B4800;
				   break;
		case 9600: ret = B9600;
				   break;
		case 19200: ret = B19200;
				   break;
		case 38400: ret = B38400;
				   break;
		case 57600: ret = B57600;
				   break;
		case 115200: ret = B115200;
				   break;
		default: fprintf(stderr, "Unsupport baud rate %d\n", baud);
				 break;
	};

	return ret;
}

int parse_args(int argc, char **argv, struct Args *args)
{
	memset(args, 0, sizeof(*args));
	args->realbaud = BAUD_RATE;
	args->baud = map_int_to_speed(BAUD_RATE);

	while(1)
	{
		int ch;
		int error = 0;

		ch = getopt(argc, argv, "b:");
		if(ch < 0)
		{
			break;
		}

		switch(ch)
		{
			case 'b': args->baud = map_int_to_speed(atoi(optarg));
					  args->realbaud = atoi(optarg);
					  if(args->baud == 0)
					  {
						  error = 1;
					  }
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
		args->ip = DEFAULT_SERIAL;
	}
	else
	{
		args->ip = argv[0];
	}

	return 1;
}

void print_help(void)
{
	fprintf(stderr, "SERSH Help\n");
	fprintf(stderr, "Usage: sersh [options] [device]\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-b baud     : Specify the baud rate (default %d)\n", BAUD_RATE);
}

int read_socket(int sock)
{
	char buf[1024];
	int len;

	len = read(sock, buf, sizeof(buf)-1);
	if(len < 0)
	{
		perror("read");
		return -1;
	}

	/* EOF */
	if(len == 0)
	{
		return -1;
	}

	buf[len] = 0;

	printf("%s", buf);
	fflush(stdout);

	return len;
}

int on_idle(void)
{
	struct termios options;

	g_context.sock = open(g_context.args.ip, O_RDWR | O_NOCTTY | O_NDELAY);
	if(g_context.sock == -1)
	{
		perror("Unable to open serial port - ");
		return 0;
	}
	else
	{
		fcntl(g_context.sock, F_SETFL, 0);
	}

	tcgetattr(g_context.sock, &options);
	cfsetispeed(&options, g_context.args.baud);
	cfsetospeed(&options, g_context.args.baud);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_cflag &= ~CRTSCTS;
	options.c_cflag |= (CLOCAL | CREAD);

	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_iflag &= ~(IXON | IXOFF | IXANY);
	options.c_iflag |= IGNCR;

	options.c_oflag &= ~OPOST;

	tcsetattr(g_context.sock, TCSANOW, &options);
	FD_SET(g_context.sock, &g_context.readsave);
	write(g_context.sock, "\n", 1);

	return 1;
}

void shell(void)
{
	fd_set readset;

	printf("Opening %s baud %d\n", g_context.args.ip, g_context.args.realbaud);

	FD_ZERO(&g_context.readsave);
	if(!on_idle())
	{
		return;
	}

	while(!g_context.exit)
	{
		int ret;

		readset = g_context.readsave;
		ret = select(FD_SETSIZE, &readset, NULL, NULL, NULL);
		if(ret < 0)
		{
			if(errno == EINTR)
			{
				continue;
			}

			perror("select");
			break;
		}
		else if(ret == 0)
		{
			continue;
		}
		else
		{
			if(FD_ISSET(g_context.sock, &readset))
			{
				/* Do read */
				if(read_socket(g_context.sock) < 0)
				{
					close(g_context.sock);
					g_context.sock = -1;
					g_context.exit = 1;
				}
			}
		}
	}
}

void sig_call(int sig)
{
	if((sig == SIGINT) || (sig == SIGTERM))
	{
		printf("Exiting\n");
		if(g_context.sock >= 0)
		{
			close(g_context.sock);
			g_context.sock = -1;
		}
		exit(0);
	}
}

int main(int argc, char **argv)
{
	memset(&g_context, 0, sizeof(g_context));
	g_context.sock = -1;
	if(parse_args(argc, argv, &g_context.args))
	{
		shell();
		if(g_context.sock >= 0)
		{
			close(g_context.sock);
		}
	}
	else
	{
		print_help();
	}

	return 0;
}
