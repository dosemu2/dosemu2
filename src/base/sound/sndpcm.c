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
 * Purpose: Sound midlayer. Resampling, converting and all the other hard work.
 *
 * Author: Stas Sergeev.
 *
 * TODO: Add the ADPCM processing.
 * TODO: Add the PDM processing for PC-Speaker.
 */

#include "emu.h"
#include "utilities.h"
#include "ringbuf.h"
#include "timers.h"
#include "sound/sound.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>


#define pcm_printf(...) do { \
    if (debug_level('S') >= 9) S_printf(__VA_ARGS__); \
} while (0)
#define SND_BUFFER_SIZE 100000	/* enough to hold 1.1s of 44100/stereo */
#define BUFFER_DELAY 40000.0

#define MIN_BUFFER_DELAY (BUFFER_DELAY)
#define NORM_BUFFER_DELAY (BUFFER_DELAY * 2)
#define INIT_BUFFER_DELAY (BUFFER_DELAY * 4)
#define MAX_BUFFER_DELAY (BUFFER_DELAY * 10)
#define MAX_BUFFER_PERIOD (MAX_BUFFER_DELAY - MIN_BUFFER_DELAY)
#define MIN_GUARD_SIZE 1024
#define MIN_READ_GUARD_PERIOD (1000000 * MIN_GUARD_SIZE / (2 * 44100))
#define WR_BUFFER_LW (BUFFER_DELAY / 2)
#define MIN_READ_DELAY (MIN_BUFFER_DELAY + MIN_READ_GUARD_PERIOD)
#define WRITE_AREA_SIZE MIN_BUFFER_DELAY
#define WRITE_INIT_POS (WRITE_AREA_SIZE / 2)

/*    Layout of our buffer is as follows:
 *
 *                  |              |
 * WRITE_INIT_POS ->+  write area  +<---------------------- <-WR_BUFFER_LW (GC)
 *                  |              |                       \
 * WRITE_AREA_SIZE->+--------------+->MIN_BUFFER_DELAY     |
 *                  |              |                       |
 *                  +  read area   +->INIT_BUFFER_DELAY    |
 *                  |              |                       |
 *                  +--------------+->MAX_BUFFER_DELAY     |
 *                  |              |                       |
 *                  |   GC area    +---------------------->
 *                  \_____________/
 */

enum {
    SNDBUF_STATE_INACTIVE,
    SNDBUF_STATE_PLAYING,
    SNDBUF_STATE_FLUSHING,
    SNDBUF_STATE_STALLED,
};

struct sample {
    int format;
    double tstamp;
    unsigned char data[2];
};

static const struct sample mute_samp = { PCM_FORMAT_NONE, 0, {0, 0} };

struct stream {
    int channels;
    struct rng_s buffer;
    /* buf_cnt is a flat counter, never decrements. We have to use
     * something really "long" for it, because "int" can overflow in
     * about 6.7 hours of playing stereo sound at rate 44100.
     * Surprisingly @runderwoo have actually hit such overflow when
     * buf_cnt was "int". Lets use "long long". */
    long long buf_cnt;
    int state;
    int flags;
    int stretch:1;
    int prepared:1;
    void *vol_arg;
    double start_time;
    double stop_time;
    double stretch_per;
    double stretch_tot;
    /* for the raw channels heuristic */
    double raw_speed_adj;
    double last_adj_time;
    double adj_time_delay;
    double last_fillup;
    /* --- */
    char *name;
};

#define MAX_STREAMS 10
#define MAX_PLAYERS 10
#define MAX_RECORDERS 10
#define MAX_EFPS 5
#define MAX_EFP_LINKS 5

struct efp_link {
    int handle;
    struct pcm_holder *efp;
};

struct pcm_player_wr {
    double time;
    long long last_cnt[MAX_STREAMS];
    int last_idx[MAX_STREAMS];
    double last_tstamp[MAX_STREAMS];
    struct efp_link efpl[MAX_EFP_LINKS];
    int num_efp_links;
};


#define HPF_CTL 10

struct efp_wr {
    enum EfpType type;
};

#define PLAYER(p) ((struct pcm_player *)p->plugin)
#define PL_PRIV(p) ((struct pcm_player_wr *)p->priv)
#define PL_LNAME(p) (p->longname ?: p->name)
#define RECORDER(p) ((struct pcm_recorder *)p->plugin)
#define EFPR(p) ((struct pcm_efp *)p->plugin)
#define EF_PRIV(p) ((struct efp_wr *)p->priv)

struct pcm_struct {
    struct stream stream[MAX_STREAMS];
    int num_streams;
    double (*get_volume)(int id, int chan_dst, int chan_src, void *);
    int (*is_connected)(int id, void *arg);
    int (*checkid2)(void *id2, void *arg);
    pthread_mutex_t strm_mtx;
    pthread_mutex_t time_mtx;
    struct pcm_holder players[MAX_PLAYERS];
    int num_players;
    int playing;
    struct pcm_holder recorders[MAX_RECORDERS];
    int num_recorders;
    struct pcm_holder efps[MAX_EFPS];
    int num_efps;
    double time;
};
static struct pcm_struct pcm;

#define MAX_DL_HANDLES 10
static void *dl_handles[MAX_DL_HANDLES];
static int num_dl_handles;

static double get_vol_dummy(int id, int chan_dst, int chan_src, void *arg)
{
    return (chan_src == chan_dst ? 1.0 : 0.0);
}

static int is_connected_dummy(int id, void *arg)
{
    return 1;
}

static int checkid2_dummy(void *id2, void *arg)
{
    return 1;
}

static int pcm_get_cfg(const char *name)
{
  int i;
  for (i = 0; i < pcm.num_players; i++) {
    struct pcm_holder *p = &pcm.players[i];
    if (!strcmp(p->plugin->name, name))
      return (p->plugin->get_cfg ? p->plugin->get_cfg(p->arg) : 0);
  }
  return -1;
}

