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
 * Purpose: SDL sound output plugin.
 *
 * Author: Stas Sergeev.
 */

#include "emu.h"
#include "init.h"
#include "sound/sound.h"
#include <SDL.h>

static const char *sdlsnd_name = "Sound Output: SDL device";
static struct player_callbacks calls;
static struct player_params params;

static void sdlsnd_callback(void *userdata, Uint8 * stream, int len)
{
    calls.get_data(stream, len, &params);
}

static void sdlsnd_start(void)
{
    SDL_PauseAudio(0);
}

static void sdlsnd_stop(void)
{
    SDL_PauseAudio(1);
}

static int sdlsnd_open(void)
{
    SDL_AudioSpec spec, spec1;
    S_printf("Initializing SDL sound output\n");
    spec.freq = 44100;
    spec.format = AUDIO_S16LSB;
    spec.channels = 2;
    spec.samples = 1024;
    spec.callback = sdlsnd_callback;
    spec.userdata = NULL;
    if (SDL_OpenAudio(&spec, &spec1) < 0) {
	error("SDL sound init failed: %s\n", SDL_GetError());
	return 0;
    }

    params.rate = spec1.freq;
    params.format = PCM_FORMAT_S16_LE;
    params.channels = spec1.channels;
    return 1;
}

static void sdlsnd_close(void)
{
    SDL_CloseAudio();
}

CONSTRUCTOR(static int sdlsnd_init(void))
{
    struct clocked_player player;
    player.name = sdlsnd_name;
    player.start = sdlsnd_start;
    player.stop = sdlsnd_stop;
    player.open = sdlsnd_open;
    player.close = sdlsnd_close;
    player.lock = SDL_LockAudio;
    player.unlock = SDL_UnlockAudio;
    player.timer = NULL;
#if 1
    return pcm_register_clocked_player(player, &calls);
#else
    return 0;
#endif
}
