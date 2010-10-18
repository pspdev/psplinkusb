/*
 * PSPLINK
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPLINK root for details.
 *
 * psplinkcnf.c - Functions to manipulate the configuration file
 *
 * Copyright (c) 2005 James F <tyranid@gmail.com>
 *
 * $HeadURL: svn://svn.ps2dev.org/psp/trunk/psplinkusb/psplink/psplinkcnf.c $
 * $Id: psplinkcnf.c 1766 2006-01-29 00:03:02Z tyranid $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <string.h>
#include "psplink.h"
#include "psplinkcnf.h"
#include "util.h"

int psplinkConfigOpen(const char *filename, struct ConfigFile *cnf)
{
	int iRet = 0;

	do
	{
		if(cnf == NULL)
		{
			Kprintf("Error, invalid configuration structure\n");
			break;
		}

		memset(cnf, 0, sizeof(struct ConfigFile));

		if(openfile(filename, &cnf->file))
		{
			iRet = 1;
		}
	}
	while(0);

	return iRet;
}

void psplinkConfigClose(struct ConfigFile *cnf)
{
	do
	{
		if(cnf == NULL)
		{
			Kprintf("Error, invalid configuration structure\n");
			break;
		}

		closefile(&cnf->file);
	}
	while(0);
}

const char* psplinkConfigReadNext(struct ConfigFile *cnf, const char **name)
{
	const char *pVal = NULL;

	do
	{
		if(cnf == NULL)
		{
			Kprintf("Error, invalid configuration structure\n");
			break;
		}

		while((pVal == NULL) && (fdgets(&cnf->file, cnf->str_buf, MAX_BUFFER)))
		{
			char *eq_pos;

			cnf->line++;
			strip_whitesp(cnf->str_buf);
			if((cnf->str_buf[0] == '#') || (cnf->str_buf[0] == 0))
			{
				continue;
			}

			eq_pos = strchr(cnf->str_buf, '=');
			if(eq_pos == NULL)
			{
				Kprintf("Error on line %d of configuration file. No '='\n", cnf->line);
				continue;
			}

			*eq_pos = 0;
			*name = cnf->str_buf;
			eq_pos++;
			if(*eq_pos == '"')
			{
				char *qend;
				eq_pos++;
				qend = strchr(eq_pos, '"');
				if(qend == NULL)
				{
					Kprintf("Error on line %d of configuration file. No closing quote\n", cnf->line);
					continue;
				}
				else
				{
					*qend = 0;
				}
			}
			pVal = eq_pos;
		}
	}
	while(0);

	return pVal;
}