int pcm_init(void)
{
#ifdef USE_DL_PLUGINS
    int ca = 0, cs = 0;
#endif
    pcm_printf("PCM: init\n");
    pthread_mutex_init(&pcm.strm_mtx, NULL);
    pthread_mutex_init(&pcm.time_mtx, NULL);

#ifdef USE_DL_PLUGINS
#define LOAD_PLUGIN_C(x, c) \
    dl_handles[num_dl_handles] = load_plugin(x); \
    if (dl_handles[num_dl_handles]) { \
	num_dl_handles++; \
	c \
    }
#define LOAD_PLUGIN(x) LOAD_PLUGIN_C(x,)
#ifdef USE_LIBAO
    LOAD_PLUGIN_C("libao", { ca = pcm_get_cfg("ao"); } );
#endif
#ifdef SDL_SUPPORT
    LOAD_PLUGIN_C("sdl", { cs = pcm_get_cfg("sdl"); } );
#endif
    if (!ca && !cs)		// auto. use ao for now
	config.libao_sound = 1;
    else if (ca == PCM_CF_ENABLED)
	config.libao_sound = 1;
    else if (cs == PCM_CF_ENABLED)
	config.sdl_sound = 1;
    else if (cs != ca) {
	if (!ca)
	    config.libao_sound = 1;
	else
	    config.sdl_sound = 1;
    }

#ifdef USE_ALSA
    LOAD_PLUGIN("alsa");
#endif
#ifdef LADSPA_SUPPORT
    LOAD_PLUGIN("ladspa");
#endif
#endif
    assert(num_dl_handles <= MAX_DL_HANDLES);

    pcm.get_volume = get_vol_dummy;
    pcm.is_connected = is_connected_dummy;
    pcm.checkid2 = checkid2_dummy;

    /* init efps before players because players init code refers to efps */
    if (!pcm_init_plugins(pcm.efps, pcm.num_efps))
      pcm_printf("no PCM effect processors initialized\n");
    if (!pcm_init_plugins(pcm.players, pcm.num_players))
      pcm_printf("ERROR: no PCM output plugins initialized\n");
    if (!pcm_init_plugins(pcm.recorders, pcm.num_recorders))
      pcm_printf("ERROR: no PCM input plugins initialized\n");
    return 1;
}

static void pcm_clear_stream(int strm_idx)
{
    pcm.stream[strm_idx].buf_cnt += rng_count(&pcm.stream[strm_idx].buffer);
    rng_clear(&pcm.stream[strm_idx].buffer);
}

static void pcm_reset_stream(int strm_idx)
{
    pcm_clear_stream(strm_idx);
    pcm.stream[strm_idx].state = SNDBUF_STATE_INACTIVE;
    pcm.stream[strm_idx].stretch = 0;
    pcm.stream[strm_idx].stretch_per = 0;
    pcm.stream[strm_idx].stretch_tot = 0;
    pcm.stream[strm_idx].prepared = 0;
}

int pcm_allocate_stream(int channels, char *name, void *vol_arg)
{
    int index;
    if (pcm.num_streams >= MAX_STREAMS) {
	error("PCM: stream pool exhausted, max=%i\n", MAX_STREAMS);
	return -1;
    }
    pthread_mutex_lock(&pcm.strm_mtx);
    index = pcm.num_streams++;
    rng_init(&pcm.stream[index].buffer, SND_BUFFER_SIZE,
	     sizeof(struct sample));
    /* to keep timestamps contiguous, we disable overwrites */
    rng_allow_ovw(&pcm.stream[index].buffer, 0);
    pcm.stream[index].channels = channels;
    pcm.stream[index].name = name;
    pcm.stream[index].buf_cnt = 0;
    pcm.stream[index].vol_arg = vol_arg;
    pcm_reset_stream(index);
    pthread_mutex_unlock(&pcm.strm_mtx);
    pcm_printf("PCM: Stream %i allocated for \"%s\"\n", index, name);
    return index;
}

void pcm_set_flag(int strm_idx, int flag)
{
    if (pcm.stream[strm_idx].flags & flag)
	return;
    pcm_printf("PCM: setting flag %x for stream %i (%s)\n",
	     flag, strm_idx, pcm.stream[strm_idx].name);
    pcm.stream[strm_idx].flags |= flag;
    if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW)
	pcm.stream[strm_idx].raw_speed_adj = 1.0;
}

void pcm_clear_flag(int strm_idx, int flag)
{
    if (!(pcm.stream[strm_idx].flags & flag))
	return;
    pcm_printf("PCM: clearing flag %x for stream %i (%s)\n",
	     flag, strm_idx, pcm.stream[strm_idx].name);
    pcm.stream[strm_idx].flags &= ~flag;
}

int pcm_format_size(int format)
{
    switch (format) {
    case PCM_FORMAT_U8:
    case PCM_FORMAT_S8:
	return 1;
    case PCM_FORMAT_U16_LE:
    case PCM_FORMAT_S16_LE:
	return 2;
    default:
	error("PCM: format %i is not supported\n", format);
	return 0;
    }
}

static int pcm_get_format8(int is_signed)
{
    return is_signed ? PCM_FORMAT_S8 : PCM_FORMAT_U8;
}

static int pcm_get_format16(int is_signed)
{
    return is_signed ? PCM_FORMAT_S16_LE : PCM_FORMAT_U16_LE;
}

int pcm_get_format(int is_16, int is_signed)
{
    return is_16 ? pcm_get_format16(is_signed) :
	pcm_get_format8(is_signed);
}

static int cutoff(int val, int min, int max)
{
    if (val < min)
	return min;
    if (val > max)
	return max;
    return val;
}

int pcm_samp_cutoff(int val, int format)
{
    switch (format) {
    case PCM_FORMAT_U8:
	return cutoff(val, 0, UCHAR_MAX);
    case PCM_FORMAT_S8:
	return cutoff(val, SCHAR_MIN, SCHAR_MAX);
    case PCM_FORMAT_U16_LE:
	return cutoff(val, 0, USHRT_MAX);
    case PCM_FORMAT_S16_LE:
	return cutoff(val, SHRT_MIN, SHRT_MAX);
    default:
	error("PCM: format %i is not supported\n", format);
	return 0;
    }
}

#define UC2SS(v) ((*(unsigned char *)(v) - 128) * 256)
#define SC2SS(v) (*(signed char *)(v) * 256)
#define US2SS(v) (*(unsigned short *)(v) - 32768)
#define SS2UC(v) ((unsigned char)(((v) + 32768) / 256))
#define SS2SC(v) ((signed char)((v) / 256))
#define SS2US(v) ((unsigned short)((v) + 32768))

static short sample_to_S16(void *data, int format)
{
    switch (format) {
    case PCM_FORMAT_U8:
	return UC2SS(data);
    case PCM_FORMAT_S8:
	return SC2SS(data);
    case PCM_FORMAT_U16_LE:
	return US2SS(data);
    case PCM_FORMAT_S16_LE:
	return *(short *) data;
    default:
	error("PCM: format %i is not supported\n", format);
	return 0;
    }
}

