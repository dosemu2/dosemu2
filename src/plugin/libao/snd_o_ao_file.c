/*
 *  Copyright (C) 2015 Stas Sergeev <stsp@users.sourceforge.net>
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
 * Purpose: libao file writer output plugin.
 *
 * libao is so lame that we need a completely different handling
 * of the file writing interface.
 *
 * Author: Stas Sergeev.
 */

#include "emu.h"
#include "init.h"
#include "sound/sound.h"
#include "ao_init.h"
#include <stdio.h>
#include <ao/ao.h>

#define aosndf_name "Sound Output: libao wav writer"
static ao_device *ao;
static const char *ao_drv_manual_name = "wav";
static struct player_params params;
static int started;

static int aosndf_open(void *arg)
{
    ao_sample_format info = {};
    int id;

    ao_init();

    params.rate = 44100;
    params.format = PCM_FORMAT_S16_LE;
    params.channels = 2;
    info.channels = params.channels;
    info.rate = params.rate;
    info.byte_format = AO_FMT_LITTLE;
    info.bits = 16;
    id = ao_driver_id(ao_drv_manual_name);
    if (id == -1)
	return 0;
    ao = ao_open_file(id, config.wav_file, 1, &info, NULL);
    if (!ao) {
	error("libao: opening %s failed\n", config.wav_file);
	return 0;
    }

    pcm_setup_hpf(&params);

    return 1;
}

static void aosndf_close(void *arg)
{
    ao_close(ao);
}

static void aosndf_start(void *arg)
{
    started = 1;
}

static void aosndf_stop(void *arg)
{
    started = 0;
}

static void aosndf_timer(double dtime, void *arg)
{
    #define BUF_SIZE 1024
    char buf[BUF_SIZE];
    ssize_t size, size1, total;
    if (!started)
	return;
    total = pcm_frag_size(dtime, &params);
    if (total < BUF_SIZE)
	return;
    while (total) {
	size = total;
	if (size > BUF_SIZE)
	    size = BUF_SIZE;
	size1 = pcm_data_get(buf, size, &params);
	if (!size1)
	    break;
	ao_play(ao, buf, size1);
	if (size1 < size)
	    break;
	total -= size1;
    }
}

static int aosndf_get_cfg(void *arg)
{
    if (config.wav_file && config.wav_file[0])
	return PCM_CF_ENABLED;
    return 0;
}

static const struct pcm_player player = {
    .name = aosndf_name,
    .open = aosndf_open,
    .close = aosndf_close,
    .start = aosndf_start,
    .stop = aosndf_stop,
    .timer = aosndf_timer,
    .get_cfg = aosndf_get_cfg,
    .id = PCM_ID_P,
    .flags = PCM_F_PASSTHRU | PCM_F_EXPLICIT,
};

CONSTRUCTOR(static void aosndf_init(void))
{
    params.handle = pcm_register_player(&player, NULL);
}
