#ifndef SPEAKER_H
#define SPEAKER_H

#include "types.h"

/*
 * Caller speaker functions
 * ============================================================================
 */
void speaker_on(unsigned short period);
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
typedef void (*speaker_on_t)(void *gp, unsigned short period);
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
void console_speaker_init(void);
void evdev_speaker_init(void);
void sound_speaker_init(void);
void sound_speaker_done(void);

void speaker_init(void);
void speaker_done(void);

#endif /* SPEAKER_H */
