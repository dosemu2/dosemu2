/* 
 * (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Output to screen and input from stream routines */

#include "midid.h"

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
