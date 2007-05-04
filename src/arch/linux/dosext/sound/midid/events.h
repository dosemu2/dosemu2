/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef _EVENTS_H
#define _EVENTS_H

/* Interface to device driver */

void do_noteoff(int chn);
void do_noteon(int chn);
void do_notepressure(int chn);
void do_program(int chn);
void do_channelpressure(int chn);
void do_bender(int chn);
void do_allnotesoff(void);
void do_modemessage(int chn,int control);
void do_controlchange(int chn);

void do_sysex(void);
void do_quarter_frame(void);
void do_song_position(void);
void do_song_select(void);
void do_tune_request(void);
void do_midi_clock(void);
void do_midi_tick(void);
void do_midi_start(void);
void do_midi_continue(void);
void do_midi_stop(void);
void do_active_sensing(void);
void do_reset(void);

#endif
