/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * main.c - Basic example of the APF
 *
 * Copyright (c) 2006 James F
 *
 */
#include <pspkernel.h>
#include <usbasync.h>
#include <stdio.h>

/* Define the module info section */
PSP_MODULE_INFO("template", 0, 1, 1);

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

struct AsyncEndpoint g_endp;

int main(int argc, char *argv[])
{
	int chan;
	int running = 1;

	pspDebugScreenInit();
	chan = usbAsyncRegister(ASYNC_ALLOC_CHAN, &g_endp);
	if(chan < 0)
	{
		printf("Could not allocate async channel\n");
		return 0;
	}

	usbWaitForConnect();

	printf("Allocated channel %d, connect to localhost port %d and start typing\n", chan, 10000 + chan);
	while(running)
	{
		unsigned char buf[512];
		int len;

		len = usbAsyncRead(chan, buf, sizeof(buf));
		if(len < 0)
		{
			/* Error, most likely shutdown */
			break;
		}

		if(len > 0)
		{
			pspDebugScreenPrintf("Read: %.*s\n", len, buf);
			usbAsyncWrite(chan, buf, len);
		}
	}

	usbAsyncUnregister(chan);

	return 0;
}