static void S16_to_sample(short sample, sndbuf_t *buf, int format)
{
    switch (format) {
    case PCM_FORMAT_U8:
	*buf = SS2UC(sample);
	break;
    case PCM_FORMAT_S8:
	*buf = SS2SC(sample);
	break;
    case PCM_FORMAT_U16_LE:
	*buf = SS2US(sample);
	break;
    case PCM_FORMAT_S16_LE:
	*buf = sample;
	break;
    default:
	error("PCM: format1 %i is not supported\n", format);
    }
}

static int count_active_streams(int id)
{
    int i, ret = 0;
    for (i = 0; i < pcm.num_streams; i++) {
	if (id != PCM_ID_ANY && !pcm.is_connected(id, pcm.stream[i].vol_arg))
	    continue;
	if (pcm.stream[i].state == SNDBUF_STATE_INACTIVE)
	    continue;
	ret++;
    }
    return ret;
}

double pcm_frame_period_us(int rate)
{
    return (1000000.0 / rate);
}

double pcm_frag_period(int size, struct player_params *params)
{
    int samp_sz, nsamps;
    samp_sz = pcm_format_size(params->format);
    nsamps = size / samp_sz;
    return nsamps * pcm_frame_period_us(params->rate) / params->channels;
}

int pcm_frag_size(double period, struct player_params *params)
{
    int nframes = period / pcm_frame_period_us(params->rate);
    int nsamps = nframes * params->channels;
    return nsamps * pcm_format_size(params->format);
}

static int peek_last_sample(int strm_idx, struct sample *samp)
{
    int idx = rng_count(&pcm.stream[strm_idx].buffer);
    if (!idx)
	return 0;
    return rng_peek(&pcm.stream[strm_idx].buffer, idx - 1, samp);
}

void pcm_prepare_stream(int strm_idx)
{
    long long now = GETusTIME(0);
    struct stream *s;

    s = &pcm.stream[strm_idx];
    switch (s->state) {

    case SNDBUF_STATE_PLAYING:
	error("PCM: prepare playing stream %s\n", s->name);
	return;

    case SNDBUF_STATE_STALLED:
	error("PCM: prepare stalled stream %s\n", s->name);
	/* should never happen, but if we are here we reset stretches */
	pthread_mutex_lock(&pcm.strm_mtx);
	pcm_reset_stream(strm_idx);
	pthread_mutex_unlock(&pcm.strm_mtx);
	break;

    case SNDBUF_STATE_FLUSHING:
	/* very careful: because of poor syncing the stupid things happen.
	 * Like, for instance, samples written in the future... */
	if (now < s->stop_time) {
	    pcm_printf("PCM: ERROR: sample in the future, %f now=%llu, %s\n",
		    s->stop_time, now, s->name);
	    now = s->stop_time;
	}
	break;
    }

    s->start_time = now;
    s->prepared = 1;
}

static void pcm_stream_stretch(int strm_idx)
{
    long long now = GETusTIME(0);
    struct stream *s = &pcm.stream[strm_idx];
    assert(s->state != SNDBUF_STATE_PLAYING &&
	    s->state != SNDBUF_STATE_INACTIVE);
    s->start_time = now;
    s->stretch = 1;
}

static double calc_buffer_fillup(int strm_idx, double time)
{
    struct stream *s = &pcm.stream[strm_idx];
    if (s->state != SNDBUF_STATE_PLAYING && s->state != SNDBUF_STATE_FLUSHING)
	return 0;
    return s->stop_time > time ? s->stop_time - time : 0;
}

static void start_player(struct pcm_holder *p)
{
    int i;
    for (i = 0; i < PL_PRIV(p)->num_efp_links; i++) {
	struct efp_link *l = &PL_PRIV(p)->efpl[i];
	EFPR(l->efp)->start(l->handle);
    }
    PLAYER(p)->start(p->arg);
}

static void stop_player(struct pcm_holder *p)
{
    int i;
    PLAYER(p)->stop(p->arg);
    for (i = 0; i < PL_PRIV(p)->num_efp_links; i++) {
	struct efp_link *l = &PL_PRIV(p)->efpl[i];
	EFPR(l->efp)->stop(l->handle);
    }
}

static void pcm_start_output(int id)
{
    int i;
    long long now = GETusTIME(0);
    for (i = 0; i < pcm.num_players; i++) {
	struct pcm_holder *p = &pcm.players[i];
	if (!(PLAYER(p)->id & id))
	    continue;
	if (p->opened) {
	    pcm_reset_player(i);
	    pthread_mutex_unlock(&pcm.strm_mtx);
	    start_player(p);
	    pthread_mutex_lock(&pcm.strm_mtx);
	}
    }
    pcm.time = now - MAX_BUFFER_DELAY;
    pcm.playing |= id;
    pcm_printf("PCM: output started, %i\n", pcm.playing);
}

static void pcm_stop_output(int id)
{
    int i;
    for (i = 0; i < pcm.num_players; i++) {
	struct pcm_holder *p = &pcm.players[i];
	if (id != PCM_ID_ANY && !(PLAYER(p)->id & id))
	    continue;
	if (p->opened) {
	    pthread_mutex_unlock(&pcm.strm_mtx);
	    stop_player(p);
	    pthread_mutex_lock(&pcm.strm_mtx);
	}
    }
    pcm.playing &= ~id;
    pcm_printf("PCM: output stopped, %i\n", pcm.playing);
}

static void handle_raw_adj(int strm_idx, double fillup, double time)
{
#define ADJ_PERIOD 2000000
    double raw_delay = INIT_BUFFER_DELAY;
    double delta = (fillup - raw_delay) / (raw_delay * 320);
    double time_delta = time - pcm.stream[strm_idx].last_adj_time;
    if (fillup == 0) {
	delta *= 10;
	if (pcm.stream[strm_idx].last_fillup == 0)
	    delta *= 10;
    }
    if (pcm.stream[strm_idx].adj_time_delay - time_delta < 0) {
	/* of course this heuristic doesnt work, but we have to try... */
	if ((fillup > raw_delay * 2 &&
	     fillup >= pcm.stream[strm_idx].last_fillup) ||
	    (fillup < raw_delay / 1.5 &&
	     fillup <= pcm.stream[strm_idx].last_fillup)) {
	    pcm.stream[strm_idx].raw_speed_adj -= delta;
	    if (pcm.stream[strm_idx].raw_speed_adj > 5)
		pcm.stream[strm_idx].raw_speed_adj = 5;
	    if (pcm.stream[strm_idx].raw_speed_adj < 0.2)
		pcm.stream[strm_idx].raw_speed_adj = 0.2;
//          error("speed %f\n", pcm.stream[strm_idx].raw_speed_adj);
	}
	pcm.stream[strm_idx].last_fillup = fillup;
	pcm.stream[strm_idx].last_adj_time = time;
	if (pcm.stream[strm_idx].adj_time_delay < ADJ_PERIOD)
	    pcm.stream[strm_idx].adj_time_delay += ADJ_PERIOD / 4;
    }
}

