/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * util.c - util functions for psplink
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/util.c $
 * $Id: util.c 2322 2007-09-30 17:49:32Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspusb.h>
#include <pspusbstor.h>
#include <pspumd.h>
#include <psputilsforkernel.h>
#include <usbhostfs.h>
#include "psplink.h"
#include "util.h"

enum UsbStates 
{
	USB_NOSTART = 0,
	USB_ON  = 1,
	USB_OFF = 2,
};

/* Indicates whether the usb drivers have been loaded */
static enum UsbStates g_usbhoststate = USB_NOSTART;

/* Global functions which are setup to point to the correct function
   for the firmware */

extern struct GlobalContext g_context;
int (*g_QueryModuleInfo)(SceUID modid, SceKernelModuleInfo *info) = NULL;
int (*g_GetModuleIdList)(SceUID *readbuf, int readbufsize, int *idcount) = NULL;

extern unsigned int sceKernelRemoveByDebugSection;

int g_isv1 = 0;

int is_alnum(char ch)
{
	int c;

	c = upcase(ch);
	if((c >= 'A') && (c <= 'Z'))
	{
		return 1;
	}

	if((c >= '0') && (c <= '9'))
	{
		return 1;
	}

	return 0;
}

int is_oct(char ch)
{
	if((ch >= '0') && (ch < '8'))
	{
		return 1;
	}

	return 0;
}

int oct_to_int(char ch)
{
	if((ch >= '0') && (ch < '8'))
	{
		return ch - '0';
	}

	return 0;
}

/* Check if character is a hexadecimal character */
int is_hex(char ch)
{
	ch = upcase(ch);

	if((ch >= '0') && (ch <= '9'))
		return 1;

	if((ch >= 'A') && (ch <= 'F'))
		return 1;

	return 0;
}

/* Convert a single hex digit to an int */
int hex_to_int(char ch)
{
	if((ch >= '0') && (ch <= '9'))
	{
		return ch - '0';
	}

	ch = upcase(ch);
	if((ch >= 'A') && (ch <= 'F'))
	{
		return ch - 'A' + 10;
	}

	return 0;
}


int is_aspace(int ch)
{
	if((ch == ' ') || (ch == '\t') || (ch == '\n') || (ch == '\r'))
	{
		return 1;
	}

	return 0;
}

/* Normalise the path, remove . and .. directories, will ignore anything at the end with no dir slash */
static int normalize_path(char *path)
{
	char *last_dir = NULL;
	char *curr_pos;
	int ret = 1;

	/* Can't start with an absolute path */
	if(*path == '/')
	{
		ret = 0;
	}
	else
	{
		curr_pos = strchr(path, '/');
		while(curr_pos != NULL)
		{
			if(last_dir != NULL)
			{
				if(strncmp(last_dir, "/.", curr_pos - last_dir) == 0)
				{
					strcpy(last_dir, curr_pos);
					curr_pos = last_dir;
				}
				else if(strncmp(last_dir, "/..", curr_pos - last_dir) == 0)
				{
					char *last_pos;
					/* Find the last directory slash from last_dir */
					last_pos = last_dir - 1;
					while(last_pos > path)
					{
						if(*last_pos == '/')
						{
							break;
						}
						last_pos--;
					}

					if(last_pos > path)
					{
						last_dir = last_pos;
					}

					strcpy(last_dir, curr_pos);
					curr_pos = last_dir;
				}
				else
				{
					/* Ignore */
				}
			}

			last_dir = curr_pos;
			curr_pos = strchr(curr_pos + 1, '/');
		}
	}

	return ret;
}

