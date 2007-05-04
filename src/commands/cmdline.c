/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* a port of CMDLINE.AWK */
#if !defined( __LARGE__) && !defined(__COMPACT__)
#error "Use the LARGE or COMPACT model!"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <dos.h>
#include "detect.h"

int msetenv(char *,char *);

#define CMDBUFFSIZE 10000
/* This program just reads stdin... */
main()
{
	char *buff = malloc(CMDBUFFSIZE), *p, *q, *endb;

  if (!is_dosemu()) {
    printf("This program requires DOSEMU to run, aborting\n");
    exit(1);
  }

	if (!buff)
	    perror("malloc failure"),
	    exit(1);
	*(endb = buff + read(0,buff,CMDBUFFSIZE)) = '\0';
	for (p = buff; p < endb; p = q + strlen(q)+1)
		 if (*p != '-' && ((q = strchr(p,'='))!=0)) {
		     *q++ = '\0';
		     if (msetenv(p,q))
			 exit(1);
		     }
		 else q = p;
}
