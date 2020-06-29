/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * stdio.h - Module library code for psplink
 *
 * Copyright (c) 2006 James F
 *
 */
#ifndef __PSP_STDIO_H__
#define __PSP_STDIO_H__

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int stdioTtyInit(void);
int stdioInstallStdinHandler(PspDebugInputHandler handler);
int stdioInstallStdoutHandler(PspDebugPrintHandler handler);
int stdioInstallStderrHandler(PspDebugPrintHandler handler);

#endif
