#include "speaker.h"
/*
 * Console Speaker Emulation
 * =============================================================================
 */

#include <sys/ioctl.h>
#ifdef __linux__
#include <sys/kd.h>
#endif


void console_speaker_on(void *gp, unsigned ms, unsigned short period)
{
#ifdef __linux__
	ioctl((int)(uintptr_t)gp, KDMKTONE,
		(unsigned) ((ms & 0xffff) << 16) | (period & 0xffff));
#endif
}

void console_speaker_off(void *gp)
{
#ifdef __linux__
	ioctl((int)(uintptr_t)gp, KDMKTONE, 0);
#endif
}
