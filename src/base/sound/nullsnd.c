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
 * Purpose: Dummy sound plugin. Doesn't do any sound I/O, but
 * provides the sound system with the timing.
 *
 * Author: Stas Sergeev.
 */

#include "emu.h"
#include "timers.h"
#include "sound/sound.h"
#include "sound/sndpcm.h"
#include "nullsnd.h"

static const char *nullsnd_name = "Sound Output: NULL device";
const int frag_size = 4096;
static struct player_params params;
static struct player_callbacks calls;
static int running, locked;
static double last_time = 0;

static void nullsnd_start(void)
{
    last_time = GETusTIME(0);
    running = 1;
}

static void nullsnd_stop(void)
{
    running = 0;
}

static int nullsnd_open(void)
{
    params.rate = 44100;
    params.format = PCM_FORMAT_S16_LE;
    params.channels = 2;
    return 1;
}

static void nullsnd_close(void)
{
}

static void nullsnd_lock(void)
{
    locked++;
}

static void nullsnd_unlock(void)
{
    locked--;
}

static void nullsnd_timer(void)
{
    double time, frag_time;
    if (!running || locked)
	return;
    frag_time = pcm_frag_period(frag_size, &params);
    time = GETusTIME(0);
    while (time - last_time > frag_time) {
	last_time += frag_time;
	calls.get_data(NULL, frag_size, &params);
    }
}

int nullsnd_init(void)
{
    struct clocked_player player;
    player.name = nullsnd_name;
    player.start = nullsnd_start;
    player.stop = nullsnd_stop;
    player.open = nullsnd_open;
    player.close = nullsnd_close;
    player.lock = nullsnd_lock;
    player.unlock = nullsnd_unlock;
    player.timer = nullsnd_timer;
    running = locked = 0;
    return pcm_register_clocked_player(player, &calls);
}
