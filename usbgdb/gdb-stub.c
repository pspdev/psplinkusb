/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * gdb-stub.c - GDB stub for psplink
 *
 * Copyright (c) 2005 Julian T <lovely@crm114.net>
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/usbgdb/gdb-stub.c $
 * $Id: gdb-stub.c 2179 2007-02-15 17:38:02Z tyranid $
 */
/* Note: there is the odd small bit which comes from the gdb stubs/linux mips stub */
/* As far as I am aware they didn't have an explicit license on them so... */

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspsdk.h>
#include <psputilsforkernel.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <debug.h>
#include <exception.h>
#include <usbasync.h>
#include <util.h>
#include <thctx.h>
#include "common.h"

#define MAX_BUF 2048

void psplinkReset(void);

static char input[MAX_BUF];
static char output[MAX_BUF];
static const char hexchars[]="0123456789abcdef";

/*
 * send the packet in buffer.
 */
static int putpacket(unsigned char *buffer)
{
	static unsigned char outputbuffer[4096];
	unsigned char checksum;
	int count;
	unsigned char ch;
	int i = 0;

	/*
	 * $<packet info>#<checksum>.
	 */

	do {
		outputbuffer[i++] = '$';
		checksum = 0;
		count = 0;

		while ((ch = buffer[count]) != 0) {
			checksum += ch;
			count += 1;
			outputbuffer[i++] = ch;
		}

		outputbuffer[i++] = '#';

		outputbuffer[i++] = hexchars[(checksum >> 4) & 0xf];

		outputbuffer[i++] = hexchars[checksum & 0xf];

		DEBUG_PRINTF("Writing %.*s\n", i, outputbuffer);
		writeDebugData(outputbuffer, i);

		DEBUG_PRINTF("calculated checksum = %02X\n", checksum);

		if(getDebugChar(&ch) <= 0)
		{
			return 0;
		}
	}
	while ((ch & 0x7f) != '+');

	return 1;
}

/*
 * Convert ch from a hex digit to an int
 */
static int hex(unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

/*
 * scan for the sequence $<data>#<checksum>
 */
static int getpacket(char *buffer)
{
	unsigned char checksum;
	unsigned char xmitcsum;
	int i;
	int count;
	unsigned char ch;

	do {
		/*
		 * wait around for the start character,
		 * ignore all other characters
		 */
		ch = 0;
		while((ch & 0x7F) != '$')
		{
			if(getDebugChar(&ch) <= 0)
			{
				return 0;
			}
		}

		checksum = 0;
		xmitcsum = -1;
		count = 0;

		/*
		 * now, read until a # or end of buffer is found
		 */
		while (count < MAX_BUF) {
			if(getDebugChar(&ch) <= 0)
			{
				return 0;
			}

			if (ch == '#')
				break;
			checksum = checksum + ch;
			buffer[count] = ch;
			count = count + 1;
		}

		if (count >= MAX_BUF)
			continue;

		buffer[count] = 0;

		if (ch == '#') {
			if(getDebugChar(&ch) <= 0)
			{
				return 0;
			}
			xmitcsum = hex(ch & 0x7f) << 4;
			if(getDebugChar(&ch) <= 0)
			{
				return 0;
			}
			xmitcsum |= hex(ch & 0x7f);

			if (checksum != xmitcsum)
				putDebugChar('-');	/* failed checksum */
			else {
				putDebugChar('+'); /* successful transfer */

				/*
				 * if a sequence char is present,
				 * reply the sequence ID
				 */
				if (buffer[2] == ':') {
					putDebugChar(buffer[0]);
					putDebugChar(buffer[1]);

					/*
					 * remove sequence chars from buffer
					 */
					count = strlen(buffer);
					for (i=3; i <= count; i++)
						buffer[i-3] = buffer[i];
				}
			}
		}
	}
	while (checksum != xmitcsum);

	return 1;
}

static struct hard_trap_info {
	unsigned char tt;		/* Trap type code for MIPS R3xxx and R4xxx */
	unsigned char signo;		/* Signal that we map this trap into */
} hard_trap_info[] = {
	{ 6, SIGBUS },			/* instruction bus error */
	{ 7, SIGBUS },			/* data bus error */
	{ 9, SIGTRAP },			/* break */
	{ 10, SIGILL },			/* reserved instruction */
	{ 12, SIGFPE },			/* overflow */
	{ 13, SIGTRAP },		/* trap */
	{ 14, SIGSEGV },		/* virtual instruction cache coherency */
	{ 15, SIGFPE },			/* floating point exception */
	{ 23, SIGSEGV },		/* watch */
	{ 31, SIGSEGV },		/* virtual data cache coherency */
	{ 0, 0}				/* Must be last */
};

static int computeSignal(int tt)
{
	struct hard_trap_info *ht;

	for (ht = hard_trap_info; ht->tt && ht->signo; ht++)
		if (ht->tt == tt)
			return ht->signo;

	return SIGHUP;		/* default for things we don't know about */
}

static char *mem2hex(unsigned char *mem, char *buf, int count)
{
	unsigned char ch;

	while (count-- > 0) {
		if (GdbReadByte(mem++, &ch) == 0)
		{
			return NULL;
		}

		*buf++ = hexchars[(ch >> 4) & 0xf];
		*buf++ = hexchars[ch & 0xf];
	}

	*buf = 0;

	return buf;
}

static char *hex2mem(char *buf, char *mem, int count, int binary)
{
	int i;
	unsigned char ch;

	for (i=0; i<count; i++)
	{
		if (binary) {
			ch = *buf++;
			if (ch == 0x7d)
				ch = 0x20 ^ *buf++;
		}
		else {
			ch = hex(*buf++) << 4;
			ch |= hex(*buf++);
		}
		if (GdbWriteByte(ch, (unsigned char *) mem++) == 0)
			return 0;
	}

	return mem;
}

static int hexToInt(char **ptr, unsigned int *intValue)
{
	int numChars = 0;
	int hexValue;

	*intValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (hexValue < 0)
			break;

		*intValue = (*intValue << 4) | hexValue;
		numChars ++;

		(*ptr)++;
	}

	return numChars;
}

