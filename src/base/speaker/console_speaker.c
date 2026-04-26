#include "speaker.h"
/*
 * Console Speaker Emulation
 * =============================================================================
 */

#include <sys/ioctl.h>
#ifdef __linux__
#include "Sys/kd.h"
#endif

#include "emu.h"

static void console_speaker_on(void *gp, unsigned ms, unsigned short period)
{
#ifdef __linux__
	ioctl((int)(uintptr_t)gp, KDMKTONE,
		(unsigned) ((ms & 0xffff) << 16) | (period & 0xffff));
#endif
}

static void console_speaker_off(void *gp)
{
#ifdef __linux__
	ioctl((int)(uintptr_t)gp, KDMKTONE, 0);
#endif
}

void console_speaker_init(void)
{
	if (config.speaker == SPKR_EMULATED && console_fd != -1)
		register_speaker((void *)(uintptr_t)console_fd,
				 console_speaker_on, console_speaker_off);
}