int handlepath(const char *currentdir, const char *relative, char *path, int type, int valid)
{
	int len, fd;

	/* Strip whitespace and append a final slash */
	path[0] = 0;
	if(strchr(relative, ':') == NULL)
	{
		if(relative[0] == '/')
		{
			int currdir_pos = 0;
			int path_pos = 0;
			while(currentdir[currdir_pos] != 0)
			{
				path[path_pos] = currentdir[currdir_pos];
				path_pos++;
				if(currentdir[currdir_pos] == ':')
				{
					break;
				}
				currdir_pos++;
			}
			path[path_pos] = 0;
		}
		else
		{
			/* relative directory */
			strcpy(path, currentdir);
		}
	}

	strcat(path, relative);
	len = strlen(path);
	while((len > 0) && (is_aspace(path[len-1])))
	{
		path[len-1] = 0;
		len--;
	}

	if(len == 0)
	{
		return 0;
	}

	/* Very unsafe, but still */
	if(type == TYPE_DIR && path[len-1] != '/') {
		path[len] = '/';
		path[len+1] = 0;
	} else if(type == TYPE_FILE && path[len-1] == '/') {
		path[len-1] = 0;
	}

	if(normalize_path(path) == 0)
		return 0;

	if(valid) {
		if(type == TYPE_DIR) {
			if(!isdir(path))
			{
				return 0;
			}
		} else if(type == TYPE_FILE) {
			if((fd = sceIoOpen(path, PSP_O_RDONLY, 0777)) < 0) {
				/* Invalid File */
				return 0;
			} else {
				sceIoClose(fd);
			}
		} else {
			SHELL_PRINT("unable to validate ether type\n");
			return 0;
		}
	}

	return 1;
}

/* We do not handle paths relative to current dir (i.e. no . specifiers in the path */
/* Returns 1 if found and places the result in output */
/* Not very safe but hey we are not here for that kinda thing :P */
int findinpath(const char *relative, char *output, const char *pathvar)
{
	int found = 0;
	int pathlen;
	int fd;
	const char *currpath;
	const char *nextpath;

	/* Only continue if we have a valid pathvar */
	if(pathvar)
	{
		currpath = pathvar;
		do
		{
			/* Path separator is semi-colon */
			nextpath = strchr(currpath, ';');
			if(nextpath)
			{
				memcpy(output, currpath, nextpath-currpath);
				output[nextpath-currpath] = 0;
				nextpath++;
			}
			else
			{
				strcpy(output, currpath);
			}
			pathlen = strlen(output);
			/* Why bother if the path is empty */
			if(pathlen > 0)
			{
				strcat(output, "/");
				strcat(output, relative);
				SHELL_PRINT("%s\n", output);
				fd = sceIoOpen(output, PSP_O_RDONLY, 0777);
				if(fd >= 0)
				{
					sceIoClose(fd);
					found = 1;
					break;
				}
			}

			currpath = nextpath;
		}
		while(currpath);
	}

	return found;
}

/* Make the character upper case */
char upcase(char ch)
{
	if((ch >= 'a') && (ch <= 'z'))
	{
		ch ^= (1 << 5);
	}

	return ch;
}

int build_bootargs(char *args, const char *bootfile, const char *execfile, int argc, char **argv)
{
	int loc = 0;
	int i;

	strcpy(args, bootfile);
	loc += strlen(bootfile) + 1;
	if(execfile != NULL)
	{
		strcpy(&args[loc], execfile);
		loc += strlen(execfile) + 1;
		for(i = 0; i < argc; i++)
		{
			strcpy(&args[loc], argv[i]);
			loc += strlen(argv[i]) + 1;
		}
	}

	return loc;
}

int build_args(char *args, const char *execfile, int argc, char **argv)
{
	int loc = 0;
	int i;

	strcpy(args, execfile);
	loc += strlen(execfile) + 1;
	for(i = 0; i < argc; i++)
	{
		strcpy(&args[loc], argv[i]);
		loc += strlen(argv[i]) + 1;
	}

	return loc;
}

int load_start_module(const char *name, int argc, char **argv)
{
	SceUID modid;
	int status;
	char args[1024];
	int len;

	modid = sceKernelLoadModule(name, 0, NULL);
	if(modid >= 0)
	{
		len = build_args(args, name, argc, argv);
		modid = sceKernelStartModule(modid, len, (void *) args, &status, NULL);
	}
	else
	{
		Kprintf("lsm: Error loading module %s %08X\n", name, modid);
	}

	return modid;
}