static char *strtohex(char *ptr, const char *str)
{
	while(*str)
	{
		*ptr++ = hexchars[*str >> 4];
		*ptr++ = hexchars[*str & 0xf];
		str++;
	}
	*ptr = 0;

	return ptr;
}

void build_trap_cmd(int sigval, struct PsplinkContext *ctx)
{
	char *ptr;
	/*
	 * reply to host that an exception has occurred
	 */
	ptr = output;
	*ptr++ = 'T';
	*ptr++ = hexchars[(sigval >> 4) & 0xf];
	*ptr++ = hexchars[sigval & 0xf];

	/*
	 * Send Error PC
	 */
	*ptr++ = hexchars[37 >> 4];
	*ptr++ = hexchars[37 & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((unsigned char *) &ctx->regs.epc, ptr, sizeof(u32));
	*ptr++ = ';';

	/*
	 * Send frame pointer
	 */
	*ptr++ = hexchars[30 >> 4];
	*ptr++ = hexchars[30 & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((unsigned char *)&ctx->regs.r[30], ptr, sizeof(u32));
	*ptr++ = ';';

	/*
	 * Send stack pointer
	 */
	*ptr++ = hexchars[29 >> 4];
	*ptr++ = hexchars[29 & 0xf];
	*ptr++ = ':';
	ptr = mem2hex((unsigned char *)&ctx->regs.r[29], ptr, sizeof(u32));
	*ptr++ = ';';

	sprintf(ptr, "thread:%08x;", ctx->thid);
	ptr += strlen(ptr);

	*ptr++ = 0;
}

static void handle_query(struct PsplinkContext *ctx, char *str)
{
	static SceUID threads[20];
	static int thread_count = 0;
	static int thread_loc = 0;

	switch(str[0])
	{
		case 'C': sprintf(output, "QC%08X", ctx->thid);
				  break;
		case 'f': 
				if(strncmp(str, "fThreadInfo", strlen("fThreadInfo")) == 0)
				{
					thread_count = psplinkReferThreadsByModule(SCE_KERNEL_TMID_Thread, g_context.uid, threads, 20);

					if(thread_count > 0)
					{
						thread_loc = 0;
						sprintf(output, "m%08X", threads[thread_loc]);
						thread_loc++;
					}
				}
				break;

		case 's':
				if(strncmp(str, "sThreadInfo", strlen("sThreadInfo")) == 0)
				{
					if(thread_loc < thread_count)
					{
						sprintf(output, "m%08X", threads[thread_loc]);
						thread_loc++;
					}
					else
					{
						strcpy(output, "l");
					}
				}
				break;
		case 'T': if(strncmp(str, "ThreadExtraInfo,", strlen("ThreadExtraInfo,")) == 0)
				  {
					SceKernelThreadInfo info;
					SceUID thid;
					int i;

					str += strlen("ThreadExtraInfo,");
					if(hexToInt(&str, (unsigned int *) &thid))
					{
						memset(&info, 0, sizeof(info));
						info.size = sizeof(info);

						i = sceKernelReferThreadStatus(thid, &info);
						if(i == 0)
						{
							strtohex(output, info.name);
						}
						else
						{
							char temp[32];
							sprintf(temp, "Error: 0x%08X", i);
							strtohex(output, temp);
						}
					}
				  }
				  break;
		case 'P':
				break;
		case 'O': if(strncmp(str, "Offsets", strlen("Offsets")) == 0)
				  {
					  if(!g_context.elf)
					  {
						  u32 text;

						  text = g_context.info.text_addr;

						  sprintf(output, "Text=%08X;Data=%08X;Bss=%08X", text, text, text);
					  }
				  }
				  break;
	};
}

void handle_bp(char *str, int set)
{
	char *ptr;
	unsigned int addr;
	unsigned int len;
	int flags = 0;

	if((!isdigit(str[0])) || (str[1] != ','))
	{
		DEBUG_PRINTF("Invalid Z string (%s)\n", str);
		strcpy(output, "E01");
		return;
	}

	ptr = &str[2];

	if (hexToInt(&ptr, &addr) && *ptr++ == ',' && hexToInt(&ptr, &len))
   	{
		DEBUG_PRINTF("%c%c: addr 0x%08X, len 0x%08X\n", set ? 'Z' : 'z', str[0], addr, len);
		switch(str[0])
		{
			case '1': flags = DEBUG_BP_HARDWARE;
			case '0': 
				if(set)
				{ 
					if(debugSetBP(addr, flags, 0))
					{
						strcpy(output, "OK");
					}
					else
					{
						strcpy(output, "E03");
					}
				}
				else
				{
					if(debugDeleteBP(addr))
					{
						strcpy(output, "OK");
					}
					else
					{
						strcpy(output, "EO3");
					}
				}
				break; 
			default:  break;
		};
	} 
	else
	{
		strcpy(output,"E01");
	}
}

void suspend_module(void)
{
	int intc;
	int count;
	int i;
	SceUID thids[20];

	usbLockBus();
	intc = pspSdkDisableInterrupts();
	count = psplinkReferThreadsByModule(SCE_KERNEL_TMID_Thread, g_context.uid, thids, 20);
	for(i = 0; i < count; i++)
	{
		sceKernelSuspendThread(thids[i]);
	}
	pspSdkEnableInterrupts(intc);
	usbUnlockBus();
}

void resume_module(void)
{
	int intc;
	int count;
	int i;
	SceUID thids[20];

	usbLockBus();
	intc = pspSdkDisableInterrupts();
	count = psplinkReferThreadsByModule(SCE_KERNEL_TMID_Thread, g_context.uid, thids, 20);
	for(i = 0; i < count; i++)
	{
		sceKernelResumeThread(thids[i]);
	}
	pspSdkEnableInterrupts(intc);
	usbUnlockBus();
}

int GdbHandleException (struct PsplinkContext *ctx)
{
	int ret = 1;
	int trap;
	int sigval;
	unsigned int addr;
	unsigned int length;
	char *ptr;
	int bflag = 0;
	SceUID thid = 0;

	DEBUG_PRINTF("In GDB Handle Exception\n");
	suspend_module();

	if(ctx->regs.type == PSPLINK_EXTYPE_DEBUG)
	{
		sigval = SIGTRAP;
	}
	else
	{
		trap = (ctx->regs.cause & 0x7c) >> 2;
		sigval = computeSignal(trap);
	}

	if(sigval == SIGHUP)
	{
		DEBUG_PRINTF("Trap %d\n", trap);
	}

	if(g_context.started)
	{
		build_trap_cmd(sigval, ctx);
		putpacket((unsigned char *) output);
	}

	while(1)
	{
		if(!getpacket(input))
		{
			ret = 0;
			ctx->cont = PSP_EXCEPTION_EXIT;
			goto restart;
		}

		if((input[0] != 'X') && (input[0] != 'x'))
		{
			DEBUG_PRINTF("Received packet '%s'\n", input);
		}
		else
		{
			DEBUG_PRINTF("Received binary packet\n");
		}

		output[0] = 0;

		switch (input[0])
		{
		case '?':
			build_trap_cmd(sigval, ctx);
			break;

		case 'c':
			ptr = &input[1];
			if(!g_context.started)
			{
				int status;

				/* Ensure any pending memory is flushed before we start the module */
				sceKernelDcacheWritebackInvalidateAll();
				sceKernelIcacheInvalidateAll();
				sceKernelStartModule(g_context.uid, g_context.args, g_context.argp, &status, NULL);

				g_context.started = 1;
			}
			else
			{
				if (hexToInt(&ptr, &addr))
				{
					ctx->regs.epc = addr;
				}
			}
	  
			ctx->cont = PSP_EXCEPTION_CONTINUE;
			goto restart;
			break;

		case 'D':
			putpacket((unsigned char *) output);
			ctx->cont = PSP_EXCEPTION_CONTINUE;
			ret = 0;
			goto restart;
			break;

		case 'g':
			{
				struct PsplinkContext thctx;
				struct PsplinkContext *pCtx = NULL;

				if(thid && (thid != ctx->thid))
				{
					if(psplinkGetFullThreadContext(thid, &thctx))
					{
						pCtx = &thctx;
					}
				}
				else
				{
					pCtx = ctx;
				}

				if(pCtx)
				{
					ptr = output;
					ptr = (char*) mem2hex((unsigned char *)&pCtx->regs.r[0], ptr, 32*sizeof(u32)); /* r0...r31 */
					ptr = (char*) mem2hex((unsigned char *)&pCtx->regs.status, ptr, 6*sizeof(u32)); /* cp0 */
					ptr = (char*) mem2hex((unsigned char *)&pCtx->regs.fpr[0], ptr, 32*sizeof(u32)); /* f0...31 */
					ptr = (char*) mem2hex((unsigned char *)&pCtx->regs.fsr, ptr, 2*sizeof(u32)); /* cp1 */
					ptr = (char*) mem2hex((unsigned char *)&pCtx->regs.frame_ptr, ptr, 2*sizeof(u32)); /* frp */
					ptr = (char*) mem2hex((unsigned char *)&pCtx->regs.index, ptr, 16*sizeof(u32)); /* cp0 */
				}
				else
				{
					strcpy(output, "E03");
				}
			}
			break;

		case 'G':
			if(thid && (thid == ctx->thid))
			{
				ptr = &input[1];
				hex2mem(ptr, (char *)&ctx->regs.r[0], 32*sizeof(unsigned int), 0);
				ptr += 32*(2*sizeof(unsigned int));
				hex2mem(ptr, (char *)&ctx->regs.status, 6*sizeof(unsigned int), 0);
				ptr += 6*(2*sizeof(unsigned int));
				hex2mem(ptr, (char *)&ctx->regs.fpr[0], 32*sizeof(unsigned int), 0);
				ptr += 32*(2*sizeof(unsigned int));
				hex2mem(ptr, (char *)&ctx->regs.fsr, 2*sizeof(unsigned int), 0);
				ptr += 2*(2*sizeof(unsigned int));
				hex2mem(ptr, (char *)&ctx->regs.frame_ptr, 2*sizeof(unsigned int), 0);
				ptr += 2*(2*sizeof(unsigned int));
				hex2mem(ptr, (char *)&ctx->regs.index, 16*sizeof(unsigned int), 0);
			}

			/* We just ignore other threads */
			strcpy(output,"OK");
			break;

		/*
		 * mAA..AA,LLLL  Read LLLL bytes at address AA..AA
		 */
		case 'm':
			ptr = &input[1];

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length)) {
				if (mem2hex((unsigned char *)addr, output, length))
					break;
				strcpy (output, "E03");
			} else
				strcpy(output,"E01");
			break;

		/*
		 * XAA..AA,LLLL: Write LLLL escaped binary bytes at address AA.AA
		 */
		case 'X':
			bflag = 1;
			/* fall through */

		/*
		 * MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK
		 */
		case 'M':
			ptr = &input[1];

			if (hexToInt(&ptr, &addr)
				&& *ptr++ == ','
				&& hexToInt(&ptr, &length)
				&& *ptr++ == ':') {
				if (hex2mem(ptr, (char *)addr, length, bflag))
					strcpy(output, "OK");
				else
					strcpy(output, "E03");
			}
			else
				strcpy(output, "E02");
			bflag = 0;
			break;

		case 's': 	ptr = &input[1];
					if (hexToInt(&ptr, &addr))
					{
						ctx->regs.epc = addr;
					}

					ctx->cont = PSP_EXCEPTION_STEP;
					goto restart;
					break;

		case 'H': if(input[1] == 'g')
				  {
					  ptr = &input[2];
					  if(hexToInt(&ptr, &addr))
					  {
						  thid = addr;
						  DEBUG_PRINTF("Setting thread to 0x%08X\n", thid);
						  strcpy(output, "OK");
					  }
					  else
					  {
						  strcpy(output, "EO3");
					  }
				  }
				  break;
		case 'q': handle_query(ctx, &input[1]);
				  break;

		case 'Z': handle_bp(&input[1], 1);
				  break;

		case 'z': handle_bp(&input[1], 0);
				  break;

		/*
		 * kill the program; let us try to restart the machine
		 * Reset the whole machine.
		 */
		case 'k':
		case 'r':	
				  psplinkReset();
			break;

		default:
			break;
		}
		/*
		 * reply to the request
		 */

		putpacket((unsigned char *) output);

	} /* while */

restart:
	/* Flush caches */
	sceKernelDcacheWritebackInvalidateAll();
	sceKernelIcacheInvalidateAll();
	resume_module();

	return ret;
}
