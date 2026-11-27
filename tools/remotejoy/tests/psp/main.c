/* User-mode end-to-end controller probe for RemoteJoy on real hardware. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>

PSP_MODULE_INFO("RemoteJoyTest", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

enum
{
	PROBE_DURATION_SECONDS = 120,
	MICROSECONDS_PER_SECOND = 1000000,
	ANALOG_REPORT_THRESHOLD = 3,
	DEFAULT_SAMPLING_CYCLE = 0
};

enum ProbeSample
{
	SAMPLE_READ_POSITIVE,
	SAMPLE_PEEK_POSITIVE,
	SAMPLE_READ_NEGATIVE,
	SAMPLE_PEEK_NEGATIVE,
	SAMPLE_COUNT
};

static const unsigned int buttons_mask = PSP_CTRL_SELECT | PSP_CTRL_START |
	PSP_CTRL_UP | PSP_CTRL_RIGHT | PSP_CTRL_DOWN | PSP_CTRL_LEFT |
	PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_TRIANGLE |
	PSP_CTRL_CIRCLE | PSP_CTRL_CROSS | PSP_CTRL_SQUARE;

struct Sample
{
	unsigned int buttons;
	int x, y, valid;
};

static void report(const char *api, const SceCtrlData *pad, int negative,
		struct Sample *previous)
{
	unsigned int buttons = (negative ? ~pad->Buttons : pad->Buttons) & buttons_mask;
	if(!previous->valid || buttons != previous->buttons ||
		abs(pad->Lx - previous->x) >= ANALOG_REPORT_THRESHOLD ||
		abs(pad->Ly - previous->y) >= ANALOG_REPORT_THRESHOLD)
	{
		printf("%s t=%u buttons=%08X down=%08X up=%08X x=%u y=%u\n",
			api, pad->TimeStamp, buttons, buttons & ~previous->buttons,
			previous->buttons & ~buttons, (unsigned int)pad->Lx, (unsigned int)pad->Ly);
		previous->buttons = buttons;
		previous->x = pad->Lx;
		previous->y = pad->Ly;
		previous->valid = 1;
	}
}

int main(int argc, char **argv)
{
	struct Sample samples[SAMPLE_COUNT] = { {0} };
	SceCtrlData pad;
	SceCtrlLatch latch;
	unsigned int previous_latch = 0;
	unsigned int start = sceKernelGetSystemTimeLow();
	int previous_cycle = DEFAULT_SAMPLING_CYCLE, previous_mode = PSP_CTRL_MODE_DIGITAL;
	(void)argc;
	(void)argv;
	setvbuf(stdout, NULL, _IOLBF, 0);
	sceCtrlGetSamplingCycle(&previous_cycle);
	sceCtrlGetSamplingMode(&previous_mode);
	sceCtrlSetSamplingCycle(DEFAULT_SAMPLING_CYCLE);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	printf("RemoteJoy test: combined application samples from all four controller APIs.\n");
	printf("Latch reports the unhooked comparison path, not a guaranteed physical-only source.\n");
	printf("Try physical buttons first, then joy press/analog/release with hands off.\n");
	printf("Test ends after %d seconds without exiting PSPLink.\n", PROBE_DURATION_SECONDS);
	while(sceKernelGetSystemTimeLow() - start < PROBE_DURATION_SECONDS * MICROSECONDS_PER_SECOND)
	{
		if(sceCtrlReadBufferPositive(&pad, 1) > 0)
			report("read+", &pad, 0, &samples[SAMPLE_READ_POSITIVE]);
		if(sceCtrlPeekBufferPositive(&pad, 1) > 0)
			report("peek+", &pad, 0, &samples[SAMPLE_PEEK_POSITIVE]);
		if(sceCtrlReadBufferNegative(&pad, 1) > 0)
			report("read-", &pad, 1, &samples[SAMPLE_READ_NEGATIVE]);
		if(sceCtrlPeekBufferNegative(&pad, 1) > 0)
			report("peek-", &pad, 1, &samples[SAMPLE_PEEK_NEGATIVE]);
		if(sceCtrlReadLatch(&latch) > 0)
		{
			unsigned int held = latch.uiPress & buttons_mask;
			unsigned int make = latch.uiMake & buttons_mask;
			unsigned int released = latch.uiBreak & buttons_mask;
			if(held != previous_latch || make || released)
			{
				printf("latch held=%08X down=%08X up=%08X\n", held, make, released);
				previous_latch = held;
			}
		}
	}
	sceCtrlSetSamplingCycle(previous_cycle);
	sceCtrlSetSamplingMode(previous_mode);
	printf("RemoteJoy test finished.\n");
	fflush(stdout);
	sceKernelExitDeleteThread(0);
	return 0;
}
