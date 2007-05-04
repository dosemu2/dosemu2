/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef _IO_H
#define _IO_H

/* Output to screen and input from stream routines */

#include "device.h"

/* Special return values; must be smaller then 0 */
#define MAGIC_EOF (-1)    /* End Of File */
#define MAGIC_INV (-2)    /* invalid byte */
#define MAGIC_TIMEOUT (-3)

void getbyte_next(void);
int getbyte(void);
int getbyte_status(void);
int getbyte_data(void);

extern Device *devices;

/* File descriptor of input file */
extern int fd;		/* file descriptor */

#endif
