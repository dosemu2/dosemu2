/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "io.h"
#include "midid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>

/***********************************************************************
  Output
 ***********************************************************************/

void error(char *err)
{
	fprintf(stderr, "Fatal error: %s\n", err);
	exit(1);
}

/***********************************************************************
  Input from stdin with 1 byte buffer
 ***********************************************************************/

int getbyte_buffer=MAGIC_EOF;

void getbyte_next(void)
/* Read next symbol into buffer */
{
        fd_set rfds;
        struct timeval tv;
        byte ch;
        bool is_eof;

        tv.tv_sec = config.timeout;
        tv.tv_usec = 0;
again:
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
	switch (select(fd + 1, &rfds, NULL, NULL, config.timeout ? &tv : NULL)) {
	    case -1:
		if (tv.tv_sec + tv.tv_usec == 0) {
		    getbyte_buffer = MAGIC_TIMEOUT;
		    break;
		}
		if (errno == EINTR)
		    goto again;
		getbyte_buffer = MAGIC_EOF;
		break;
	    case 0:
		if (errno == EINTR && tv.tv_sec + tv.tv_usec > 0)
		    goto again;
		getbyte_buffer = MAGIC_TIMEOUT;
		break;
	    default:
		is_eof = (read(fd, &ch, 1) == 0);
		getbyte_buffer = ch;
		if (is_eof)
		  getbyte_buffer = MAGIC_EOF;
		break;
	}
	if (debugall) fprintf(stderr,"getbyte_next=0x%02x (%i)\n",
			      getbyte_buffer,getbyte_buffer);
}

int getbyte(void)
/* Return current contents of buffer */
{
	return getbyte_buffer;
}

int getbyte_status(void)
/* Tries to read a status byte (==bit 7 set) */
{
	bool accept=(getbyte_buffer & 0x80);
	int ch=getbyte_buffer;
	if (!accept) {
		ch=MAGIC_INV;
		if (debugall) fprintf(stderr,"Status not specified, using previous\n");
	}
	return(ch);
}

int getbyte_data(void)
/* Tries to read a data byte (==bit 7 reset) */
{
	bool accept=(!(getbyte_buffer & 0x80));
	int ch=getbyte_buffer;
	if (!accept) {
		ch=MAGIC_INV;
		if (warning) fprintf(stderr,"Warning: got an illegal data byte=%i\n",getbyte_buffer);
	}
	return(ch);
}
