/* Tests the RemoteJoy input protocol without touching PSP hardware. */
#include "../joy.h"
#include "../../tools/remotejoy/remotejoy.h"
#include "../../tools/remotejoy/joy_stream.h"
#include "../../tools/remotejoy/ctrl_wrapper.h"
#include "pspctrl.h"

#include <assert.h>
#include <chrono>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const uint32_t supported_buttons = PSP_CTRL_SELECT | PSP_CTRL_START |
	PSP_CTRL_UP | PSP_CTRL_RIGHT | PSP_CTRL_DOWN | PSP_CTRL_LEFT |
	PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_TRIANGLE |
	PSP_CTRL_CIRCLE | PSP_CTRL_CROSS | PSP_CTRL_SQUARE;

/* Explicit little-endian fixture bytes, independent of the production encoder. */
#define TEST_LE32_BYTES(value) \
	(unsigned char)((uint32_t)(value)), \
	(unsigned char)((uint32_t)(value) >> 8), \
	(unsigned char)((uint32_t)(value) >> 16), \
	(unsigned char)((uint32_t)(value) >> 24)

static void expect_event(int sock, uint32_t type, uint32_t value)
{
	unsigned char packet[sizeof(struct JoyEvent)];
	const uint32_t fields[] = { JOY_MAGIC, type, value };
	size_t off = 0;
	size_t i;

	while(off < sizeof(packet))
	{
		ssize_t ret = read(sock, packet + off, sizeof(packet) - off);
		assert(ret > 0);
		off += (size_t)ret;
	}

	for(i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
	{
		unsigned int j;
		for(j = 0; j < sizeof(fields[i]); ++j)
		{
			assert(packet[sizeof(fields[i]) * i + j] == (unsigned char)(fields[i] >> (j * CHAR_BIT)));
		}
	}
}

static void test_press(void)
{
	int pair[2];
	struct JoyCommand cmd;
	char action[] = "press", names[] = "cross+right", ms[] = "1";
	char *args[] = { action, names, ms };

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	assert(joy_parse(3, args, &cmd));
	assert(cmd.type == JOY_COMMAND_PRESS);
	assert(cmd.buttons == (PSP_CTRL_CROSS | PSP_CTRL_RIGHT));
	assert(joy_send(pair[0], &cmd) == 0);
	expect_event(pair[1], TYPE_BUTTON_DOWN, cmd.buttons);
	expect_event(pair[1], TYPE_BUTTON_UP, cmd.buttons);
	close(pair[0]);
	close(pair[1]);
}

static void test_duration(void)
{
	using namespace std::chrono;
	int pair[2];
	struct timespec start, end;
	struct JoyCommand cmd = { JOY_COMMAND_PRESS, PSP_CTRL_CROSS, 0, 0, 200 };
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
	assert(joy_send(pair[0], &cmd) == 0);
	assert(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
	const nanoseconds elapsed = seconds(end.tv_sec - start.tv_sec) + nanoseconds(end.tv_nsec - start.tv_nsec);
	assert(elapsed >= milliseconds(cmd.duration_ms));
	expect_event(pair[1], TYPE_BUTTON_DOWN, cmd.buttons);
	expect_event(pair[1], TYPE_BUTTON_UP, cmd.buttons);
	close(pair[0]);
	close(pair[1]);
}

static void test_controller_wrappers(void)
{
	/* Actual 6.61 sceCtrl export wrapper instructions. */
	uint32_t code[] = { 0x27BDFFF0, 0x00003021, 0xAFBF0000, 0x0E04A5DC,
		0x00003821, 0x8FBF0000, 0x03E00008, 0x27BD0010 };
	const uint32_t mode_instructions[] = { 0x00003821, 0x24070001, 0x24070002, 0x24070003 };
	const unsigned int modes[] = { CTRL_WRAPPER_PEEK, CTRL_WRAPPER_NEGATIVE,
		CTRL_WRAPPER_READ, CTRL_WRAPPER_READ | CTRL_WRAPPER_NEGATIVE };
	for(unsigned int i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
	{
		code[CTRL_WRAPPER_FOUR_ARGS_CALL + 1] = mode_instructions[i];
		assert(ctrl_wrapper_layout(code, modes[i]) == CTRL_WRAPPER_FOUR_ARGS);
		assert(ctrl_wrapper_layout(code, modes[i] ^ CTRL_WRAPPER_NEGATIVE) == CTRL_WRAPPER_UNKNOWN);
	}
	code[5] = mips_nop();
	assert(ctrl_wrapper_layout(code, CTRL_WRAPPER_READ | CTRL_WRAPPER_NEGATIVE) == CTRL_WRAPPER_UNKNOWN);
	code[CTRL_WRAPPER_LEGACY_CALL] = 0x0E04A5DC;
	assert(ctrl_wrapper_layout(code, CTRL_WRAPPER_READ | CTRL_WRAPPER_NEGATIVE) == CTRL_WRAPPER_LEGACY);
}

static void test_mips_instructions(void)
{
	/* Independent encodings from the firmware and existing patch sites. */
	assert(mips_addiu(MIPS_REG_SP, MIPS_REG_SP, (uint16_t)-16) == 0x27BDFFF0U);
	assert(mips_addiu(MIPS_REG_V0, MIPS_REG_ZERO, UINT16_MAX) == 0x2402FFFFU);
	assert(mips_move(MIPS_REG_V0, MIPS_REG_ZERO) == 0x00001021U);
	assert(mips_sw(MIPS_REG_RA, 0, MIPS_REG_SP) == 0xAFBF0000U);
	assert(mips_lw(MIPS_REG_RA, 0, MIPS_REG_SP) == 0x8FBF0000U);
	assert(mips_jr(MIPS_REG_RA) == 0x03E00008U);
	assert(mips_nop() == 0x00000000U);
	assert(mips_opcode(0x0E04A5DCU) == MIPS_OP_JAL);
	assert(mips_opcode(0x0A04A5DCU) == MIPS_OP_J);
	assert(mips_function(0x03E00008U) == MIPS_FUNC_JR);
	assert(mips_rs(0x03E00008U) == MIPS_REG_RA);
	assert(mips_jump_target(0x881293ECU, 0x0E04A5DCU) == 0x88129770U);
	assert(mips_encode_jump(MIPS_OP_JAL, 0x88129770U) == 0x0E04A5DCU);
	assert(mips_encode_jump(MIPS_OP_J, 0x88129770U) == 0x0A04A5DCU);
	/* J-format addresses use the high nibble of PC + 4, not PC. */
	assert(mips_jump_target(0x0FFFFFFCU, 0x08000000U) == 0x10000000U);
}

static void test_analog(void)
{
	int pair[2];
	struct JoyCommand cmd;
	char action[] = "analog", x[] = "255", y[] = "0", ms[] = "1";
	char *args[] = { action, x, y, ms };

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	assert(joy_parse(4, args, &cmd));
	assert(cmd.x == JOY_ANALOG_MAX && cmd.y == JOY_ANALOG_MIN);
	assert(joy_send(pair[0], &cmd) == 0);
	expect_event(pair[1], TYPE_ANALOG_X, JOY_ANALOG_MAX);
	expect_event(pair[1], TYPE_ANALOG_Y, JOY_ANALOG_MIN);
	expect_event(pair[1], TYPE_ANALOG_X, JOY_ANALOG_CENTER);
	expect_event(pair[1], TYPE_ANALOG_Y, JOY_ANALOG_CENTER);
	close(pair[0]);
	close(pair[1]);
}

static void test_release(void)
{
	int pair[2];
	struct JoyCommand cmd;
	char action[] = "release";
	char *args[] = { action };

	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	assert(joy_parse(1, args, &cmd));
	assert(joy_send(pair[0], &cmd) == 0);
	expect_event(pair[1], TYPE_BUTTON_UP, supported_buttons);
	expect_event(pair[1], TYPE_ANALOG_X, JOY_ANALOG_CENTER);
	expect_event(pair[1], TYPE_ANALOG_Y, JOY_ANALOG_CENTER);
	close(pair[0]);
	close(pair[1]);
}

static void test_invalid(void)
{
	struct JoyCommand cmd;
	char action[] = "press", unknown[] = "home", zero[] = "0",
			long_delay[] = "10001", bad_combo[] = "cross+",
			valid[] = "cross";
	char *unknown_args[] = { action, unknown };
	char *zero_args[] = { action, valid, zero };
	char *long_args[] = { action, valid, long_delay };
	char *combo_args[] = { action, bad_combo };
	char analog[] = "analog", bad_axis[] = "256", center[] = "128";
	char *axis_args[] = { analog, bad_axis, center };

	assert(!joy_parse(2, unknown_args, &cmd));
	assert(!joy_parse(3, zero_args, &cmd));
	assert(!joy_parse(3, long_args, &cmd));
	assert(!joy_parse(2, combo_args, &cmd));
	assert(!joy_parse(3, axis_args, &cmd));
}

static void test_fragmented_stream(void)
{
	/* Independent wire layout, including junk and a partial magic. */
	const uint32_t buttons = PSP_CTRL_CROSS | PSP_CTRL_UP;
	const unsigned char bytes[] = {
		0xff, 0xef, 0xef, 0xcc, 0x9a, 0x90, 1, 0, 0, 0, TEST_LE32_BYTES(buttons),
		0xef, 0xcc, 0x9a, 0x90, 2, 0, 0, 0, TEST_LE32_BYTES(buttons)
	};
	for(size_t split = 0; split <= sizeof(bytes); ++split)
	{
		struct JoyStream stream = {};
		struct JoyEvent event;
		unsigned int count = 0;
		for(unsigned int chunk = 0; chunk < 2; ++chunk)
		{
			for(size_t i = chunk ? split : 0; i < (chunk ? sizeof(bytes) : split); ++i)
			{
				if(joy_stream_push(&stream, bytes[i], &event))
				{
					assert(event.magic == JOY_MAGIC);
					assert(event.type == (count ? TYPE_BUTTON_UP : TYPE_BUTTON_DOWN));
					assert(event.value == buttons);
					++count;
				}
			}
		}
		assert(count == 2 && stream.used == 0);
	}
}

static void test_truncated_stream(void)
{
	const unsigned char analog[] = { 0xef, 0xcc, 0x9a, 0x90, 4, 0, 0, 0, TEST_LE32_BYTES(JOY_ANALOG_MAX) };
	const unsigned char release[] = { 0xef, 0xcc, 0x9a, 0x90, 2, 0, 0, 0,
		TEST_LE32_BYTES(supported_buttons) };
	for(size_t cut = 0; cut < sizeof(analog); ++cut)
	{
		struct JoyStream stream = {};
		struct JoyEvent event;
		for(size_t i = 0; i < cut; ++i)
			assert(!joy_stream_push(&stream, analog[i], &event));
		for(size_t i = 0; i < sizeof(release); ++i)
			assert(joy_stream_push(&stream, release[i], &event) == (i == sizeof(release) - 1));
		assert(event.type == TYPE_BUTTON_UP && event.value == supported_buttons);
	}
}

static void test_interrupt(void)
{
	int pair[2], status;
	struct JoyCommand cmd = { JOY_COMMAND_PRESS, PSP_CTRL_CROSS, 0, 0, JOY_DURATION_MAX_MS };
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	pid_t child = fork();
	assert(child >= 0);
	if(child == 0)
	{
		close(pair[1]);
		_exit(joy_send(pair[0], &cmd) == -1 ? 0 : 1);
	}
	close(pair[0]);
	expect_event(pair[1], TYPE_BUTTON_DOWN, cmd.buttons);
	assert(kill(child, SIGTERM) == 0);
	expect_event(pair[1], TYPE_BUTTON_UP, cmd.buttons);
	close(pair[1]);
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void test_disconnected(void)
{
	int pair[2], status;
	struct JoyCommand cmd = { JOY_COMMAND_PRESS, PSP_CTRL_CROSS, 0, 0, 250 };
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	pid_t child = fork();
	assert(child >= 0);
	if(child == 0)
	{
		close(pair[0]);
		expect_event(pair[1], TYPE_BUTTON_DOWN, cmd.buttons);
		close(pair[1]);
		_exit(0);
	}
	close(pair[1]);
	assert(joy_send(pair[0], &cmd) == -1);
	close(pair[0]);
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(void)
{
	alarm(5);
	assert(sizeof(struct JoyEvent) == 12);
	assert(sizeof(struct JoyScrHeader) == 16);
	test_press();
	test_duration();
	test_controller_wrappers();
	test_mips_instructions();
	test_analog();
	test_release();
	test_invalid();
	test_fragmented_stream();
	test_truncated_stream();
	test_interrupt();
	test_disconnected();
	alarm(0);
	puts("RemoteJoy input protocol test passed");
	return 0;
}
