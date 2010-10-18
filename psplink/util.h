/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * util.h - header file for util.c
 *
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/util.h $
 * $Id: util.h 2316 2007-09-17 18:13:33Z tyranid $
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#define MAX_BUFFER 4096
#define TYPE_FILE	1
#define TYPE_DIR	2
#define TYPE_ETHER	3

typedef struct _PspFile
{
	int fd;
	char read_buf[MAX_BUFFER];
	int  read_size;
	int  read_pos;
} PspFile;

int is_hex(char ch);
int is_oct(char ch);
int hex_to_int(char ch);
int oct_to_int(char ch);
char upcase(char ch);
int is_aspace(int ch);
int is_alnum(char ch);
int build_bootargs(char *args, const char *bootfile, const char *execfile, int argc, char **argv);
int build_args(char *args, const char *execfile, int argc, char **argv);
int handlepath(const char *currentdir, const char *relative, char *path, int type, int validate);
int findinpath(const char *relative, char *output, const char *pathvar);
int load_start_module(const char *name, int argc, char **argv);
int load_start_module_debug(const char *name);
void map_firmwarerev(void);
int init_usbmass(void);
int stop_usbmass(void);
int init_usbhost(const char *bootpath);
int stop_usbhost(void);
void save_execargs(int argc, char **argv);
int openfile(const char *filename, PspFile *pFile);
int closefile(PspFile *pFile);
int fdgetc(PspFile *pFile);
int fdgets(PspFile *pFile, char *buf, int size);
void strip_whitesp(char *s);
int strtoint(const char *str, unsigned int *i);
void* memmem_mask(void *data, void *mask, int len, void *search, int slen);
int memcmp_mask(void *data1, void *data2, void *mask, int len);
int decode_hexstr(const char *str, unsigned char *data, int max);
SceUID refer_module_by_addr(unsigned int addr, SceKernelModuleInfo *info);
SceUID refer_module_by_name(const char *name, SceKernelModuleInfo *info);
int psplinkReferThreadsByModule(int type, SceUID modid, SceUID *uids, int max);
int psplinkReferModule(SceUID uid, SceKernelModuleInfo *info);
SceUID psplinkReferModuleByName(const char *name, SceKernelModuleInfo *info);
int isdir(const char *path);

#define DEBUG_REG_KPRINTF_ENABLE  0x00001000
#define DEBUG_REG_GLOBAL_PROFILER 0x00800000
#define DEBUG_REG_THREAD_PROFILER 0x00C00000
#define DEBUG_REG_PROFILER_MASK   (~DEBUG_REG_THREAD_PROFILER)
unsigned int *get_debug_register(void);
void enable_kprintf(int enable);

#define MODE_GENERIC 0
#define MODE_EXP 1
#define MODE_FLOAT_ONLY 2
void f_cvt(float *val, char *buf, int bufsize, int precision, int mode);

extern int (*g_QueryModuleInfo)(SceUID modid, SceKernelModuleInfo *info);
extern int (*g_GetModuleIdList)(SceUID *readbuf, int readbufsize, int *idcount);
extern int g_isv1;

#endif