void map_firmwarerev(void)
{

#if _PSP_FW_VERSION > 200
	g_QueryModuleInfo = sceKernelQueryModuleInfo;
	g_GetModuleIdList = sceKernelGetModuleIdList;
	g_isv1 = 0;
#else
	unsigned int rev = sceKernelDevkitVersion();
	/* Special case for version 1 firmware */
    if((rev & 0xFFFF0000) == 0x01000000)
	{
		g_QueryModuleInfo = pspSdkQueryModuleInfoV1;
		g_GetModuleIdList = pspSdkGetModuleIdList;
		g_isv1 = 1;
	}
	else
	{
		g_QueryModuleInfo = sceKernelQueryModuleInfo;
		g_GetModuleIdList = sceKernelGetModuleIdList;
		g_isv1 = 0;
	}
#endif
}

int sceUsb_E20B23A6(int pid, int power);

int init_usbhost(const char *bootpath)
{
	int retVal;
	char prx_path[MAXPATHLEN];

	do
	{
		if((g_usbhoststate == USB_ON))
		{
			retVal = 0;
			break;
		}

		if(g_usbhoststate == USB_NOSTART)
		{
			strcpy(prx_path, bootpath);
			strcat(prx_path, "usbhostfs.prx");
			load_start_module(prx_path, 0, NULL);
		}

		retVal = sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0);
		if (retVal != 0) {
			Kprintf("Error starting USB Bus driver (0x%08X)\n", retVal);
			break;
		}
		retVal = sceUsbStart(HOSTFSDRIVER_NAME, 0, 0);
		if (retVal != 0) {
			Kprintf("Error starting USB Host driver (0x%08X)\n",
			   retVal);
			break;
		}

		retVal = sceUsbActivate(g_context.pid);
		//retVal = sceUsb_E20B23A6(g_context.pid, 0);

		if(retVal == 0)
		{
			g_usbhoststate = USB_ON;
		}
	}
	while(0);

	return retVal;
}

int stop_usbhost(void)
{
	int retVal;

	if((g_usbhoststate != USB_ON))
	{
		return 0;
	}

	retVal = sceUsbDeactivate(g_context.pid);
	if (retVal != 0) {
	    Kprintf("Error calling sceUsbDeactivate (0x%08X)\n", retVal);
    }

    retVal = sceUsbStop(HOSTFSDRIVER_NAME, 0, 0);
    if (retVal != 0) {
		Kprintf("Error stopping USB host driver (0x%08X)\n",
	       retVal);
	}

    retVal = sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
    if (retVal != 0) {
		Kprintf("Error stopping USB BUS driver (0x%08X)\n", retVal);
	}

	g_usbhoststate = USB_OFF;

	return 0;
}

int openfile(const char *filename, PspFile *pFile)
{
	int iRet = 0;

	do
	{
		if(pFile == NULL)
		{
			Kprintf("Error, invalid file\n");
			break;
		}

		memset(pFile, 0, sizeof(PspFile));

		pFile->fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);
		if(pFile->fd < 0)
		{
			Kprintf("Error, cannot open file %s\n", filename);
			break;
		}

		iRet = 1;
	}
	while(0);

	return iRet;
}

int closefile(PspFile *pFile)
{
	int iRet = 0;
	do
	{
		if(pFile == NULL)
		{
			Kprintf("Error, invalid configuration structure\n");
			break;
		}

		if(pFile->fd < 0)
		{
			Kprintf("Error, invalid file descriptor\n");
			break;
		}

		sceIoClose(pFile->fd);
		iRet = 1;
	}
	while(0);

	return iRet;
}

/* Seems the kernel's fdgetc is broken :/ */
int fdgetc(PspFile *pFile)
{
	int ch = -1;

	if(pFile->read_size == 0)
	{
		int size;
		size = sceIoRead(pFile->fd, pFile->read_buf, MAX_BUFFER);
		if(size > 0)
		{
			pFile->read_size = size;
			pFile->read_pos = 0;
		}
		else
		{
			pFile->read_size = 0;
			pFile->read_pos = 0;
		}
	}

	if(pFile->read_pos < pFile->read_size)
	{
		ch = pFile->read_buf[pFile->read_pos++];
	}

	return ch;
}

