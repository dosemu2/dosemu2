/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef INT_H
#define INT_H

#include "extern.h"
#include <sys/types.h> /* for u_char */
#include <time.h> /* for time_t */


EXTERN void *interrupt_function[0x100];
EXTERN unsigned int  check_date INIT(0);
EXTERN time_t        start_time;

void do_int(int);
void fake_int(int, int);
void fake_call(int, int);
void fake_pusha(void);
void setup_interrupts(void);
void version_init(void);

#define REVECT		0
#define NO_REVECT	1

extern int can_revector(int i);

extern int redir_state;
extern void set_int21_revectored(int);

#endif
