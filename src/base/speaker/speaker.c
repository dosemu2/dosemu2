/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * Speaker emulation code, file speaker.c
 *
 * (C) 1997 under GPL or LGPL, Eric Biederman <ebiederm+eric@nwpt.net>
 *
 * DANG_BEGIN_MODULE
 *
 * REMARK
 *
 * The pc-speaker emulator for Dosemu.
 *
 * This file contains functions to make a pc speaker beep for dosemu.  
 *
 * Actuall emulation is done in src/base/dev/misc/timers.c in do_sound.
 *
 * Currenly emulation is only done when the new keyboard is enabled but with a
 * little extra work it should be possible to enable it for the old keyboard
 * code if necessary. 
 *
 * For parts of dosemu that want to beep the pc-speaker (say the video bios)
 * #include "speaker.h"
 * Use 'speaker_on(ms, period)'
 * to turn the pc-speaker on for 'ms' milliseconds with period 'period'.
 * The function returns immediately.
 *
 * Use 'speaker_off()'
 * To turn the pc-speaker definentily off.  This is mostly useful when exiting
 * the program to ensure you aren't killy someones ears :)
 *
 * 'speaker_on' always overrides whatever previous speaker sound was previously
 * given.  No mixing happens.
 *
 * For code that wants to implement speaker emulation.  The recommeded method
 * is to add a file in src/base/speaker with the necessary code.  Declare it's
 * methods in speaker.h (or somewhere accessible to your code).  And call
 *
 * 'register_speaker(gp, on, off)'
 * when your speaker code is ready to function.
 *
 *  gp may be any void pointer value.
 *
 *  gp is passed as the first argument to the functions arguments 'on'
 *  and 'off' when global functions 'speaker_on' and 'speaker_off' are called.
 *  This allows important state information to be passed to the functions.  And
 *  reduces reliance on global variables.  
 *
 * The functions 'on' and 'off' besides the extra parameter are called just as
 * the global functions 'speaker_on' and 'speaker_off' are called respectively.
 * 
 * Before the registered function is no longer valid call 
 * 'register_speaker(NULL, NULL, NULL)' this will reset the speaker code
 * to it's default speaker functions, which will always work.
 *
 * --EB 20 Sept 1997
 *
 * /REMARK
 * maintainer: 
 * Eric W. Biederman <eric@biederman.org>
 * DANG_END_MODULE
 *
 * Changes:	Hans 970926 (at time of patch inclusion)
 *		- Reduced type/prototyping usage to DOSEMU common one ;-)
 *			
 */

#include "emu.h"
#include "speaker.h"
#include "port.h"
#include "iodev.h"
#include "timers.h"

/*
 * Speaker info structure.
 * ============================================================================
 */
struct speaker_info {
	void *gp;  /* a general pointer it can hold anything, 
		    * it is passed to both speaker_on & speaker_off 
		    */
	speaker_on_t on;
	speaker_off_t off;
};

/*
 * Generic speaker emulation
 * =============================================================================
 */
#include <stdio.h> /* for putchar */
#include <unistd.h>

static void dumb_speaker_on(void * gp, unsigned ms, unsigned short period)
{
	putchar('\007');
}
static void dumb_speaker_off(void *gp)
{
	/* we can't :( */
}

static struct speaker_info dumb_speaker = 
{ NULL, dumb_speaker_on, dumb_speaker_off };

/*
 * Speaker Emulation Control
 * =============================================================================
 */


static struct speaker_info speaker = 
{ NULL, dumb_speaker_on, dumb_speaker_off};

void register_speaker(void *gp, 
			     speaker_on_t speaker_on, 
			     speaker_off_t speaker_off)
{
	if (speaker_on && speaker_off) {
		speaker.gp = gp;
		speaker.on = speaker_on;
		speaker.off = speaker_off;
	} else {
		speaker = dumb_speaker;
	}
}

/* this does the EMULATED mode speaker emulation */
void speaker_on(unsigned ms, unsigned short period) 
{
	if (config.speaker == SPKR_OFF)
		return;
	i_printf("SPEAKER: on, period=%d\n", period);
	if (!speaker.on) {
		speaker = dumb_speaker;
	}
	speaker.on(speaker.gp, ms, period);
}

void speaker_off(void)
{
	i_printf("SPEAKER: sound OFF!\n");
	if (!speaker.off) {
		speaker = dumb_speaker;
	}
	speaker.off(speaker.gp);
}

static int saved_port_val;
void speaker_pause (void)
{
	switch (config.speaker)
	{
	case SPKR_NATIVE:
		saved_port_val = port_safe_inb (0x61);
	 	port_safe_outb (0x61, saved_port_val & 0xFC);	/* clear timer & speaker bits */
		break;
	case SPKR_EMULATED:
		speaker_off ();
		break;
	case SPKR_OFF:
		break;
	}
}

void speaker_resume (void)
{
	switch (config.speaker)
	{
	case SPKR_NATIVE:
		port_safe_outb (0x61, saved_port_val);	/* restore timer & speaker bits */
		break;
	case SPKR_EMULATED:
		do_sound (pit [2].write_latch & 0xffff);
		break;
	case SPKR_OFF:
		break;
	}
}
