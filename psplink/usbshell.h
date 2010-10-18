/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * usbshell.h - PSPLINK kernel module usb shell code
 *
 * Copyright (c) 2006 James F <tyranid@gmail.com>
 *
 * $HeadURL$
 * $Id$
 */

int usbShellInit(void);
int usbShellReadInput(unsigned char *cli, char **argv, int max_cli, int max_arg);
