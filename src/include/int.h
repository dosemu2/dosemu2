/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef INT_H
#define INT_H

#include "extern.h"
#include <sys/types.h> /* for u_char */
#include <time.h> /* for time_t */


EXTERN unsigned int  check_date INIT(0);
EXTERN time_t        start_time;
EXTERN char video_ints[256];

void do_int(int);
void fake_int(int, int);
void fake_int_to(int cs, int ip);
void fake_call(int, int);
void fake_call_to(int cs, int ip);
void fake_pusha(void);
void setup_interrupts(void);
void version_init(void);
void int_vector_setup(void);

#define REVECT		0
#define NO_REVECT	1

extern int can_revector(int i);

extern int redir_state;
extern void set_int21_revectored(int);

int dos_helper(void);

void do_periodic_stuff(void);

#endif
