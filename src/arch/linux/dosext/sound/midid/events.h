/* 
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Interface to device driver */

#include"midid.h"

void do_noteoff(int chn);
void do_noteon(int chn);
void do_notepressure(int chn);
void do_program(int chn);
void do_channelpressure(int chn);
void do_bender(int chn);
void do_sysex(void);
void do_allnotesoff(void);
void do_modemessage(int chn,int control);
void do_controlchange(int chn);
