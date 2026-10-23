#include "../parse_args.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect_rejected(const char *command)
{
	char buffer[4096] = {};
	char redirection[4096] = {};
	char *argv[16] = {};
	int argc = -1;
	int type = -1;
	int length = parse_cli(command, buffer, &argc, argv, 16, 0, NULL, &type, redirection);
	assert(length == 0);
}

int main(void)
{
	char buffer[4096] = {};
	char redirection[4096] = {};
	char *argv[16] = {};
	int argc = 0;
	int type = -1;

	/* Regressions from https://github.com/pspdev/psplinkusb/issues/42. */
	expect_rejected("\"./oceanpop.prx");
	expect_rejected("\" ./oceanpop.prx");
	expect_rejected("./oceanpop.prx\"");
	expect_rejected("./oceanpop.prx \"");
	expect_rejected("run 'unfinished");
	expect_rejected("run >\"unfinished");
	expect_rejected(">filename");

	int length = parse_cli("\"game with spaces.prx\" arg", buffer, &argc, argv,
		16, 0, NULL, &type, redirection);
	assert(length > 0);
	assert(argc == 2);
	assert(strcmp(argv[0], "game with spaces.prx") == 0);
	assert(strcmp(argv[1], "arg") == 0);
	assert(type == REDIR_TYPE_NONE);

	puts("pspsh quote parser test passed");
	return 0;
}
