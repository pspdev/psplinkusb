/* End-to-end pspsh -e RemoteJoy smoke test with a mock USBHostFS bridge. */
#include "../../tools/remotejoy/remotejoy.h"
#include "../../usbhostfs/usbasync.h"
#include "../../psplink/shellcmd.h"
#include "pspctrl.h"

#include <assert.h>
#include <limits.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static int listen_on(unsigned short port, unsigned short *bound_port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	socklen_t length;

	assert(fd >= 0);
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(port);
	if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		close(fd);
		return -1;
	}
	assert(listen(fd, 1) == 0);
	length = sizeof(addr);
	assert(getsockname(fd, (struct sockaddr *)&addr, &length) == 0);
	*bound_port = ntohs(addr.sin_port);
	return fd;
}

static void expect_event(int fd, uint32_t type, uint32_t value)
{
	unsigned char bytes[sizeof(struct JoyEvent)];
	uint32_t expected[] = { JOY_MAGIC, type, value };
	size_t offset = 0;
	size_t i;

	while(offset < sizeof(bytes))
	{
		ssize_t ret = read(fd, bytes + offset, sizeof(bytes) - offset);
		assert(ret > 0);
		offset += (size_t)ret;
	}
	for(i = 0; i < sizeof(bytes); ++i)
	{
		assert(bytes[i] == (unsigned char)(expected[i / sizeof(expected[0])] >>
			(CHAR_BIT * (i % sizeof(expected[0])))));
	}
}

static void listen_bridge(int *shell, int *remote, unsigned short *port)
{
	unsigned short joy_port = 0;
	*shell = *remote = -1;
	for(int attempt = 0; attempt < 50; ++attempt)
	{
		*shell = listen_on(0, port);
		assert(*shell >= 0);
		if(*port <= UINT16_MAX - ASYNC_USER)
		{
			*remote = listen_on((unsigned short)(*port + ASYNC_USER), &joy_port);
		}
		if(*remote >= 0)
		{
			break;
		}
		close(*shell);
		*shell = -1;
	}
	assert(*shell >= 0 && *remote >= 0);
	assert(joy_port == *port + ASYNC_USER);
}

static pid_t start_cli(const char *binary, int shell, int remote, unsigned short port,
		const char *command, const char *script)
{
	char port_text[16];
	snprintf(port_text, sizeof(port_text), "%u", port);
	pid_t child = fork();
	assert(child >= 0);
	if(child == 0)
	{
		close(shell);
		close(remote);
		if(script)
		{
			execl(binary, binary, "-n", "-i", "127.0.0.1",
				"-p", port_text, script, (char *)NULL);
		}
		else
		{
			execl(binary, binary, "-n", "-i", "127.0.0.1",
				"-p", port_text, "-e", command, (char *)NULL);
		}
		_exit(127);
	}
	return child;
}

static int accept_shell(int shell)
{
	int shell_client = accept(shell, NULL, NULL);
	assert(shell_client >= 0);
	char packet[256];
	const unsigned char success[] = { SHELL_CMD_BEGIN, SHELL_CMD_SUCCESS, '0', SHELL_CMD_END };
	assert(read(shell_client, packet, sizeof(packet)) > 0);
	assert(write(shell_client, success, sizeof(success)) == (ssize_t)sizeof(success));
	return shell_client;
}

static void expect_exit(pid_t child, int expected)
{
	int status;
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == expected);
}

static void test_one_shot(const char *binary)
{
	int shell, remote;
	unsigned short port;
	listen_bridge(&shell, &remote, &port);
	pid_t child = start_cli(binary, shell, remote, port, "joy press cross+up 1", NULL);
	int shell_client = accept_shell(shell);
	int joy_client = accept(remote, NULL, NULL);
	assert(joy_client >= 0);
	expect_event(joy_client, TYPE_BUTTON_DOWN, PSP_CTRL_CROSS | PSP_CTRL_UP);
	expect_event(joy_client, TYPE_BUTTON_UP, PSP_CTRL_CROSS | PSP_CTRL_UP);
	close(joy_client);
	expect_exit(child, 0);
	close(shell_client);
	close(remote);
	close(shell);
}

static void test_invalid(const char *binary, const char *command, const char *script)
{
	int shell, remote;
	unsigned short port;
	listen_bridge(&shell, &remote, &port);
	pid_t child = start_cli(binary, shell, remote, port, command, script);
	int shell_client = accept_shell(shell);
	expect_exit(child, 1);
	/* An invalid command must not connect to RemoteJoy or execute the next line. */
	struct pollfd pending = { remote, POLLIN, 0 };
	assert(poll(&pending, 1, 0) == 0);
	close(shell_client);
	close(shell);
	close(remote);
}

static void test_script(const char *binary)
{
	char path[] = "/tmp/pspsh-joy-script.XXXXXX";
	const char script[] = "joy press cross 1\njoy analog 255 0 1\n";
	int fd = mkstemp(path);
	assert(fd >= 0);
	assert(write(fd, script, sizeof(script) - 1) == (ssize_t)sizeof(script) - 1);
	close(fd);
	int shell, remote;
	unsigned short port;
	listen_bridge(&shell, &remote, &port);
	pid_t child = start_cli(binary, shell, remote, port, NULL, path);
	int shell_client = accept_shell(shell);
	int joy_client = accept(remote, NULL, NULL);
	assert(joy_client >= 0);
	expect_event(joy_client, TYPE_BUTTON_DOWN, PSP_CTRL_CROSS);
	expect_event(joy_client, TYPE_BUTTON_UP, PSP_CTRL_CROSS);
	/* All script commands must use one connection to preserve ordering. */
	expect_event(joy_client, TYPE_ANALOG_X, JOY_ANALOG_MAX);
	expect_event(joy_client, TYPE_ANALOG_Y, JOY_ANALOG_MIN);
	expect_event(joy_client, TYPE_ANALOG_X, JOY_ANALOG_CENTER);
	expect_event(joy_client, TYPE_ANALOG_Y, JOY_ANALOG_CENTER);
	close(joy_client);
	expect_exit(child, 0);
	struct pollfd pending = { remote, POLLIN, 0 };
	assert(poll(&pending, 1, 0) == 0);
	close(shell_client);
	close(shell);
	close(remote);
	assert(unlink(path) == 0);

	char bad_path[] = "/tmp/pspsh-joy-script.XXXXXX";
	const char invalid[] = "joy press home\njoy press cross 1\n";
	fd = mkstemp(bad_path);
	assert(fd >= 0);
	assert(write(fd, invalid, sizeof(invalid) - 1) == (ssize_t)sizeof(invalid) - 1);
	close(fd);
	test_invalid(binary, NULL, bad_path);
	assert(unlink(bad_path) == 0);
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	alarm(15);
	test_one_shot(argv[1]);
	test_invalid(argv[1], "joy press home", NULL);
	test_invalid(argv[1], "joy", NULL);
	test_script(argv[1]);
	alarm(0);
	puts("pspsh RemoteJoy CLI and script integration tests passed");
	return 0;
}
