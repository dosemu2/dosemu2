/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef INT_H
#define INT_H

#include "extern.h"


EXTERN void *interrupt_function[0x100];
EXTERN unsigned int  check_date INIT(0);
EXTERN time_t        start_time;
EXTERN u_char in_sigsegv INIT(0);
EXTERN u_char in_sighandler INIT(0);	/* so I know to not use non-reentrant
					 * syscalls like ioctl() :-( */
EXTERN u_char ignore_segv INIT(0);      /* ignore sigsegv's */

void do_int(int);
void setup_interrupts(void);
void version_init(void);

#define REVECT		0
#define NO_REVECT	1

extern int can_revector(int i);

extern int redir_state;
extern void set_int21_revectored(int);

#endif
