#ifdef __linux__
#include "speaker.h"
/*
 * evdev speaker Emulation
 * =============================================================================
 */

#include "linux/input.h"
#include "emu.h"

static void evdev_speaker_on(void *gp, unsigned short period)
{
	struct input_event e = {.type = EV_SND, .code = SND_TONE,
				.value = speaker_period_to_Hz(period)};
	write((int)(uintptr_t)gp, &e, sizeof e);
}

static void evdev_speaker_off(void *gp)
{
	struct input_event e = {.type = EV_SND, .code = SND_TONE,
				.value = 0};
	write((int)(uintptr_t)gp, &e, sizeof e);
}

void evdev_speaker_init(void)
{
	if (config.speaker == SPKR_EMULATED && console_fd == -1) {
		int fd = open("/dev/input/by-path/platform-pcspkr-event-spkr",
			      O_WRONLY);
		if (fd != -1)
			register_speaker((void *)(uintptr_t)fd,
					 evdev_speaker_on, evdev_speaker_off);
	}
}

#else

void evdev_speaker_init(void)
{
}

#endif
