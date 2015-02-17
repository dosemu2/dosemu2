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
 * Purpose: libao sound output interface to "live" targets (pulseaudio).
 *
 * File-based targets are handled elsewhere.
 * libao only pretends to provide the unification. The fact is, the
 * entirely different code is needed to handle the file-based targets.
 *
 * Author: Stas Sergeev.
 */

#include "emu.h"
#include "init.h"
#include "sound/sound.h"
#include <stdio.h>
#include <unistd.h>
#include <ao/ao.h>
#include <pthread.h>
#include <semaphore.h>

static const char *aosnd_name = "Sound Output: libao";
static ao_device *ao;
static struct player_params params;
static int started;
static pthread_mutex_t start_mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t start_sem;
static int stopped;
static pthread_mutex_t stop_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t stop_cnd = PTHREAD_COND_INITIALIZER;
static pthread_t write_thr;
/* libao is not only lame but also its pulseaudio backend produces clicks.
 * So we manually select alsa driver, which gets re-routed to pulseaudio
 * if your /etc/asound.conf specifies that (usually the case). */
static const int ao_drv_manual = 1;
static const char *ao_drv_manual_name = "alsa";
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
    id = ao_drv_manual ? ao_driver_id(ao_drv_manual_name) :
	    ao_default_driver_id();
    if (id == -1) {
	ao_shutdown();
	return 0;
    }
    ao = ao_open_live(id, &info, NULL);
    if (!ao) {
	ao_shutdown();
	return 0;
    }
    sem_init(&start_sem, 0, 0);
    pthread_create(&write_thr, NULL, aosnd_write, NULL);
    return 1;
}

static void aosnd_close(void *arg)
{
    pthread_cancel(write_thr);
    pthread_join(write_thr, NULL);
    sem_destroy(&start_sem);
    ao_close(ao);
    ao_shutdown();
}

static void *aosnd_write(void *arg)
{
    #define BUF_SIZE 4096
    char buf[BUF_SIZE];
    uint32_t size;
    int l_started;
    while (1) {
	while (1) {
	    pthread_mutex_lock(&start_mtx);
	    l_started = started;
	    pthread_mutex_unlock(&start_mtx);
	    if (!l_started) {
		pthread_mutex_lock(&stop_mtx);
		stopped = 1;
		pthread_cond_signal(&stop_cnd);
		pthread_mutex_unlock(&stop_mtx);
		sem_wait(&start_sem);
	    } else {
		pthread_mutex_lock(&stop_mtx);
		stopped = 0;
		pthread_mutex_unlock(&stop_mtx);
		break;
	    }
	}
	/* if we are here, stop() will wait for us on stop_cnd */
	size = pcm_data_get(buf, sizeof(buf), &params);
	if (!size) {
	    usleep(10000);
	    continue;
	}
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	ao_play(ao, buf, size);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_testcancel();
    }
    return NULL;
}

static void aosnd_start(void *arg)
{
    pthread_mutex_lock(&start_mtx);
    started = 1;
    pthread_mutex_unlock(&start_mtx);
    sem_post(&start_sem);
}

static void aosnd_stop(void *arg)
{
    pthread_mutex_lock(&start_mtx);
    started = 0;
    pthread_mutex_unlock(&start_mtx);
    pthread_mutex_lock(&stop_mtx);
    while (!stopped)
	pthread_cond_wait(&stop_cnd, &stop_mtx);
    pthread_mutex_unlock(&stop_mtx);
}

CONSTRUCTOR(static void aosnd_init(void))
{
    struct pcm_player player = {};
    player.name = aosnd_name;
    player.open = aosnd_open;
    player.close = aosnd_close;
    player.start = aosnd_start;
    player.stop = aosnd_stop;
    player.timer = NULL;
    player.id = PCM_ID_P;
    params.handle = pcm_register_player(player);
}
