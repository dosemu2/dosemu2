/*
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* a port of CMDLINE.AWK */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "config.h"
#include "emu.h"
#include "memory.h"
#include "doshelpers.h"
#include "../coopthreads/coopthreads.h"

#define printf	com_printf
#define perror	com_printf
#define read	com_dosread

int msetenv(char *,char *);

#define CMDBUFFSIZE 10000
/* This program just reads stdin... */
int cmdline_main()
{
	char *buff = malloc(CMDBUFFSIZE), *p, *q, *endb;


	if (!buff)
	    perror("malloc failure");
	    return(1);
	*(endb = buff + read(0,buff,CMDBUFFSIZE)) = '\0';
	for (p = buff; p < endb; p = q + strlen(q)+1)
		 if (*p != '-' && ((q = strchr(p,'='))!=0)) {
		     *q++ = '\0';
		     if (msetenv(p,q))
			 return (1);
		     }
		 else q = p;
	return 0;
}
