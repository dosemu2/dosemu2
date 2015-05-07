/*
 *  Copyright (C) 2006 Stas Sergeev <stsp@users.sourceforge.net>
 *
 * The below copyright strings have to be distributed unchanged together
 * with this file. This prefix can not be modified or separated.
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "emu.h"
#include "init.h"
#include "sound/midi.h"
#include <alsa/asoundlib.h>


static snd_rawmidi_t *handle = NULL;
#define midoalsa_name "MIDI Output: ALSA device"
static const char *device = "default";

static int midoalsa_init(void *arg)
{
    int err;
    err = snd_rawmidi_open(NULL, &handle, device,
			   SND_RAWMIDI_NONBLOCK | SND_RAWMIDI_SYNC);
    if (err) {
	S_printf("%s: unable to open %s for writing: %s\n",
		 midoalsa_name, device, snd_strerror(err));
	return 0;
    }
    return 1;
}

static void midoalsa_done(void *arg)
{
    if (!handle)
	return;
    snd_rawmidi_close(handle);
    handle = NULL;
}

static void midoalsa_write(unsigned char val)
{
    if (!handle)
	return;
    snd_rawmidi_write(handle, &val, 1);
}

static const struct midi_out_plugin midoalsa = {
    .name = midoalsa_name,
    .open = midoalsa_init,
    .close = midoalsa_done,
    .write = midoalsa_write,
    .weight = MIDI_W_PREFERRED,
};

CONSTRUCTOR(static int midoalsa_register(void))
{
    return midi_register_output_plugin(&midoalsa);
}
