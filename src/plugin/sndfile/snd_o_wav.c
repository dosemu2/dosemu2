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

/*
 * Purpose: WAV file sound output plugin.
 *
 * Author: Stas Sergeev.
 */

#include "emu.h"
#include "init.h"
#include "sound/sound.h"
#include "sound/sndpcm.h"
#include <stdio.h>
#include <sndfile.h>

static const char *wavsnd_name = "Sound Output: WAV writer";
static SNDFILE *wav;

static int wavsnd_open(struct player_params *par)
{
    SF_INFO info;
    par->rate = 44100;
    par->format = PCM_FORMAT_S16_LE;
    par->channels = 2;
    info.samplerate = par->rate;
    info.channels = par->channels;
    info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    wav = sf_open("/tmp/a.wav", SFM_WRITE, &info);
    return !!wav;
}

static void wavsnd_close(void)
{
    sf_close(wav);
}

static size_t wavsnd_write(void *data, size_t size)
{
    return sf_write_raw(wav, data, size);
}

CONSTRUCTOR(static int wavsnd_init(void))
{
    struct unclocked_player player;
    player.name = wavsnd_name;
    player.open = wavsnd_open;
    player.close = wavsnd_close;
    player.write = wavsnd_write;
    player.timer = NULL;
#if 0
    return pcm_register_unclocked_player(player);
#else
    return 0;
#endif
}