/* As the kernel's fdgetc is broke so is fdgets */
int fdgets(PspFile *pFile, char *buf, int max)
{
	int pos = 0;

	while(pos < (max-1))
	{
		int ch;

		ch = fdgetc(pFile);

		/* EOF */
		if(ch == -1)
		{
			break;
		}

		buf[pos++] = (char) ch;

		if(ch == '\n')
		{
			break;
		}
	}

	buf[pos] = 0;

	return pos;
}

void strip_whitesp(char *s)
{
	int start;
	int end;

	end = strlen(s);
	while(end > 0)
	{
		if(is_aspace(s[end-1]))
		{
			end--;
			s[end] = 0;
		}
		else
		{
			break;
		}
	}

	start = 0;
	while(s[start])
	{
		if(is_aspace(s[start]))
		{
			start++;
		}
		else
		{
			break;
		}
	}

	if(start > 0)
	{
		int pos = 0;
		while(s[start])
		{
			s[pos++] = s[start++];
		}
		s[pos] = 0;
	}
}

int strtoint(const char *str, unsigned int *i)
{
	char *endp;
	unsigned int val;

	val = strtoul(str, &endp, 0);
	if(*endp != 0)
	{
		return 0;
	}
	*i = val;

	return 1;
}

int memcmp_mask(void *data1, void *data2, void *mask, int len)
{
	unsigned char *m, *d1, *d2;
	int i;

	m = mask;
	d1 = data1;
	d2 = data2;

	if(m == NULL)
	{
		return memcmp(data1, data2, len);
	}

	for(i = 0; i < len; i++)
	{
		if((d1[i] & m[i]) != d2[i])
		{
			return (d1[i] & m[i]) - d2[i];
		}
	}

	return 0;
}

void* memmem_mask(void *data, void *mask, int len, void *search, int slen)
{
	int i;

	if((data == NULL) || (len < 0) || (search == NULL) || (slen < 0))
	{
		return NULL;
	}

	for(i = 0; i < (len - slen + 1); i++)
	{
		if(memcmp_mask(data, search, mask, slen) == 0)
		{
			return data;
		}
		data++;
	}

	return NULL;
}

int decode_hexstr(const char *str, unsigned char *data, int max)
{
	int hexlen;
	int i;

	hexlen = strlen(str);
	if(hexlen & 1)
	{
		SHELL_PRINT("Invalid number of hex characters\n");
		return 0;
	}

	if((hexlen / 2) > max)
	{
		SHELL_PRINT("Hex string too long to fit in buffer\n");
		return 0;
	}

	for(i = 0; i < (hexlen / 2); i++)
	{
		if((!is_hex(str[i*2])) || (!is_hex(str[(i*2)+1])))
		{
			SHELL_PRINT("Invalid hex byte %.2s\n", &str[i*2]);
			return 0;
		}

		data[i] = (hex_to_int(str[i*2]) << 4) | hex_to_int(str[(i*2)+1]);
	}

	return hexlen / 2;
}

static SceUID module_refer(SceModule *pMod, SceKernelModuleInfo *info)
{
	SceUID uid = -1;
	if(pMod)
	{
		uid = pMod->modid;
		if((info) && (psplinkReferModule(pMod->modid, info) == 0))
		{
			uid = -1;
		}
	}
	return uid;
}

SceUID refer_module_by_addr(unsigned int addr, SceKernelModuleInfo *info)
{
	return module_refer(sceKernelFindModuleByAddress(addr), info);
}

SceUID refer_module_by_name(const char *name, SceKernelModuleInfo *info)
{
	return module_refer(sceKernelFindModuleByName(name), info);
}

static unsigned int get_thread_addr(SceUID uid)
{
	SceKernelThreadInfo info;
	unsigned int addr = 0;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if(sceKernelReferThreadStatus(uid, &info) == 0)
	{
		addr = (unsigned int) info.entry;
	}

	return addr;
}

