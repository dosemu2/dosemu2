/* a port of CMDLINE.AWK */
#ifndef __LARGE__
#error "Use the LARGE model!"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <dos.h>

int msetenv(char *,char *);

#define CMDBUFFSIZE 10000
/* This program just reads stdin... */
main()
{
	char *buff = malloc(CMDBUFFSIZE), *p, *q, *endb;
	if (!buff)
	    perror("malloc failure"),
	    exit(1);
	*(endb = buff + read(0,buff,CMDBUFFSIZE)) = '\0';
	for (p = buff; p < endb; p = q + strlen(q)+1)
		 if (*p != '-' && (q = strchr(p,'='))) {
		     *q++ = '\0';
		     if (msetenv(p,q))
			 exit(1);
		     }
		 else q = p;
}