static void pcm_handle_get(int strm_idx, double time)
{
    double fillup = calc_buffer_fillup(strm_idx, time);
    if (debug_level('S') >= 9)
	pcm_printf("PCM: Buffer %i fillup=%f\n", strm_idx, fillup);
    switch (pcm.stream[strm_idx].state) {

    case SNDBUF_STATE_INACTIVE:
	error("PCM: getting data from inactive buffer (strm=%i)\n",
	      strm_idx);
	break;

    case SNDBUF_STATE_PLAYING:
	if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW)
	    handle_raw_adj(strm_idx, fillup, time);
	if (rng_count(&pcm.stream[strm_idx].buffer) <
	    pcm.stream[strm_idx].channels * 2 && fillup == 0) {
	    pcm_printf("PCM: ERROR: buffer on stream %i exhausted (%s)\n",
		      strm_idx, pcm.stream[strm_idx].name);
	    /* ditch the last sample here, if it is the only remaining */
	    pcm_clear_stream(strm_idx);
	}
	if (fillup == 0) {
	    if (!(pcm.stream[strm_idx].flags & PCM_FLAG_RAW))
		pcm_printf("PCM: ERROR: buffer on stream %i stalled (%s)\n",
		      strm_idx, pcm.stream[strm_idx].name);
	    pcm.stream[strm_idx].state = SNDBUF_STATE_STALLED;
	}
	if (pcm.stream[strm_idx].state == SNDBUF_STATE_PLAYING &&
		!(pcm.stream[strm_idx].flags & PCM_FLAG_POST) &&
		fillup < WR_BUFFER_LW) {
	    pcm_printf("PCM: buffer fillup %f is too low, %s %i %f\n",
		    fillup, pcm.stream[strm_idx].name,
		    rng_count(&pcm.stream[strm_idx].buffer), time);
	}
	break;

    case SNDBUF_STATE_FLUSHING:
	if (rng_count(&pcm.stream[strm_idx].buffer) <
		pcm.stream[strm_idx].channels * 2 && fillup == 0) {
	    pcm_reset_stream(strm_idx);
	    pcm_printf("PCM: stream %s stopped\n", pcm.stream[strm_idx].name);
	} else if (fillup == 0 && !pcm.stream[strm_idx].stretch) {
	    pcm_stream_stretch(strm_idx);
	    pcm_printf("PCM: stream %s passed wr margin\n",
		    pcm.stream[strm_idx].name);
	}
	break;

    case SNDBUF_STATE_STALLED:
	if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW) {
	    if (fillup == 0 && pcm.stream[strm_idx].last_fillup == 0) {
		pcm_reset_stream(strm_idx);
		pcm.stream[strm_idx].raw_speed_adj = 1;
	    } else {
		handle_raw_adj(strm_idx, fillup, time);
	    }
	}
	break;
    }
}

static void pcm_handle_write(int strm_idx, double time)
{
    if (pcm.playing && time < pcm.time + MAX_BUFFER_PERIOD) {
	if (pcm.stream[strm_idx].state == SNDBUF_STATE_PLAYING)
	    error("PCM: timing screwed up\n");
	pcm_printf("PCM: timing screwed up, s=%s cur=%f pl=%f delta=%f\n",
		pcm.stream[strm_idx].name, time, pcm.time,
		pcm.time - time);
    }

    switch (pcm.stream[strm_idx].state) {
    case SNDBUF_STATE_STALLED:
	pcm.stream[strm_idx].stretch_tot += pcm.stream[strm_idx].stretch_per;
	pcm_printf("PCM: restarting stalled stream %s, str=%f strt=%f\n",
		pcm.stream[strm_idx].name, pcm.stream[strm_idx].stretch_per,
		pcm.stream[strm_idx].stretch_tot);
	pcm.stream[strm_idx].stretch_per = 0;
	break;
    case SNDBUF_STATE_FLUSHING:
	if (pcm.stream[strm_idx].stretch)
	    pcm_printf("PCM: restarting stream %s\n", pcm.stream[strm_idx].name);
	else
	    pcm_printf("PCM: resuming stream %s\n", pcm.stream[strm_idx].name);
	break;
    }

    if (pcm.stream[strm_idx].state != SNDBUF_STATE_PLAYING) {
	pcm.stream[strm_idx].state = SNDBUF_STATE_PLAYING;
	pcm.stream[strm_idx].stretch = 0;
	pcm.stream[strm_idx].prepared = 0;
    }
}

static void pcm_handle_flush(int strm_idx)
{
    switch (pcm.stream[strm_idx].state) {

    case SNDBUF_STATE_INACTIVE:
	if (!(pcm.stream[strm_idx].flags & PCM_FLAG_RAW)) {
	    error("PCM: attempt to flush inactive buffer (%s)\n",
		  pcm.stream[strm_idx].name);
	}
	break;

    case SNDBUF_STATE_PLAYING:
	pcm.stream[strm_idx].state = SNDBUF_STATE_FLUSHING;
	break;

    case SNDBUF_STATE_STALLED:
	pthread_mutex_lock(&pcm.strm_mtx);
	pcm_reset_stream(strm_idx);
	pthread_mutex_unlock(&pcm.strm_mtx);
	break;
    }
}

int pcm_flush(int strm_idx)
{
    pcm_printf("PCM: flushing stream %i (%s)\n", strm_idx,
	     pcm.stream[strm_idx].name);
    pcm_handle_flush(strm_idx);
    return 1;
}

