/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include"io.h"

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
	byte ch;
	bool is_eof;
	is_eof=(!read(fd, &ch, 1));
	getbyte_buffer=ch;
	if (is_eof) getbyte_buffer=MAGIC_EOF;
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
		if (warning) fprintf(stderr,"Warning: got an illegal status byte=%i\n",getbyte_buffer);
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