static unsigned int get_callback_addr(SceUID uid)
{
	SceKernelCallbackInfo info;
	unsigned int addr = 0;

	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if(sceKernelReferCallbackStatus(uid, &info) == 0)
	{
		addr = (unsigned int) info.callback;
	}

	return addr;
}

int psplinkReferThreadsByModule(int type, SceUID modid, SceUID *uids, int max)
{
	SceUID thids[100];
	SceKernelModuleInfo modinfo;
	unsigned int addr;
	int count;
	int i;
	int ret;

	ret = 0;
	memset(thids, 0, sizeof(thids));
	if(sceKernelGetThreadmanIdList(type, thids, 100, &count) >= 0)
	{
		for(i = 0; i < count; i++)
		{
			switch(type)
			{
				case SCE_KERNEL_TMID_Thread: addr = get_thread_addr(thids[i]);
											 break;
				case SCE_KERNEL_TMID_Callback: addr = get_callback_addr(thids[i]);
											   break;
				default: addr = 0;
						 break;
			};

			if(refer_module_by_addr(addr, &modinfo) == modid)
			{
				if(ret < max)
				{
					uids[ret++] = thids[i];
				}
			}
		}
	}

	return ret;
}

SceUID psplinkReferModuleByName(const char *name, SceKernelModuleInfo *info)
{
	int k1;
	SceUID uid;

	k1 = psplinkSetK1(0);
	uid = refer_module_by_name(name, info);
	psplinkSetK1(k1);

	return uid;
}

int psplinkReferModule(SceUID uid, SceKernelModuleInfo *info)
{
	int ret;
	int k1;

	k1 = psplinkSetK1(0);

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);

	enable_kprintf(0);
	ret = g_QueryModuleInfo(uid, info);
	if(ret == 0)
	{
		ret = 1;
	}
	else
	{
		ret = 0;
	}
	enable_kprintf(1);

	psplinkSetK1(k1);

	return ret;
}

static int is_nan(float *val)
{
	unsigned int conv;
	int sign;
	int exp;
	int mantissa;

	conv = *((unsigned int *) val);
	sign = (conv >> 31) & 1;

	exp = (conv >> 23) & 0xff;
	mantissa = conv & 0x7fffff;

	if((exp == 255) && (mantissa != 0))
	{
		return 1;
	}

	return 0;
}

static int is_inf(float *val)
{
	unsigned int conv;
	int sign;
	int exp;
	int mantissa;

	conv = *((unsigned int *) val);
	sign = (conv >> 31) & 1;

	exp = (conv >> 23) & 0xff;
	mantissa = conv & 0x7fffff;

	if((exp == 255) && (mantissa == 0))
	{
		if(sign)
		{
			return -1;
		}
		else
		{
			return 1;
		}
	}

	return 0;
}

static char get_num(float *val, int *exp)
{
	int digit;
	float tmp;
	char ret = '0';

	if((*exp)++ < 16)
	{
		digit = (int) *val;
		if((digit >= 0) && (digit < 10))
		{
			ret = digit + '0';
			tmp = (float) digit;
			*val = (*val - digit)*10.0f;
		}
	}

	return ret;
}