static double get_stream_time(int strm_idx)
{
    long long now = GETusTIME(0);
    double time = pcm.stream[strm_idx].start_time;
    double delta = now - time;
    switch (pcm.stream[strm_idx].state) {

    case SNDBUF_STATE_INACTIVE:
	if (pcm.stream[strm_idx].prepared) {
user_tstamp:
	    if (delta > WRITE_AREA_SIZE) {
		error("PCM: too large delta on stream %s\n",
			pcm.stream[strm_idx].name);
		pcm.stream[strm_idx].start_time = time = now - WRITE_INIT_POS;
	    }
	    return time;
	}
	pcm_printf("PCM: stream not prepared: %s\n", pcm.stream[strm_idx].name);
	return now - WRITE_INIT_POS;

    case SNDBUF_STATE_STALLED:
	pcm.stream[strm_idx].stretch_per = now - WRITE_AREA_SIZE -
		pcm.stream[strm_idx].stop_time;
	return now - WRITE_AREA_SIZE;

    case SNDBUF_STATE_FLUSHING:
	if (pcm.stream[strm_idx].stretch)
	    return now - fmod(delta, MIN_BUFFER_DELAY);
	if (pcm.stream[strm_idx].prepared &&
		!(pcm.stream[strm_idx].flags & PCM_FLAG_SLTS))
	    goto user_tstamp;
	/* in SLoppy TimeStamp mode ignore user's timestamp */
	/* no break */
    case SNDBUF_STATE_PLAYING:
	return pcm.stream[strm_idx].stop_time;

    }
    return 0;
}

double pcm_get_stream_time(int strm_idx)
{
    /* we allow user to write samples to the future to prevent
     * subsequent underflows */
    return get_stream_time(strm_idx) - pcm.stream[strm_idx].stretch_tot;
}

double pcm_time_lock(int strm_idx)
{
    /* well, yes, the lock needs to be per-stream... Go get it. :) */
    pthread_mutex_lock(&pcm.time_mtx);
    return pcm_get_stream_time(strm_idx);
}

void pcm_time_unlock(int strm_idx)
{
    pthread_mutex_unlock(&pcm.time_mtx);
}

static double pcm_calc_tstamp(int strm_idx)
{
    double tstamp = get_stream_time(strm_idx);
    if ((pcm.stream[strm_idx].flags & PCM_FLAG_RAW) &&
	    pcm.stream[strm_idx].state == SNDBUF_STATE_STALLED) {
	long long now = GETusTIME(0);
	if (tstamp < now - WRITE_INIT_POS)
	    tstamp = now - WRITE_INIT_POS;
    }
    return tstamp;
}

void pcm_write_interleaved(sndbuf_t ptr[][SNDBUF_CHANS], int frames,
	int rate, int format, int nchans, int strm_idx)
{
    int i, j;
    struct sample samp;
    double frame_per;
    struct stream *strm;

    strm = &pcm.stream[strm_idx];
    assert(nchans <= strm->channels);
    if (strm->flags & PCM_FLAG_RAW)
	rate /= strm->raw_speed_adj;

    samp.format = format;
    samp.tstamp = 0;
    frame_per = pcm_frame_period_us(rate);
    pthread_mutex_lock(&pcm.strm_mtx);
    for (i = 0; i < frames; i++) {
	int l;
	struct sample s2;
retry:
	samp.tstamp = pcm_calc_tstamp(strm_idx);
	l = peek_last_sample(strm_idx, &s2);
	assert(!(l && samp.tstamp < s2.tstamp));
	for (j = 0; j < strm->channels; j++) {
	    int ch = j % nchans;
	    memcpy(samp.data, &ptr[i][ch], pcm_format_size(format));
	    l = rng_put(&strm->buffer, &samp);
	    if (!l) {
		if (!(strm->flags & PCM_FLAG_RAW)) {
		    error("Sound buffer %i overflowed (%s)\n", strm_idx,
			    strm->name);
		    pcm_reset_stream(strm_idx);
		    goto retry;
		} else {
		    pcm_printf("Sound buffer %i overflowed (%s)\n", strm_idx,
			    strm->name);
		    strm->adj_time_delay = 0;
		    goto cont;
		}
	    }
	}
	pcm_handle_write(strm_idx, samp.tstamp);
	strm->stop_time = samp.tstamp + frame_per;
    }

cont:
    for (i = 0; i < PCM_ID_MAX; i++) {
	int id = 1 << i;
	if (!pcm.is_connected(id, strm->vol_arg))
	    continue;
	if (!(id & pcm.playing))
	    pcm_start_output(id);
    }
    pthread_mutex_unlock(&pcm.strm_mtx);
}

static void pcm_remove_samples(double time)
{
    #define GUARD_SAMPS 1
    int i;
    struct sample s;
    for (i = 0; i < pcm.num_streams; i++) {
	if (pcm.stream[i].state == SNDBUF_STATE_INACTIVE)
	    continue;
	while (rng_count(&pcm.stream[i].buffer) >= pcm.stream[i].channels *
		(GUARD_SAMPS + 1)) {
	    /* we leave GUARD_SAMPS samples below the timestamp untouched */
	    rng_peek(&pcm.stream[i].buffer, pcm.stream[i].channels *
		    GUARD_SAMPS, &s);
	    if (s.tstamp > time)
		break;
	    pcm.stream[i].buf_cnt += pcm.stream[i].channels;
	    rng_remove(&pcm.stream[i].buffer, pcm.stream[i].channels, NULL);
	}
    }
}

static void pcm_get_samples(double time,
		struct sample samp[MAX_STREAMS][SNDBUF_CHANS], int *idxs,
		int out_channels, int id)
{
    int i, j;
    struct sample s[SNDBUF_CHANS], prev_s[SNDBUF_CHANS];

    for (i = 0; i < pcm.num_streams; i++) {
	for (j = 0; j < SNDBUF_CHANS; j++)
	    samp[i][j] = mute_samp;
	if (pcm.stream[i].state == SNDBUF_STATE_INACTIVE ||
		!pcm.is_connected(id, pcm.stream[i].vol_arg))
	    continue;

//    pcm_printf("PCM: stream %i fillup: %i\n", i, rng_count(&pcm.stream[i].buffer));
	for (j = 0; j < pcm.stream[i].channels; j++) {
	    if (idxs[i] >= pcm.stream[i].channels)
		rng_peek(&pcm.stream[i].buffer,
			 idxs[i] - pcm.stream[i].channels + j, &prev_s[j]);
	    else
		prev_s[j] = mute_samp;
	}
	if (out_channels == 2 && pcm.stream[i].channels == 1)
	    prev_s[1] = prev_s[0];
	while (rng_count(&pcm.stream[i].buffer) - idxs[i] >=
	       pcm.stream[i].channels) {
	    for (j = 0; j < pcm.stream[i].channels; j++)
		rng_peek(&pcm.stream[i].buffer, idxs[i] + j, &s[j]);
	    if (s[0].tstamp > time) {
//        pcm_printf("PCM: stream %i time=%lli, req_time=%lli\n", i, s.tstamp, time);
		memcpy(samp[i], prev_s, sizeof(struct sample) * out_channels);
		break;
	    }
	    memcpy(prev_s, s, sizeof(struct sample) * pcm.stream[i].channels);
	    if (out_channels == 2 && pcm.stream[i].channels == 1)
		prev_s[1] = prev_s[0];
	    idxs[i] += pcm.stream[i].channels;
	}
    }
}

