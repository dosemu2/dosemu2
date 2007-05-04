/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef SPEAKER_H
#define SPEAKER_H

#include "types.h"

/*
 * Caller speaker functions
 * ============================================================================
 */
void speaker_on(unsigned ms, unsigned short period);
void speaker_off(void);
void speaker_pause(void);
void speaker_resume(void);

/*
 * Time length converions
 * =============================================================================
 */
#define SPEAKER_PERIOD_BASE 1193180U
static inline unsigned speaker_period_to_Hz(unsigned short period)
{
	unsigned Hz;
	Hz = (period)? (SPEAKER_PERIOD_BASE/period): 18;
	return Hz;
}

static inline unsigned short speaker_Hz_to_period(unsigned Hz)
{
	unsigned short period;
	period = (Hz > 18)? (SPEAKER_PERIOD_BASE/Hz): 65535;
	return period;
}

/*
 *  Speaker registration
 * ============================================================================
 */
typedef void (*speaker_on_t)(void *gp, unsigned ms, unsigned short period);
typedef void (*speaker_off_t)(void *gp);

/* an invalid value of speaker_on || speaker_off resets the default speaker */
/* Each new sound event should override the last one */
void register_speaker(void *gp, 
			     speaker_on_t speaker_on, 
			     speaker_off_t speaker_off);

/*
 * Speaker emulation routines
 * =============================================================================
 */
/* for now declare these here */
void X_speaker_on(void *gp, unsigned ms, unsigned short period);
void X_speaker_off(void *gp);
void console_speaker_on(void *gp, unsigned ms, unsigned short period);
void console_speaker_off(void *gp);

/*
 * These are used by kbd code but reside in timers.c
 * =============================================================================
 */
Bit8u spkr_io_read(ioport_t port);
void spkr_io_write(ioport_t port, Bit8u value);

#endif /* SPEAKER_H */
