/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/***********************************************************************
  GUS device
 ***********************************************************************/

#include "midid.h"
#include <libgus.h>
#include <assert.h>

int gusdevice;

bool gus_detect(void)
{
	return (gus_cards() > 0);
}

bool gus_init(void)
{
	gus_midi_device_t *device;
  	if ( gus_midi_open_intelligent( GUS_MIDI_OUT, NULL, 2048, 0 ) < 0 )
	      return(FALSE);

	device = gus_midi_output_devices();
	assert(device);
	gusdevice=device->device;

	gus_midi_reset();
     	if (device -> cap & GUS_MIDI_CAP_MEMORY)
        	gus_midi_memory_reset(gusdevice);

	/* We set the timer to a random value since we don't use it.
	   But when we don't start the timer, the program blocks. Why? */
	gus_midi_timer_base(100);
	gus_midi_timer_tempo(100);
	gus_midi_timer_start();
	return (TRUE);
}

void gus_done(void)
{
	gus_midi_close();
}

void gus_flush(void)
{
	gus_midi_flush();
}

void gus_noteon(int chn, int note, int vel)
{
	gus_midi_note_on(gusdevice, chn, note, vel);
}

void gus_noteoff(int chn, int note, int vel)
{
	gus_midi_note_off(gusdevice, chn, note, vel);
}

void gus_control(int chn, int control, int value)
{
	gus_midi_control(gusdevice, chn, control, value);
}

void gus_notepressure(int chn, int note, int vel)
{
	gus_midi_note_pressure(gusdevice, chn, note, vel);
}

void gus_channelpressure(int chn, int vel)
{
	gus_midi_channel_pressure(gusdevice, chn, vel);
}

void gus_bender(int chn, int pitch)
{
	gus_midi_bender(gusdevice, chn, pitch);
}

void gus_program(int chn, int pgm)
{
	/* FIXME: This isn't nice, but it works :) */
	gus_midi_timer_stop();
	gus_midi_preload_program(gusdevice,&pgm,1);
	gus_midi_timer_continue();
	gus_midi_program_change(gusdevice, chn, pgm);
}

bool gus_setmode(Emumode new_mode)
{
	int emulation;
        switch(new_mode) {
        case EMUMODE_MT32:
		emulation=GUS_MIDI_EMUL_MT32;
		break;
	case EMUMODE_GM:
		emulation=GUS_MIDI_EMUL_GM;
		break;
	default:
		if (warning)
			fprintf(stderr,"Warning: unknown emulation mode (%i)\n",new_mode);
		return FALSE;
        }
        return(gus_midi_emulation_set(gusdevice,emulation)!=-1);
}

void register_gus(Device * dev)
{
	dev->name = "UltraDriver";
	dev->version = 100;
	dev->detect = gus_detect;
	dev->init = gus_init;
	dev->done = gus_done;
	dev->flush = gus_flush;
	dev->noteon = gus_noteon;
	dev->noteoff = gus_noteoff;
	dev->control = gus_control;
	dev->notepressure = gus_notepressure;
	dev->channelpressure = gus_channelpressure;
	dev->bender = gus_bender;
	dev->program = gus_program;
	dev->setmode = gus_setmode;
}