static void pcm_mix_samples(struct sample in[][SNDBUF_CHANS],
	sndbuf_t out[SNDBUF_CHANS], int channels, int format,
	double volume[][SNDBUF_CHANS][SNDBUF_CHANS])
{
    int i, j, k;
    int value[SNDBUF_CHANS] = { 0 };

    for (j = 0; j < SNDBUF_CHANS; j++) {
	for (i = 0; i < pcm.num_streams; i++) {
	    if (pcm.stream[i].state == SNDBUF_STATE_INACTIVE)
		continue;
	    for (k = 0; k < SNDBUF_CHANS; k++) {
		if (volume[i][j][k] == 0 || in[i][k].format == PCM_FORMAT_NONE)
		    continue;
		value[j] += sample_to_S16(in[i][k].data, in[i][k].format) *
			volume[i][j][k];
	    }
	}
    }
    for (i = channels; i < SNDBUF_CHANS; i++)
	value[0] += value[i];
    for (i = 0; i < channels; i++) {
	S16_to_sample(pcm_samp_cutoff(value[i], PCM_FORMAT_S16_LE),
		&out[i], format);
    }
}

static void calc_idxs(struct pcm_player_wr *pl, int idxs[MAX_STREAMS])
{
    int i;
    for (i = 0; i < pcm.num_streams; i++) {
	if (pcm.stream[i].state == SNDBUF_STATE_INACTIVE)
	    continue;
	assert(pcm.stream[i].buf_cnt >= pl->last_cnt[i]);
	if (pl->last_idx[i] > pcm.stream[i].buf_cnt - pl->last_cnt[i]) {
	    struct sample s;
	    idxs[i] = pl->last_idx[i] - (pcm.stream[i].buf_cnt -
		    pl->last_cnt[i]);
	    assert(idxs[i] <= rng_count(&pcm.stream[i].buffer));
	    rng_peek(&pcm.stream[i].buffer, idxs[i] - 1, &s);
	    assert(pl->last_tstamp[i] == s.tstamp);
	} else {
	    idxs[i] = 0;
	}
    }
}

static void save_idxs(struct pcm_player_wr *pl, int idxs[MAX_STREAMS])
{
    int i;
    for (i = 0; i < pcm.num_streams; i++) {
	if (pcm.stream[i].state == SNDBUF_STATE_INACTIVE)
	    continue;
	assert(idxs[i] <= rng_count(&pcm.stream[i].buffer));
	if (idxs[i] > 0) {
	    struct sample s;
	    rng_peek(&pcm.stream[i].buffer, idxs[i] - 1, &s);
	    pl->last_tstamp[i] = s.tstamp;
	}
	pl->last_cnt[i] = pcm.stream[i].buf_cnt;
	pl->last_idx[i] = idxs[i];
    }
}

static void get_volumes(int id, double volume[][SNDBUF_CHANS][SNDBUF_CHANS])
{
    int i, j, k;
    for (i = 0; i < pcm.num_streams; i++) {
	struct stream *strm = &pcm.stream[i];
	if (strm->state == SNDBUF_STATE_INACTIVE)
	    continue;
	for (j = 0; j < SNDBUF_CHANS; j++)
	    for (k = 0; k < SNDBUF_CHANS; k++)
		volume[i][j][k] = pcm.get_volume(id, j, k, strm->vol_arg);
    }
}

int pcm_data_get_interleaved(sndbuf_t buf[][SNDBUF_CHANS], int nframes,
			   struct player_params *params)
{
    int idxs[MAX_STREAMS], out_idx, handle, i;
    long long now;
    double start_time, stop_time, frame_period, frag_period, time;
    struct sample samp[MAX_STREAMS][SNDBUF_CHANS];
    double volume[MAX_STREAMS][SNDBUF_CHANS][SNDBUF_CHANS];
    struct pcm_holder *p;

    now = GETusTIME(0);
    handle = params->handle;
    p = &pcm.players[handle];
    start_time = PL_PRIV(p)->time;
    frag_period = nframes * pcm_frame_period_us(params->rate);
    stop_time = start_time + frag_period;
    if (start_time < now - MAX_BUFFER_DELAY) {
	error("PCM: \"%s\" too large delay, start=%f min=%f d=%f\n",
		  p->plugin->name, start_time,
		  now - MAX_BUFFER_DELAY, now - MAX_BUFFER_DELAY - start_time);
	start_time = now - INIT_BUFFER_DELAY;
	stop_time = start_time + frag_period;
    }
    if (start_time > now - MIN_READ_DELAY) {
	pcm_printf("PCM: \"%s\" too small start delay, stop=%f max=%f d=%f\n",
		  p->plugin->name, stop_time,
		  now - MIN_BUFFER_DELAY, stop_time -
		  (now - MIN_BUFFER_DELAY));
	return 0;
    }
    if (stop_time > now - MIN_BUFFER_DELAY) {
	size_t new_nf;
	pcm_printf("PCM: \"%s\" too small stop delay, stop=%f max=%f d=%f\n",
		  p->plugin->name, stop_time,
		  now - MIN_BUFFER_DELAY, stop_time -
		  (now - MIN_BUFFER_DELAY));
	stop_time = now - MIN_BUFFER_DELAY;
	frag_period = stop_time - start_time;
	new_nf = frag_period / pcm_frame_period_us(params->rate);
	assert(new_nf <= nframes);
	nframes = new_nf;
    }
    pcm_printf("PCM: going to process %i samps for %s (st=%f stp=%f d=%f)\n",
	 nframes, p->plugin->name, start_time,
	 stop_time, now - start_time);

    pthread_mutex_lock(&pcm.strm_mtx);
    if (!p->opened) {
	pcm_printf("PCM: player %s already closed\n",
		p->plugin->name);
	pthread_mutex_unlock(&pcm.strm_mtx);
	return 0;
    }
    frame_period = pcm_frame_period_us(params->rate);
    time = start_time;
    calc_idxs(PL_PRIV(p), idxs);
    get_volumes(PLAYER(p)->id, volume);
    for (out_idx = 0; out_idx < nframes; out_idx++) {
	pcm_get_samples(time, samp, idxs, params->channels, PLAYER(p)->id);
	pcm_mix_samples(samp, buf[out_idx], params->channels, params->format,
		volume);
	time += frame_period;
    }
    if (fabs(time - stop_time) > frame_period)
	error("PCM: time=%f stop_time=%f p=%f\n",
		    time, stop_time, frame_period);
    PL_PRIV(p)->time = stop_time;
    save_idxs(PL_PRIV(p), idxs);
    pthread_mutex_unlock(&pcm.strm_mtx);

