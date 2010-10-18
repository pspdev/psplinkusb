/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * parse_args.c - PSPLINK argument parser code
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/pspsh/parse_args.C $
 * $Id: parse_args.C 2222 2007-04-23 19:12:30Z tyranid $
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "parse_args.h"

int hex_to_int(char ch)
{
	ch = toupper(ch);
	if((ch >= '0') && (ch <= '9'))
	{
		return ch-'0';
	}
	else if((ch >= 'A') && (ch <= 'F'))
	{
		return ch-'A';
	}
	
	return 0;
}

int oct_to_int(char ch)
{
	if((ch >= '0') && (ch < '8'))
	{
		return ch-'0';
	}

	return 0;
}

int isodigit(char ch)
{
	if((ch >= '0') && (ch < '8'))
	{
		return 1;
	}

	return 0;
}

int decode_hex(const char *str, unsigned char *ch)
{
	int i;

	*ch = 0;
	for(i = 0; i < 2; i++)
	{
		if(!isxdigit(str[i]))
		{
			break;
		}
		*ch = *ch << 4;
		*ch = *ch | hex_to_int(str[i]);
	}
	if(i == 0)
	{
		printf("Missing following hex characters\n");
		return 0;
	}
	if(*ch == 0)
	{
		printf("Invalid hex character (not allowed NULs)\n");
		return 0;
	}

	return i;
}

int decode_oct(const char *str, unsigned char *ch)
{
	int i;

	*ch = 0;
	for(i = 0; i < 4; i++)
	{
		if(!isodigit(str[i]))
		{
			break;
		}
		*ch = *ch << 3;
		*ch = *ch | oct_to_int(str[i]);
	}
	if(i == 0)
	{
		printf("Missing following octal characters\n");
		return 0;
	}
	if(*ch == 0)
	{
		printf("Invalid octal character (not allowed NULs)\n");
		return 0;
	}

	return i;
}

/* Insert an arg or environment variable */
void insert_arg(const char **pin, char **pout, int sargc, char**sargv)
{
	const char *in = *pin;
	char *out = *pout;

	/* If an argument */
	if(isdigit(*in))
	{
		char *endp;
		int val = strtoul(in, &endp, 10);
		in = endp;
		if((val < sargc) && (sargv) && (sargv[val]))
		{
			int len = strlen(sargv[val]);
			memcpy(out, sargv[val], len);
			out += len;
		}
	}
	else
	{
		char name[PATH_MAX];
		char *pname = name;

		if(*in == '(') /* Alpha numeric variables $(name) */
		{
			in++;
			while((*in != 0) && (*in != ')'))
			{
				*pname++ = *in++;
			}

			if(*in != ')')
			{
				/* Error, escape with an empty string */
				fprintf(stderr, "Warning: No matching ')' for variable\n");
				name[0] = 0;
			}
			else
			{
				in++;
			}

			*pname = 0;
		}
		/* Punctuation, internal variables */
		else if(ispunct(*in))
		{
			name[0] = *in++;
			name[1] = 0;
		}
		else
		{
			/* Just restore the dollar sign */
			name[0] = 0;
			*out++ = '$';
		}
		if(name[0])
		{
			char *val = getenv(name);
			int len;
			if(val)
			{
				len = strlen(val);
				memcpy(out, val, len);
				out += len;
			}
		}
	}

	*pout = out;
	*pin = in;
}

