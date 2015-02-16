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
 * Purpose: libao sound output plugin.
 *
 * Author: Stas Sergeev.
 */

#include "emu.h"
#include "init.h"
#include "ringbuf.h"
#include "sound/sound.h"
#include <stdio.h>
#include <ao/ao.h>
#include <pthread.h>

static const char *aosnd_name = "Sound Output: libao";
static ao_device *ao;
static struct player_params params;
static int started;
#define SND_BUF_SZ (1*44100*2*2)
static struct rng_s sndbuf;
static pthread_mutex_t buf_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t data_cnd = PTHREAD_COND_INITIALIZER;
static pthread_t write_thr;
static void *aosnd_write(void *arg);

static int aosnd_open(void *arg)
{
    ao_sample_format info = {};
    int id;
    params.rate = 44100;
    params.format = PCM_FORMAT_S16_LE;
    params.channels = 2;
    info.channels = params.channels;
    info.rate = params.rate;
    info.byte_format = AO_FMT_LITTLE;
    info.bits = 16;
    ao_initialize();
    id = ao_default_driver_id();
    if (id == -1) {
	ao_shutdown();
	return 0;
    }
    ao = ao_open_live(id, &info, NULL);
    if (!ao) {
	ao_shutdown();
	return 0;
    }
    rng_init(&sndbuf, SND_BUF_SZ, 1);
    pthread_create(&write_thr, NULL, aosnd_write, NULL);
    return 1;
}

static void aosnd_close(void *arg)
{
    pthread_cancel(write_thr);
    pthread_join(write_thr, NULL);
    ao_close(ao);
    ao_shutdown();
    rng_destroy(&sndbuf);
}

static void *aosnd_write(void *arg)
{
    #define BUF_SIZE 4096
    char buf[BUF_SIZE];
    uint32_t size;
    while (1) {
	pthread_mutex_lock(&buf_mtx);
	while (1) {
	    size = rng_count(&sndbuf);
	    if (!size)
		pthread_cond_wait(&data_cnd, &buf_mtx);
	    else
		break;
	}
	if (size > BUF_SIZE)
	    size = BUF_SIZE;
	rng_remove(&sndbuf, size, buf);
	pthread_mutex_unlock(&buf_mtx);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	ao_play(ao, buf, size);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_testcancel();
    }
    return NULL;
}

static void aosnd_start(void *arg)
{
    started = 1;
}

static void aosnd_stop(void *arg)
{
    started = 0;
}

static void aosnd_timer(double dtime, void *arg)
{
    #define BUF2_SIZE 1024
    char buf[BUF2_SIZE];
    ssize_t size, total;
    if (!started)
	return;
    total = pcm_frag_size(dtime, &params);
    if (total < BUF2_SIZE)
	return;
    while (total) {
	size = total;
	if (size > BUF2_SIZE)
	    size = BUF2_SIZE;
	size = pcm_data_get(buf, size, &params);
	if (!size)
	    break;
	pthread_mutex_lock(&buf_mtx);
	rng_add(&sndbuf, size, buf);
	pthread_mutex_unlock(&buf_mtx);
	pthread_cond_signal(&data_cnd);
	total -= size;
    }
}

CONSTRUCTOR(static void aosnd_init(void))
{
    struct pcm_player player = {};
    player.name = aosnd_name;
    player.open = aosnd_open;
    player.close = aosnd_close;
    player.start = aosnd_start;
    player.stop = aosnd_stop;
    player.timer = aosnd_timer;
    player.id = PCM_ID_P;
    params.handle = pcm_register_player(player);
}