    for (i = 0; i < PL_PRIV(p)->num_efp_links; i++) {
	struct efp_link *l = &PL_PRIV(p)->efpl[i];
	EFPR(l->efp)->process(l->handle, buf, nframes,
		params->channels, params->format, params->rate);
    }

    if (out_idx != nframes)
	error("PCM: requested=%i prepared=%i\n", nframes, out_idx);

    return out_idx;
}

size_t pcm_data_get(void *data, size_t size,
			   struct player_params *params)
{
    int i, j;
    sndbuf_t buf[size][SNDBUF_CHANS];
    int ss = pcm_format_size(params->format);
    int fsz = params->channels * ss;
    int nframes = size / fsz;

    nframes = pcm_data_get_interleaved(buf, nframes, params);
    for (i = 0; i < nframes; i++) {
	for (j = 0; j < params->channels; j++)
	    memcpy(data + (i * fsz + j * ss), &buf[i][j], ss);
    }
    return nframes * fsz;
}

static void pcm_advance_time(double stop_time)
{
    int i;
    pthread_mutex_lock(&pcm.strm_mtx);
    pcm.time = stop_time;
    /* remove processed samples from input buffers (last sample stays) */
    pcm_remove_samples(stop_time);
    for (i = 0; i < pcm.num_streams; i++) {
	if (pcm.stream[i].state == SNDBUF_STATE_INACTIVE)
	    continue;
	if (debug_level('S') >= 9)
	    pcm_printf("PCM: stream %i fillup2: %i\n", i,
		 rng_count(&pcm.stream[i].buffer));
	pcm_handle_get(i, stop_time + MAX_BUFFER_PERIOD);
    }

    for (i = 0; i < PCM_ID_MAX; i++) {
	if (!count_active_streams(1 << i) && (pcm.playing & (1 << i)))
	    pcm_stop_output(1 << i);
    }
    pthread_mutex_unlock(&pcm.strm_mtx);
}

int pcm_register_player(const struct pcm_player *player, void *arg)
{
    struct pcm_holder *p;
    pcm_printf("PCM: registering player: %s\n", PL_LNAME(player));
    if (pcm.num_players >= MAX_PLAYERS) {
	error("PCM: attempt to register more than %i player\n", MAX_PLAYERS);
	return 0;
    }
    p = &pcm.players[pcm.num_players];
    p->plugin = player;
    p->arg = arg;
    p->priv = malloc(sizeof(struct pcm_player_wr));
    memset(p->priv, 0, sizeof(struct pcm_player_wr));
    return pcm.num_players++;
}

int pcm_register_recorder(const struct pcm_recorder *recorder, void *arg)
{
    struct pcm_holder *p;
    pcm_printf("PCM: registering recorder: %s\n", PL_LNAME(recorder));
    if (pcm.num_recorders >= MAX_RECORDERS) {
	error("PCM: attempt to register more than %i recorder\n", MAX_RECORDERS);
	return 0;
    }
    p = &pcm.recorders[pcm.num_recorders];
    p->plugin = recorder;
    p->arg = arg;
    return pcm.num_recorders++;
}

int pcm_register_efp(const struct pcm_efp *efp, enum EfpType type, void *arg)
{
    struct pcm_holder *p;
    pcm_printf("PCM: registering efp: %s\n", PL_LNAME(efp));
    if (pcm.num_efps >= MAX_EFPS) {
	error("PCM: attempt to register more than %i efps\n", MAX_EFPS);
	return 0;
    }
    p = &pcm.efps[pcm.num_efps];
    p->plugin = efp;
    p->arg = arg;
    p->priv = malloc(sizeof(struct efp_wr));
    memset(p->priv, 0, sizeof(struct efp_wr));
    EF_PRIV(p)->type = type;
    return pcm.num_efps++;
}

void pcm_reset_player(int handle)
{
    long long now = GETusTIME(0);
    struct pcm_holder *p = &pcm.players[handle];
    struct pcm_player_wr *pl = PL_PRIV(p);
    pl->time = now - INIT_BUFFER_DELAY;
    memset(pl->last_idx, 0, sizeof(pl->last_idx));
    memset(pl->last_cnt, 0, sizeof(pl->last_cnt));
}

void pcm_timer(void)
{
    int i;
    long long now = GETusTIME(0);
    for (i = 0; i < pcm.num_players; i++) {
	struct pcm_holder *p = &pcm.players[i];
	struct pcm_player_wr *pl = PL_PRIV(p);
	if (p->opened && PLAYER(p)->timer) {
	    double delta = now - NORM_BUFFER_DELAY - pl->time;
	    PLAYER(p)->timer(delta, p->arg);
	}
    }
    pthread_mutex_lock(&pcm.time_mtx);
    pcm_advance_time(now - MAX_BUFFER_DELAY);
    pthread_mutex_unlock(&pcm.time_mtx);
}

void pcm_done(void)
{
    int i;
    for (i = 0; i < pcm.num_streams; i++) {
	if (pcm.stream[i].state == SNDBUF_STATE_PLAYING ||
		pcm.stream[i].state == SNDBUF_STATE_STALLED)
	    pcm_flush(i);
    }
    pthread_mutex_lock(&pcm.strm_mtx);
    if (pcm.playing)
	pcm_stop_output(PCM_ID_ANY);
    pthread_mutex_unlock(&pcm.strm_mtx);

    pcm_deinit_plugins(pcm.recorders, pcm.num_recorders);
    pcm_deinit_plugins(pcm.players, pcm.num_players);
    pcm_deinit_plugins(pcm.efps, pcm.num_efps);

    for (i = 0; i < pcm.num_streams; i++)
	rng_destroy(&pcm.stream[i].buffer);
    pthread_mutex_destroy(&pcm.strm_mtx);
    pthread_mutex_destroy(&pcm.time_mtx);

    for (i = 0; i < num_dl_handles; i++)
	close_plugin(dl_handles[i]);
    for (i = 0; i < pcm.num_players; i++)
	free(pcm.players[i].priv);
    for (i = 0; i < pcm.num_recorders; i++)
	free(pcm.recorders[i].priv);
    for (i = 0; i < pcm.num_efps; i++)
	free(pcm.efps[i].priv);
}

