/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Output to screen and input from stream routines */

#include"midid.h"

void error(char *err);

/* Special return values; must be smaller then 0 */
#define MAGIC_EOF (-1)    /* End Of File */
#define MAGIC_INV (-2)    /* invalid byte */

void getbyte_next(void);
int getbyte(void);
int getbyte_status(void);
int getbyte_data(void);
