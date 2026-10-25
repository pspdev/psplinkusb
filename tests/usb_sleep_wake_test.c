/* Test clock-gap detection without sleeping the host or requiring a PSP. */
#define main usbhostfs_pc_main
#include "../usbhostfs_pc/main.c"
#undef main

#include <assert.h>

int main(void)
{
	struct UsbSuspendClock clock = {0};
	double slept;

	assert(usb_suspend_elapsed(&clock, 100.0, 50.0) == 0.0);
	assert(usb_suspend_elapsed(&clock, 101.0, 51.0) == 0.0);
	/* Time elapsed while the Mac is awake is not sleep time. */
	assert(usb_suspend_elapsed(&clock, 901.0, 851.0) == 0.0);
	/* A large difference between continuous and awake elapsed time is sleep. */
	slept = usb_suspend_elapsed(&clock, 955.0, 852.0);
	assert(slept >= 52.9 && slept <= 53.1);
	assert(usb_suspend_elapsed(&clock, 956.0, 853.0) == 0.0);
	/* Ignore tiny clock skew and very short sleeps to avoid reset storms. */
	assert(usb_suspend_elapsed(&clock, 957.8, 854.0) == 0.0);
	assert(usb_suspend_elapsed(&clock, 963.0, 855.0) == 0.0);
	/* A second, longer sleep must still trigger the detector. */
	assert(usb_suspend_elapsed(&clock, 1000.0, 855.5) > 35.0);
	puts("USB sleep/wake detector test passed");
	return 0;
}
