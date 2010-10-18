/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * webget.c - Simple wget like clone using libcurl
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/tools/webget/webget.c $
 * $Id: webget.c 1910 2006-05-14 18:39:19Z tyranid $
 */
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

/* Define the module info section */
PSP_MODULE_INFO("webget", 0, 1, 1);
PSP_MAIN_THREAD_NAME("WebGet");

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define WRITEBUF_SIZE (512*1024)

unsigned char g_writebuf[WRITEBUF_SIZE];
int g_writebufpos = 0;

/* Buffer up the data */
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	int *fd = (int *) data;
	int totalsize = size * nmemb;
	int ret = totalsize;

	if((totalsize + g_writebufpos) < WRITEBUF_SIZE)
	{
		memcpy(&g_writebuf[g_writebufpos], ptr, totalsize);
		g_writebufpos += totalsize;
	}
	else
	{
		if(g_writebufpos > 0)
		{
			sceIoWrite(*fd, g_writebuf, g_writebufpos);
			g_writebufpos = 0;
		}

		while(totalsize > WRITEBUF_SIZE)
		{
			sceIoWrite(*fd, ptr, WRITEBUF_SIZE);
			totalsize -= WRITEBUF_SIZE;
			ptr += WRITEBUF_SIZE;
		}

		if(totalsize > 0)
		{
			memcpy(g_writebuf, ptr, totalsize);
			g_writebufpos = totalsize;
		}
	}

	return ret;
}

int main(int argc, char *argv[])
{
	int fd = -1;
	CURL *curl = NULL;
	double speed = 0.0;
	double size = 0.0;

	printf("WebGet v0.1 (uses the CURL library)\n");
	if(argc < 3)
	{
		printf("Usage: webget.prx URL output\n");
		return 1;
	}

	do
	{
		fd = sceIoOpen(argv[2], PSP_O_WRONLY | PSP_O_TRUNC | PSP_O_CREAT, 0777);
		if(fd < 0)
		{
			printf("Couldn't open file %s, 0x%08X\n", argv[2], fd);
			break;
		}

		curl = curl_easy_init();
		if(curl == NULL)
		{
			printf("Couldn't initialise curl library\n");
			break;
		}

		if(curl_easy_setopt(curl, CURLOPT_URL, argv[1]) != CURLE_OK)
		{
			printf("Could not set curl URL\n");
			break;
		}

		if(curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &fd) != CURLE_OK)
		{
			printf("Could not set write file pointer\n");
			break;
		}

		if(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback) != CURLE_OK)
		{
			printf("Could not set write callback\n");
			break;
		}

		if(curl_easy_perform(curl) != CURLE_OK)
		{
			printf("Could not read data from URL\n");
			break;
		}

		if(g_writebufpos > 0)
		{
			sceIoWrite(fd, g_writebuf, g_writebufpos);
		}

		if(curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &size) != CURLE_OK)
		{
			printf("Couldn't get the data size\n");
		}

		if(curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &speed) != CURLE_OK)
		{
			printf("Couldn't get the download speed\n");
		}

		printf("Download %d bytes, %fKb/s\n", (int) size, speed / 1024.0);
	}
	while(0);

	if(curl)
	{
		curl_easy_cleanup(curl);
	}

	if(fd >= 0)
	{
		sceIoClose(fd);
	}

	return 0;
}
