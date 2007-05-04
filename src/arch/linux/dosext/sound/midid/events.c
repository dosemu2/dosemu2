/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/***********************************************************************
  event handling: read parameters and put event in driver queue
 ***********************************************************************/

#include "events.h"
#include "midid.h"
#include "io.h"
#include "stats.h"
#include "emulation.h"
#include "device.h"
#include <stdio.h>
#include <sys/soundcard.h>

#define EOX 0xf7

void do_noteoff(int chn)
{
	int note, vel;
	note = getbyte_data();
	if (note<0) return; /* Return at error */
	getbyte_next();
	vel = getbyte_data();
	if (vel<0) return;
	getbyte_next();
	if (debug)
		fprintf(stderr, "note_off(chn=%i,note=%i,vel=%i)\n", chn, note, vel);
	dev_noteoff(chn, note, vel);
}

void do_noteon(int chn)
{
	int note, vel;
	note = getbyte_data();
	if (note<0) return;
	getbyte_next();
	vel = getbyte_data();
	if (vel<0) return;
	getbyte_next();
	if (debug)
		fprintf(stderr, "note_on(chn=%i,note=%i,vel=%i)\n", chn, note, vel);
	dev_noteon(chn, note, vel);
}

void do_notepressure(int chn)
{
	/* Polyphonic key pressure, note pressure */
	int note, vel;
	note = getbyte_data();
	if (note<0) return;
	getbyte_next();
	vel = getbyte_data();
	if (vel<0) return;
	getbyte_next();
	if (debug)
		fprintf(stderr, "note_pressure(chn=%i,note=%i,vel=%i)\n", chn, note, vel);
	dev_notepressure(chn, note, vel);
}

void do_program(int chn)
{
	/* Program change */
	int pgm;
	pgm = getbyte_data();
	if (pgm<0) return;
	getbyte_next();
	if (statistics)
		stats_addprogram(pgm);
	if (debug)
		fprintf(stderr, "change_program(chn=%i,pgm=%i)\n", chn, pgm);
	/* Use mapping for MT32 <-> GM emulation (imap[]) */
	dev_program(chn, imap[pgm]);
}

void do_channelpressure(int chn)
{
	/* Channel pressure */
	int vel;
	vel = getbyte_data();
	if (vel<0) return;
	getbyte_next();
	if (debug)
		fprintf(stderr, "control_change(chn=%i,vel=%i)\n", chn, vel);
	dev_channelpressure(chn, vel);
}

void do_bender(int chn)
{
	/* Pitch wheel, bender */
	int pitch;
	pitch=getbyte_data();
	if (pitch<0) return;
	getbyte_next();
	if (getbyte_data()<0) return;
	pitch += getbyte_data() << 7;
	getbyte_next();
	if (debug)
		fprintf(stderr, "bender(chn=%i,pitch=%i)\n", chn, pitch);
	dev_bender(chn, pitch);
}

void do_controlchange(int chn)
{
	int control;
	int value;
	control = getbyte_data();
	if (control<0) return;
	getbyte_next();
	value = getbyte_data();
	if (value<0) return;
	getbyte_next();
	if (debug)
		fprintf(stderr, "control_change(chn=%i,control=%i,val=%i)\n",
                        chn, control, value);
	dev_control(chn, control, value);
}

void do_sysex(void)
{
#define SYSEX_BUF_LEN 1024
char buf[SYSEX_BUF_LEN];
int ch, len = 0;
	buf[len++] = MIDI_SYSTEM_PREFIX;
	ch = getbyte();
	if (debug)
		fprintf(stderr,"Sysex for vendor ID#%x\n",ch);
	while(1) {
		buf[len++] = ch;
		getbyte_next();
		if (ch == EOX) {
			if (debug)
				fprintf(stderr,"End of SysEx, len=%i\n", len);
			if (imap == imap_default)
				dev_sysex(buf, len);
			return;
		}
		ch = getbyte();
		if (ch<0 || len >= SYSEX_BUF_LEN) return;
		if (debug)
			fprintf(stderr,"Sysex: %x\n",ch);
	}
}

void do_quarter_frame(void)
{
	int value;
	value = getbyte_data();
	if (value<0) return;
	getbyte_next();
	if (warning)
		fprintf(stderr,"Warning: Quarter Frame message is not implemented\n");
}

void do_song_position(void)
{
	int beat, clocks;
	beat = getbyte_data();
	if (beat<0) return;
	getbyte_next();
	if (getbyte_data()<0) return;
	clocks = getbyte_data() << 7;
	getbyte_next();
	if (warning)
		fprintf(stderr,"Warning: Song Position message is not implemented\n");
}

void do_song_select(void)
{
	int song;
	song = getbyte_data();
	if (song<0) return;
	getbyte_next();
	if (warning)
		fprintf(stderr,"Warning: Song Select message is not implemented\n");
}

void do_tune_request(void)
{
	if (warning)
		fprintf(stderr,"Warning: Tune Request message is not implemented\n");
}

void do_midi_clock(void)
{
	if (warning)
		fprintf(stderr,"Warning: MIDI Clock message is not implemented\n");
}

void do_midi_tick(void)
{
	if (warning)
		fprintf(stderr,"Warning: MIDI Tick message is not implemented\n");
}

void do_midi_start(void)
{
	if (warning)
		fprintf(stderr,"Warning: MIDI Start message is not implemented\n");
}

void do_midi_continue(void)
{
	if (warning)
		fprintf(stderr,"Warning: MIDI Continue message is not implemented\n");
}

void do_midi_stop(void)
{
	if (warning)
		fprintf(stderr,"Warning: MIDI Stop message is not implemented\n");
}

void do_active_sensing(void)
{
	if (warning)
		fprintf(stderr,"Warning: Active Sensing message is not implemented\n");
}

void do_reset(void)
{
	if (warning)
		fprintf(stderr,"Warning: Reset message is not implemented\n");
}