int pcm_init_plugins(struct pcm_holder *plu, int num)
{
#define SAFE_OPEN(p) (p->plugin->open ? p->plugin->open(p->arg) : 1)
  int i, sel, max_w, max_i, cnt;
  cnt = 0;
  sel = 0;
  /* first deal with enabled plugins */
  for (i = 0; i < num; i++) {
    struct pcm_holder *p = &plu[i];
    p->cfg_flags = (p->plugin->get_cfg ? p->plugin->get_cfg(p->arg) : 0);
    if (p->cfg_flags & PCM_CF_ENABLED) {
      p->opened = SAFE_OPEN(p);
      pcm_printf("PCM: Initializing selected plugin: %s: %s\n",
	  PL_LNAME(p->plugin), p->opened ? "OK" : "Failed");
      if (p->opened) {
        cnt++;
        if (!(p->plugin->flags & PCM_F_PASSTHRU))
          sel++;
      } else {
        p->failed = 1;
      }
    }
  }
  /* then deal with pass-thru plugins */
  for (i = 0; i < num; i++) {
    struct pcm_holder *p = &plu[i];
    if (p->opened || p->failed ||
	    (p->plugin->flags & PCM_F_EXPLICIT) ||
	    !(p->plugin->flags & PCM_F_PASSTHRU))
      continue;
    p->opened = SAFE_OPEN(p);
    pcm_printf("PCM: Initializing pass-through plugin: %s: %s\n",
	    PL_LNAME(p->plugin), p->opened ? "OK" : "Failed");
    if (!p->opened)
      p->failed = 1;
    else
      cnt++;
  }
  /* lastly deal with weight */
  if (!sel) do {
    max_w = -1;
    max_i = -1;
    for (i = 0; i < num; i++) {
      struct pcm_holder *p = &plu[i];
      if (p->opened || p->failed ||
	    (p->plugin->flags & PCM_F_EXPLICIT) ||
	    (p->plugin->flags & PCM_F_PASSTHRU))
	continue;
      if (p->plugin->weight > max_w) {
        if (max_i != -1)
          pcm_printf("PCM: Bypassing plugin: %s: (%i < %i)\n",
		PL_LNAME(plu[max_i].plugin), max_w,
		p->plugin->weight);
        max_w = p->plugin->weight;
        max_i = i;
      }
    }
    if (max_i != -1) {
      struct pcm_holder *p = &plu[max_i];
      p->opened = SAFE_OPEN(p);
      pcm_printf("PCM: Initializing plugin: %s (w=%i): %s\n",
	    PL_LNAME(p->plugin), max_w, p->opened ? "OK" : "Failed");
      if (!p->opened)
        p->failed = 1;
      else
        cnt++;
    }
  } while (max_i != -1 && !plu[max_i].opened);
  return cnt;
}

void pcm_deinit_plugins(struct pcm_holder *plu, int num)
{
    int i;
    for (i = 0; i < num; i++) {
	struct pcm_holder *p = &plu[i];
	if (p->opened) {
	    if (p->plugin->close)
		p->plugin->close(p->arg);
	    p->opened = 0;
	}
    }
}

int pcm_start_input(void *arg)
{
    int i, ret = 0;
    for (i = 0; i < pcm.num_recorders; i++) {
	struct pcm_holder *p = &pcm.recorders[i];
	if (p->opened && RECORDER(p)->start &&
		pcm.checkid2(RECORDER(p)->id2, arg)) {
	    RECORDER(p)->start(p->arg);
	    ret++;
	}
    }
    pcm_printf("PCM: input started, %i\n", ret);
    return ret;
}

void pcm_stop_input(void *arg)
{
    int i;
    for (i = 0; i < pcm.num_recorders; i++) {
	struct pcm_holder *p = &pcm.recorders[i];
	if (p->opened && RECORDER(p)->stop &&
		pcm.checkid2(RECORDER(p)->id2, arg))
	    RECORDER(p)->stop(p->arg);
    }
    pcm_printf("PCM: input stopped\n");
}

void pcm_set_volume_cb(double (*get_vol)(int, int, int, void *))
{
    pcm.get_volume = get_vol;
}

void pcm_set_connected_cb(int (*is_connected)(int, void *))
{
    pcm.is_connected = is_connected;
}

void pcm_set_checkid2_cb(int (*checkid2)(void *, void *))
{
    pcm.checkid2 = checkid2;
}

int pcm_setup_efp(int handle, enum EfpType type, int param1, int param2,
	float param3)
{
    struct pcm_holder *p;
    int i;

    p = &pcm.players[handle];
    for (i = 0; i < pcm.num_efps; i++) {
	struct pcm_holder *e = &pcm.efps[i];
	if (e->opened && EF_PRIV(e)->type == type) {
	    struct efp_link *l =
		    &PL_PRIV(p)->efpl[PL_PRIV(p)->num_efp_links++];
	    assert(PL_PRIV(p)->num_efp_links <= MAX_EFP_LINKS);
	    l->handle = EFPR(e)->setup(param1, param2, param3, e->arg);
	    l->efp = e;
	    pcm_printf("PCM: connected efp \"%s\" to player \"%s\"\n",
		    e->plugin->name, p->plugin->name);
	    return 1;
	}
    }
    return 0;
}

int pcm_setup_hpf(struct player_params *params)
{
    return pcm_setup_efp(params->handle, EFP_HPF, params->rate,
	    params->channels, HPF_CTL);
}

int pcm_parse_cfg(const char *string, const char *name)
{
    char *p;
    int l;
    l = strlen(name);
    p = strstr(string, name);
    if (p && (p == string || p[-1] == ',') && (p[l] == 0 || p[l] == ',')) {
	pcm_printf("PCM: Enabling %s driver\n", name);
	return PCM_CF_ENABLED;
    }
    return 0;
}

char *pcm_parse_params(const char *string, const char *name, const char *param)
{
    char *p, *buf;
    int l;
    l = asprintf(&buf, "%s:%s=", name, param);
    assert(l > 0);
    p = strstr(string, buf);
    free(buf);
    if (p && (p == string || p[-1] == ',')) {
	char *val = strdup(p + l);
	char *c = strchr(val, ',');
	if (c)
	    *c = 0;
	pcm_printf("PCM: Param \"%s\" for driver \"%s\": %s\n", param, name, val);
	return val;
    }
    return NULL;
}