void f_cvt(float *val, char *buf, int bufsize, int precision, int mode)
{
	char conv_buf[128];
	char *conv_p = conv_buf;
	float normval;
	int digits = 0;
	int exp = 0;
	int exp_pos = 0;
	int inf;
	int sign = 0;
	float round;
	int rndpos = 0;

	/* check for nan and +/- infinity */
	if(is_nan(val))
	{
		strncpy(buf, "NaN", bufsize);
		buf[bufsize-1] = 0;
		return;
	}

	inf = is_inf(val);
	if(inf != 0)
	{
		if(inf < 0)
		{
			strncpy(buf, "-Infinity", bufsize);
			buf[bufsize-1] = 0;
		}
		else
		{
			strncpy(buf, "Infinity", bufsize);
			buf[bufsize-1] = 0;
		}

		return;
	}

	if(*val < 0.0f)
	{
		sign = 1;
		normval -= *val;
	}
	else
	{
		sign = 0;
		normval = *val;
	}

	if(precision < 0)
	{
		precision = 6;
	}

	/* normalise value */
	if(normval > 0.0f)
	{
		while((normval >= 1e8f) && (digits++ < 100))
		{
			normval *= 1e-8f;
			exp += 8;
		}
		while((normval >= 10.0f) && (digits++ < 100))
		{
			normval *= 0.1f;
			exp++;
		}
		while((normval < 1e-8f) && (digits++ < 100))
		{
			normval *= 1e8f;
			exp -= 8;
		}
		while((normval < 1.0f) && (digits++ < 100))
		{
			normval *= 10.0f;
			exp--;
		}
	}

	for(rndpos = precision, round = 0.4999f; rndpos > 0; rndpos--, round *= 0.1f);

	if(sign)
	{
		*conv_p++ = '-';
	}

	if((mode != MODE_EXP) && (mode != MODE_FLOAT_ONLY))
	{
		if((exp < -4) || (exp > precision))
		{
			mode = MODE_EXP;
		}
	}

	normval += round;

	if(mode == MODE_EXP)
	{
		*conv_p++ = get_num(&normval, &exp_pos);
		*conv_p++ = '.';

		while(precision > 0)
		{
			*conv_p++ = get_num(&normval, &exp_pos);
			precision--;
		}

		while((conv_p[-1] == '0') || (conv_p[-1] == '.'))
		{
			conv_p--;
		}

		*conv_p++ = 'e';
		if(exp < 0)
		{
			exp = -exp;
			*conv_p++ = '-';
		}
		else
		{
			*conv_p++ = '+';
		}

		if(exp / 100)
		{
			*conv_p++ = (exp / 100) + '0';
			exp %= 100;
		}

		if(exp / 10)
		{
			*conv_p++ = (exp / 10) + '0';
			exp %= 10;
		}
		*conv_p++ = exp + '0';
	}
	else
	{
		if(exp >= 0)
		{
			for(; (exp >= 0); exp--)
			{
				*conv_p++ = get_num(&normval, &exp_pos);
			}
		}
		else
		{
			*conv_p++ = '0';
		}

		exp++;
		if(precision > 0)
		{
			*conv_p++ = '.';
		}

		for( ; (exp < 0) && (precision > 0); exp++, precision--)
		{
			*conv_p++ = '0';
		}

		while(precision > 0)
		{
			*conv_p++ = get_num(&normval, &exp_pos);
			precision--;
		}
	}

	*conv_p = 0;

	strncpy(buf, conv_buf, bufsize);
	buf[bufsize-1] = 0;

	return;
}

unsigned int *get_debug_register(void)
{
	unsigned int *pData;
	unsigned int ptr;

	pData = (unsigned int *) (0x80000000 | ((sceKernelRemoveByDebugSection & 0x03FFFFFF) << 2));
	ptr = ((pData[0] & 0xFFFF) << 16) + (short) (pData[2] & 0xFFFF);

	return (unsigned int *) ptr;
}

int isdir(const char *path)
{
	SceIoStat stat;
	char localpath[1024];
	int len;

	strcpy(localpath, path);
	len = strlen(localpath);
	if(len < 2) /* Not a valid full path */
	{
		return 0;
	}

	if(localpath[len-1] == '/')
	{
		/* Absolute directory, fix for dodgy Sony drivers */
		if((localpath[len-2] == ':') && (strchr(localpath, '/') == &localpath[len-1]))
		{
			return 1;
		}

		localpath[len-1] = 0;
	}

	if(sceIoGetstat(localpath, &stat) == 0)
	{
		if(FIO_SO_ISDIR(stat.st_attr) || FIO_S_ISDIR(stat.st_mode))
		{
			return 1;
		}
	}

	return 0;
}

void enable_kprintf(int enable)
{
	unsigned int *pData;

	pData = get_debug_register();
	if(enable)
	{
		*pData |= DEBUG_REG_KPRINTF_ENABLE;
	}
	else
	{
		*pData &= ~DEBUG_REG_KPRINTF_ENABLE;
	}
}