/* Read a single string from the output, escape characters and quotes, insert $ args */
int read_string(const char **pin, char **pout, int sargc, char **sargv)
{
	int in_quote = 0;
	const char *in = *pin;
	char *out = *pout;
	int len = 0;
	int error = 0;

	while(isspace(*in))
	{
		in++;
	}

	if(*in == 0)
	{
		*pin = in;
		return 0;
	}

	while((*in != 0) && (error == 0))
	{
		/* Escape character */
		if(*in == '\\')
		{
			if(in_quote == '\'')
			{
				*out++ = *in++;
				if(*in)
				{
					*out++ = *in++;
				}
			}
			else
			{
				/* Skip the escape */
				in++;
				switch(*in)
				{
					case 'n': *out++ = 10;
							  in++;
							  break;
					case 'r': *out++ = 13;
							  in++;
							  break;
					case '0': /* Octal */
							  {
								  int i;
								  unsigned char ch;
								  in++;
								  i = decode_oct(in, &ch);
								  if((i == 0) || (i == 1))
								  {
									  error = 1;
									  break;
								  }
								  in += i;
								  *out++ = ch;
							  }
							  break;
					case 'x': /* Hexadecimal */
							  {
								  int i;
								  unsigned char ch;
								  in++;
								  i = decode_hex(in, &ch);
								  if((i == 0) || (i == 1))
								  {
									  error = 1;
									  break;
								  }
								  in += i;
								  *out++ = ch;
							  }
							  break;
					case 0  : break; /* End of string */
					default : *out++ = *in++;
							  break;
				};
			}
		}
		else
		{
			if((isspace(*in)) && (in_quote == 0))
			{
				while(isspace(*in))
				{
					in++;
				}
				break;
			}
			else if((*in == '>') && (in_quote == 0))
			{
				break;
			}
			else if((*in == '"') || (*in == '\''))
			{
				if(in_quote)
				{
					if(*in == in_quote)
					{
						in_quote = 0;
						in++;
					}
					else
					{
						*out++ = *in++;
					}
				}
				else
				{
					in_quote = *in;
					in++;
				}
			}
			else
			{
				if((*in == '$') && (in_quote != '\''))
				{
					in++;
					insert_arg(&in, &out, sargc, sargv);
				}
				else
				{
					*out++ = *in++;
				}
			}
		}
	}

	if(in_quote)
	{
		printf("Missing matching quote %c\n", in_quote);
	}
	else if(error)
	{
		printf("Error in command line\n");
	}
	else
	{
		*out++ = 0;
		len = out - *pout;
	}

	*pin = in;
	*pout = out;
	return len;
}

int parse_cli(const char *in, char *out, int *argc, char **argv, int max_args, int sargc, char **sargv, int *type, char *redir)
{
	char *lastout;
	char *outstart = out;

	if((in == NULL) || (out == NULL) || (argc == NULL) || (argv == NULL) || (max_args <= 0) || (type == NULL) || (redir == NULL))
	{
		printf("Error in parse_args, invalid arguments\n");
		return 0;
	}

	*argc = 0;
	*type = REDIR_TYPE_NONE;

	/* Skip any leading white space */
	while(isspace(*in))
	{
		in++;
	}

	if(*in == 0)
	{
		return 0;
	}

	lastout = out;
	while(*argc < (max_args-1))
	{
		/* Parse shell characters */
		if(*in == '>')
		{
			char *outfile = redir;

			in++;
			if(*in == '>')
			{
				*type = REDIR_TYPE_CAT;
				in++;
			}
			else
			{
				*type = REDIR_TYPE_NEW;
			}

			if(read_string(&in, &outfile, sargc, sargv) == 0)
			{
				printf("Error in redirection, no filename\n");
				return 0;
			}
		}
		else
		{
			if(read_string(&in, &out, sargc, sargv) == 0)
			{
				break;
			}

			argv[*argc] = lastout;
			*argc += 1;
			lastout = out;
		}
	}

	argv[*argc] = NULL;
	/* A command ends with a 1 */
	*out++ = 1;

	return out-outstart;
}

#ifdef _TEST
int main(void)
{
	char str[1024];
	char out[1024];

	while(fgets(str, sizeof(str), stdin))
	{
		char *argv[16];
		char redir[1025];
		int  type = 0;
		int  argc;
		int binlen;

		binlen = parse_cli("test me\n", out, &argc, argv, 16, 0, NULL, &type, redir);
		if(binlen > 0)
		{
			int i;
			for(i = 0; i < argc; i++)
			{
				printf("Arg %d: '%s'\n", i, argv[i]);
			}

			for(i = 0; i < binlen; i++)
			{
				if(out[i] < 32)
				{
					printf("\\x%02X", out[i]);
				}
				else
				{
					printf("%c", out[i]);
				}
			}
			printf("\n");
			if(type > 0)
			{
				printf("Redir type %d, '%s'\n", type, redir);
			}
		}
	}
}
#endif
