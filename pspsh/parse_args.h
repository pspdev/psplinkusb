/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * parse_args.h - PSPLINK argument parser code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.pspdev.org/psp/branches/psplinkusb/psplink/parse_args.h $
 * $Id: parse_args.h 1604 2005-12-18 13:55:03Z tyranid $
 */

#define REDIR_TYPE_NONE 0
#define REDIR_TYPE_NEW 1
#define REDIR_TYPE_CAT 2

int parse_cli(const char *in, char *out, int *argc, char **argv, int max_args, int sargc, char **sargv, int *type, char *filename);
const char *parse_redir(const char *in, char *filename, int *type);
