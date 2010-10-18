/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * ttyline.c - PSPLINK text terminal
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.pspdev.org/psp/branches/psplinkusb/pcterm/pcterm.c $
 * $Id: pcterm.c 1963 2006-07-07 17:25:29Z tyranid $
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
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <signal.h>

#ifndef SOL_TCP
#define SOL_TCP 6
#endif

#define DEFAULT_PORT 10000
#define DEFAULT_IP     "localhost"

struct Args
{
	const char *ip;
	const char *log;
	unsigned short port;
};

struct GlobalContext
{
	struct Args args;
	int exit;
	int conn_sanity;
	fd_set readsave;
	int outsock;
	int errsock;
	int log;
};

struct GlobalContext g_context;

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

void cli_handler(char *buf)
{
	if((buf) && (*buf))
	{
		write(g_context.outsock, buf, strlen(buf));
		write(g_context.outsock, "\n", 1);
	}
}

int init_readline(void)
{
	rl_callback_handler_install("", cli_handler);

	return 1;
}

int parse_args(int argc, char **argv, struct Args *args)
{
	memset(args, 0, sizeof(*args));
	args->port = DEFAULT_PORT;

	while(1)
	{
		int ch;
		int error = 0;

		ch = getopt(argc, argv, "p:l:");
		if(ch < 0)
		{
			break;
		}

		switch(ch)
		{
			case 'p': args->port = atoi(optarg);
					  break;
			case 'l': args->log = optarg;
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
		args->ip = DEFAULT_IP;
	}
	else
	{
		args->ip = argv[0];
	}

	return 1;
}

void print_help(void)
{
	fprintf(stderr, "TTYLink Help\n");
	fprintf(stderr, "Usage: ttyline [options] [ipaddr]\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "-p port     : Specify the port number\n");
	fprintf(stderr, "-n          : Do not connect up the tty (stdin/stdout/stderr)\n");
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

int read_outsocket(int sock)
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

	fprintf(stdout, "%s", buf);
	fflush(stdout);

	return len;
}

int read_errsocket(int sock)
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

	fprintf(stderr, "%s", buf);
	fflush(stderr);

	return len;
}

int connect_to(const char *ipaddr, unsigned short port)
{
	int sock = -1;
	int success = 0;
	struct sockaddr_in name;

	do
	{
		if(!init_sockaddr(&name, ipaddr, port))
		{
			break;
		}
		sock = socket(PF_INET, SOCK_STREAM, 0);
		if(sock < 0)
		{
			perror("socket");
			break;
		}

		if(connect(sock, (struct sockaddr *) &name, sizeof(name)) < 0)
		{
			perror("connect");
			break;
		}
		else
		{
			FD_SET(sock, &g_context.readsave);
		}

		success = 1;
	}
	while(0);

	if(!success)
	{
		if(sock >= 0)
		{
			close(sock);
			sock = -1;
		}
	}

	return sock;
}

void shell(void)
{
	fd_set readset;
	FD_ZERO(&g_context.readsave);

	printf("Opening connection to %s port %d\n", g_context.args.ip, g_context.args.port);

	if((g_context.outsock = connect_to(g_context.args.ip, g_context.args.port+2)) < 0)
	{
		fprintf(stderr, "Could not connect to stdout channel\n");
		return;
	}
	if((g_context.errsock = connect_to(g_context.args.ip, g_context.args.port+3)) < 0)
	{
		fprintf(stderr, "Could not connect to stderr channel\n");
		return;
	}

	init_readline();

	FD_SET(STDIN_FILENO, &g_context.readsave);

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
			if(FD_ISSET(STDIN_FILENO, &readset))
			{
				rl_callback_read_char();
			}

			if(FD_ISSET(g_context.outsock, &readset))
			{
				if(read_outsocket(g_context.outsock) < 0)
				{
					FD_CLR(g_context.outsock, &g_context.readsave);
					close(g_context.outsock);
					g_context.outsock = -1;
				}
			}
			if(FD_ISSET(g_context.errsock, &readset))
			{
				if(read_errsocket(g_context.errsock) < 0)
				{
					FD_CLR(g_context.errsock, &g_context.readsave);
					close(g_context.errsock);
					g_context.errsock = -1;
				}
			}
		}
	}

	rl_callback_handler_remove();
}

void sig_call(int sig)
{
	if((sig == SIGINT) || (sig == SIGTERM))
	{
		printf("Exiting\n");
		if(g_context.outsock >= 0)
		{
			close(g_context.outsock);
			g_context.outsock = -1;
		}
		if(g_context.errsock >= 0)
		{
			close(g_context.errsock);
			g_context.errsock = -1;
		}
		rl_callback_handler_remove();
		exit(0);
	}
}

int main(int argc, char **argv)
{
	memset(&g_context, 0, sizeof(g_context));
	g_context.outsock = -1;
	g_context.errsock = -1;
	g_context.log  = -1;
	if(parse_args(argc, argv, &g_context.args))
	{
		if(g_context.args.log)
		{
			g_context.log = open(g_context.args.log, O_WRONLY | O_CREAT | O_TRUNC, 0660);
			if(g_context.log < 0)
			{
				fprintf(stderr, "Warning: Could not open log file %s (%s)\n", g_context.args.log, 
						strerror(errno));
			}
		}
		shell();
		if(g_context.outsock >= 0)
		{
			close(g_context.outsock);
		}
		if(g_context.errsock >= 0)
		{
			close(g_context.errsock);
		}
		if(g_context.log >= 0)
		{
			close(g_context.log);
		}
	}
	else
	{
		print_help();
	}

	return 0;
}
