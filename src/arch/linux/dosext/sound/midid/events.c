/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/***********************************************************************
  event handling: read parameters and put event in driver queue
 ***********************************************************************/

#include"events.h"
#include"io.h"
#include"stats.h"
#include"emulation.h"

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
	dev->noteoff(chn, note, vel);
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
	if (vel) {
		dev->noteon(chn, note, vel);
	} else {
		/* No velocity */
		dev->noteoff(chn, note, vel);
	}
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
	dev->notepressure(chn, note, vel);
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
	dev->program(chn, imap[pgm]);
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
	dev->channelpressure(chn, vel);
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
	dev->bender(chn, pitch);
}

void do_sysex(void)
{
	int ch; /* Last read char */
	ch = getbyte_data();
	if (ch<0) return;
	getbyte_next();
	if (debug)
		fprintf(stderr,"Sysex for vendor ID#%i\n",ch);
	while(1) {
		ch = getbyte_data();
		if (ch<0) break;
		getbyte_next();
	}
	if (getbyte()==EOX) getbyte_next();
	else
		if (debug)
			fprintf(stderr,"Sysex event wasn't terminated by EOX\n");
}

void do_allnotesoff(void)
{
	/* NYI */
	if (warning)
		fprintf(stderr,"do_allnotesoff NYI\n");
}

void do_modemessage(int chn,int control)
{
	int value;
	if (debug)
		fprintf(stderr,"mode message (chn=%i,control=%i)\n",
                        chn,control);
	switch(control) {
	case 122:
		value=getbyte_data();
		if (value<0) return;
		getbyte_next();
		if (ignored)
			fprintf(stderr,
                                "Ignored: Local control (chn=%i,val=%i\n",
                                chn,value);
		break;
	case 123:
		/* all notes off */
		do_allnotesoff();
		break;
	case 124:
		if (ignored) fprintf(stderr,"Ignored: omni mode off\n");
		do_allnotesoff();
		break;
	case 125:
		if (ignored) fprintf(stderr,"Ignored: omni mode on\n");
		do_allnotesoff();
		break;
	case 126:
		value = getbyte_data();
		if (value<0) return;
		getbyte_next();
		if (ignored) fprintf(stderr,"Ignored: monophonic mode, %i channels\n",value);
		do_allnotesoff();
		break;
	case 127:
		if (ignored) fprintf(stderr,"Ignored: polyphonic mode\n");
		do_allnotesoff();
		break;
	default:
		assert(FALSE);
	}
}

void do_controlchange(int chn)
{
	int control;
	int value;
	control = getbyte_data();
	if (control<0) return;
	getbyte_next();
	if ((control>=122)&&(control<=127)) do_modemessage(chn,control);
	/* Unhandled controller; let the driver decide */
	value = getbyte();
	if (value<0) return;
	getbyte_next();
	if (debug)
		fprintf(stderr, "control_change(chn=%i,control=%i,val=%i)\n",
                        chn, control, value);
	dev->control(chn, control, value);
}

