/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "config.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include "speaker.h"

/*
 * X speaker emulation
 * =============================================================================
 */
void X_speaker_on(void *gp, unsigned ms, unsigned short period)
{
	XKeyboardControl new_state;
	Display *display = gp;

#if 0
	/* Turn off the previos sound, I hope */
	new_state.bell_pitch = 0;  
	new_state.bell_duration = 0;
	XChangeKeyboardControl(display, KBBellDuration | KBBellPitch, &new_state);
	XBell(display, -100);
#endif

	/* Make the sound I want */
	new_state.bell_pitch = speaker_period_to_Hz(period);
	/* Prohibit a crashing X server by not allowing to go higher than
	 * 32767 kHz. Since the X server interpretes the pitch as a short int
	 */
	if (new_state.bell_pitch >= 0x8000) new_state.bell_pitch = 0x7fff;
	new_state.bell_duration = ms;
	XChangeKeyboardControl(display, KBBellDuration | KBBellPitch, &new_state);
	
	/* Reset the sound defaults */
	XBell(display, 100); /* I'm not sure how to get the default here so make it loud. */
	new_state.bell_pitch = -1;  /* reset the sound defaults */
	new_state.bell_duration = -1;
	XChangeKeyboardControl(display, KBBellDuration | KBBellPitch, &new_state);
}

void X_speaker_off(void*gp)
{
	/* Just make a zero length sound */
	X_speaker_on(gp, 0,0);
}

