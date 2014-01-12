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
#include "sound/sndpcm.h"
#include <string.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>


#define SND_BUFFER_SIZE 50000	/* enough to hold 0.5s of 44100/stereo */
#define RAW_BUFFER_SIZE 100000	/* can hold 0.5s of 44100/stereo/16bit */
#define BUFFER_DELAY 40000.0

#define MAX_BUFFER_DELAY (BUFFER_DELAY * 3)
#define INIT_BUFFER_DELAY (BUFFER_DELAY * 2)
#define MIN_BUFFER_DELAY (BUFFER_DELAY)
#define MAX_BUFFER_PERIOD (MAX_BUFFER_DELAY - MIN_BUFFER_DELAY)
#define MIN_GUARD_SIZE 1024
#define MIN_READ_GUARD_PERIOD (1000000 * MIN_GUARD_SIZE / (2 * 44100))
#define WR_BUFFER_LW (BUFFER_DELAY / 2)
#define MIN_READ_DELAY (MIN_BUFFER_DELAY + MIN_READ_GUARD_PERIOD)

enum {
    SNDBUF_STATE_INACTIVE,
    SNDBUF_STATE_PLAYING,
    SNDBUF_STATE_FLUSHING,
    SNDBUF_STATE_STALLED,
};

#define STREAM_INACTIVE(i) (pcm.stream[i].state == SNDBUF_STATE_INACTIVE || \
    pcm.stream[i].state == SNDBUF_STATE_STALLED)

struct sample {
    int format;
    double tstamp;
    unsigned char data[2];
};

static const struct sample mute_samp = { PCM_FORMAT_NONE, 0, {0, 0} };

struct raw_buffer {
    unsigned char data[RAW_BUFFER_SIZE];
    int idx;
};

struct stream {
    int channels;
    struct rng_s buffer;
    int state;
    int mode;
    int flags;
    double start_time;
    /* for the raw channels heuristic */
    double raw_speed_adj;
    double last_adj_time;
    double last_fillup;
    /* --- */
    char *name;
    int id;
};

struct pcm_player_wr {
    struct pcm_player player;
    double time;
    int opened;
};

#define MAX_STREAMS 10
#define MAX_PLAYERS 10
struct pcm_struct {
    struct stream stream[MAX_STREAMS];
    int num_streams;
    pthread_mutex_t strm_mtx;
    struct pcm_player_wr players[MAX_PLAYERS];
    int num_players;
    int playing;
    double time;
    struct raw_buffer out_buf;
} pcm;

int pcm_init(void)
{
    int i;
    S_printf("PCM: init\n");
    memset(&pcm, 0, sizeof(pcm));
    pthread_mutex_init(&pcm.strm_mtx, NULL);
//  memset(&players, 0, sizeof(players));
#ifdef USE_DL_PLUGINS
#ifdef SDL_SUPPORT
    load_plugin("sdl");
#endif
#ifdef USE_ALSA
    load_plugin("alsa");
#endif
#ifdef USE_SNDFILE
    load_plugin("sndfile");
#endif
#ifdef USE_FLUIDSYNTH
    load_plugin("fluidsynth");
#endif
#endif
    for (i = 0; i < pcm.num_players; i++) {
	struct pcm_player_wr *p = &pcm.players[i];
	if (p->player.open)
	    p->opened = p->player.open(p->player.arg);
	else
	    p->opened = 1;
	if (!p->opened) {
	    S_printf("PCM: \"%s\" failed to open\n",
		  p->player.name);
	}
    }
    return 1;
}

static void pcm_clear_stream(int strm_idx)
{
    rng_clear(&pcm.stream[strm_idx].buffer);
}

static void pcm_reset_stream(int strm_idx)
{
    pcm_clear_stream(strm_idx);
    pcm.stream[strm_idx].state = SNDBUF_STATE_INACTIVE;
    pcm.stream[strm_idx].mode = PCM_MODE_NORMAL;
}

int pcm_allocate_stream(int channels, char *name, int id)
{
    int index;
    if (pcm.num_streams >= MAX_STREAMS) {
	error("PCM: stream pool exhausted, max=%i\n", MAX_STREAMS);
	return -1;
    }
    index = pcm.num_streams++;
    rng_init(&pcm.stream[index].buffer, SND_BUFFER_SIZE,
	     sizeof(struct sample));
    pcm.stream[index].channels = channels;
    pcm.stream[index].name = name;
    pcm.stream[index].id = id;
    pcm_reset_stream(index);
    S_printf("PCM: Stream %i allocated for \"%s\"\n", index, name);
    return index;
}

void pcm_set_flag(int strm_idx, int flag)
{
    if (pcm.stream[strm_idx].flags & flag)
	return;
    S_printf("PCM: setting flag %i for stream %i (%s)\n",
	     flag, strm_idx, pcm.stream[strm_idx].name);
    pcm.stream[strm_idx].flags |= flag;
    if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW)
	pcm.stream[strm_idx].raw_speed_adj = 1.0;
}

void pcm_set_mode(int strm_idx, int mode)
{
    if (pcm.stream[strm_idx].mode == mode)
	return;
    S_printf("PCM: setting mode %i for stream %i (%s)\n",
	     mode, strm_idx, pcm.stream[strm_idx].name);
    pcm.stream[strm_idx].mode = mode;
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

static void S16_to_sample(short sample, void *buf, int format)
{
    switch (format) {
    case PCM_FORMAT_U8:
	*(unsigned char *) buf = SS2UC(sample);
	break;
    case PCM_FORMAT_S8:
	*(signed char *) buf = SS2SC(sample);
	break;
    case PCM_FORMAT_U16_LE:
	*(unsigned short *) buf = SS2US(sample);
	break;
    case PCM_FORMAT_S16_LE:
	*(signed short *) buf = sample;
	break;
    default:
	error("PCM: format1 %i is not supported\n", format);
    }
}

static int count_active_streams(void)
{
    int i, ret = 0;
    for (i = 0; i < pcm.num_streams; i++)
	if (!STREAM_INACTIVE(i))
	    ret++;
    return ret;
}

double pcm_frame_period_us(double rate)
{
    return (1000000 / rate);
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

static double calc_buffer_fillup(int strm_idx, double time)
{
    struct sample samp;
    if (!peek_last_sample(strm_idx, &samp))
	return 0;
    return samp.tstamp > time ? samp.tstamp - time : 0;
}

static int pcm_truncate_stream(int strm_idx)
{
    double fillup;
    if (pcm.stream[strm_idx].state != SNDBUF_STATE_FLUSHING &&
	    pcm.stream[strm_idx].state != SNDBUF_STATE_STALLED)
	return 0;
    if (pcm.stream[strm_idx].state == SNDBUF_STATE_STALLED) {
	pcm_reset_stream(strm_idx);
	return 1;
    }
    assert(pcm.playing);
    fillup = calc_buffer_fillup(strm_idx, pcm.time + MAX_BUFFER_PERIOD);
    if (fillup == 0) {
	pcm_reset_stream(strm_idx);
	return 1;
    }
    return 0;
}

static void pcm_start_output(void)
{
    int i;
    long long now = GETusTIME(0);
    for (i = 0; i < pcm.num_players; i++) {
	struct pcm_player_wr *p = &pcm.players[i];
	if (p->opened) {
	    p->time = now - INIT_BUFFER_DELAY;
	    p->player.start(p->player.arg);
	}
    }
    pcm.time = now - MAX_BUFFER_DELAY;
    pcm.playing = 1;
    S_printf("PCM: output started\n");
}

static void pcm_stop_output(void)
{
    int i;
    for (i = 0; i < pcm.num_players; i++) {
	struct pcm_player_wr *p = &pcm.players[i];
	if (p->opened)
	    p->player.stop(p->player.arg);
    }
    pcm.playing = 0;
    S_printf("PCM: output stopped\n");
}

static void pcm_handle_get(int strm_idx, double time)
{
    double fillup = calc_buffer_fillup(strm_idx, time);
    S_printf("PCM: Buffer %i fillup=%f\n", strm_idx, fillup);
    switch (pcm.stream[strm_idx].state) {

    case SNDBUF_STATE_INACTIVE:
    case SNDBUF_STATE_STALLED:
	error("PCM: getting data from inactive buffer (strm=%i)\n",
	      strm_idx);
	break;

    case SNDBUF_STATE_PLAYING:
#if 1
	if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW) {
#define ADJ_PERIOD 2000000
	    double raw_delay = INIT_BUFFER_DELAY;
	    double delta = (fillup - raw_delay) / (raw_delay * 320);
	    if (time - pcm.stream[strm_idx].last_adj_time > ADJ_PERIOD) {
		/* of course this heuristic doesnt work, but we have to try... */
		if ((fillup > raw_delay * 2 &&
		     fillup > pcm.stream[strm_idx].last_fillup) ||
		    (fillup < raw_delay / 1.5 &&
		     fillup < pcm.stream[strm_idx].last_fillup)) {
		    pcm.stream[strm_idx].raw_speed_adj -= delta;
//          error("speed %f\n", pcm.stream[strm_idx].raw_speed_adj);
		}
		pcm.stream[strm_idx].last_fillup = fillup;
		pcm.stream[strm_idx].last_adj_time = time;
	    }
	}
#endif
	if (rng_count(&pcm.stream[strm_idx].buffer) <
	    pcm.stream[strm_idx].channels * 2 && fillup == 0) {
	    /* ditch the last sample here, if it is the only remaining */
	    pcm_clear_stream(strm_idx);
	}
	if (fillup == 0) {
	    if (!(pcm.stream[strm_idx].flags & PCM_FLAG_RAW) &&
		pcm.stream[strm_idx].mode == PCM_MODE_NORMAL)
		S_printf("PCM: ERROR: buffer on stream %i exhausted (%s)\n",
		      strm_idx, pcm.stream[strm_idx].name);
	    pcm.stream[strm_idx].state = SNDBUF_STATE_STALLED;
	}
	if (pcm.stream[strm_idx].state == SNDBUF_STATE_PLAYING &&
		pcm.stream[strm_idx].mode == PCM_MODE_NORMAL &&
		fillup < WR_BUFFER_LW) {
	    S_printf("PCM: buffer fillup %f is too low, %s %i %f\n",
		    fillup, pcm.stream[strm_idx].name,
		    rng_count(&pcm.stream[strm_idx].buffer), time);
	    /* FIXME: how do we propagate that event to the caller?
	     * The hackish (but simple) solution was removed in f0e0a7d437
	     * Maybe we don't even need to? The only known reason for the
	     * permanent buffer starvation is when the caller does
	     * "unnecessary" flushes.
	     * Anyway, SB code was fixed to minimize the chances of
	     * unnecessary flushes, so maybe the problem does not exist,
	     * or exists only for a poorly written DOS progs.
	     * Of course there are many reasons for the temporary
	     * starvations, we only need to make sure they are quickly
	     * re-filled. */
	}
	break;

    case SNDBUF_STATE_FLUSHING:
	if (rng_count(&pcm.stream[strm_idx].buffer) <
	    pcm.stream[strm_idx].channels * 2 && fillup == 0)
	    pcm_reset_stream(strm_idx);
	break;

    }
}

static int pcm_handle_write(int strm_idx, double time)
{
    int err;
    switch (pcm.stream[strm_idx].state) {

    case SNDBUF_STATE_FLUSHING:
	err = pcm_truncate_stream(strm_idx);
	if (err)
	    return err;
	break;

    case SNDBUF_STATE_INACTIVE:
    case SNDBUF_STATE_STALLED:
	pcm.stream[strm_idx].start_time = time;
	S_printf("PCM: stream %i (%s) started, time=%f delta=%f\n",
		 strm_idx, pcm.stream[strm_idx].name, time,
		 time - pcm.time);
	break;

    }

    if (pcm.playing && time < pcm.time + MAX_BUFFER_PERIOD) {
	if (pcm.stream[strm_idx].state == SNDBUF_STATE_PLAYING)
	    error("PCM: timing screwed up\n");
	S_printf("PCM: timing screwed up, s=%s cur=%f pl=%f delta=%f\n",
		pcm.stream[strm_idx].name, time, pcm.time,
		pcm.time - time);
	err = pcm_truncate_stream(strm_idx);
	if (err)
	    return err;
    }
    pcm.stream[strm_idx].state = SNDBUF_STATE_PLAYING;
    return 0;
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
	pcm_reset_stream(strm_idx);
	break;
    }
}

int pcm_flush(int strm_idx)
{
    S_printf("PCM: flushing stream %i (%s)\n", strm_idx,
	     pcm.stream[strm_idx].name);
    pcm_handle_flush(strm_idx);
    return 1;
}

static double pcm_get_stream_time(int strm_idx)
{
    struct sample samp;
    if (STREAM_INACTIVE(strm_idx) ||
	!peek_last_sample(strm_idx, &samp))
	return 0;
    return samp.tstamp;
}

static double pcm_calc_tstamp(double rate, int strm_idx)
{
    double time, period, tstamp;
    long long now = GETusTIME(0);
    if (rate == 0)
	return now;
    time = pcm_get_stream_time(strm_idx);
    if (time == 0)
	return now;
    period = pcm_frame_period_us(rate);
    tstamp = time + period;
    if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW) {
	if (tstamp < now)
	    tstamp = now;
    }
    return tstamp;
}

void pcm_write_interleaved(sndbuf_t ptr[][SNDBUF_CHANS], int frames,
	double rate, int format, int nchans, int strm_idx)
{
    int i, j;
    struct sample samp;
    if (nchans > pcm.stream[strm_idx].channels) {
	dosemu_error("PCM: wrong num of channels specified %i, max %i, "
		"name=%s\n",
		nchans, pcm.stream[strm_idx].channels,
		pcm.stream[strm_idx].name);
	return;
    }
    if (rng_count(&pcm.stream[strm_idx].buffer) + frames * nchans >=
	SND_BUFFER_SIZE) {
	error("Sound buffer %i overflowed (%s)\n", strm_idx,
	      pcm.stream[strm_idx].name);
	pcm_reset_stream(strm_idx);
//    return;
    }

    if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW)
	rate /= pcm.stream[strm_idx].raw_speed_adj;

    samp.format = format;
    pthread_mutex_lock(&pcm.strm_mtx);
    for (i = 0; i < frames; i++) {
	int l;
	struct sample s2;
	do {
	    /* unfortunately the state machine may need a retry */
	    samp.tstamp = pcm_calc_tstamp(rate, strm_idx);
	    l = pcm_handle_write(strm_idx, samp.tstamp);
	} while (l);
	l = peek_last_sample(strm_idx, &s2);
	assert(!(l && samp.tstamp < s2.tstamp));
	for (j = 0; j < pcm.stream[strm_idx].channels; j++) {
	    int ch = j % nchans;
	    memcpy(samp.data, &ptr[i][ch], pcm_format_size(format));
	    rng_put(&pcm.stream[strm_idx].buffer, &samp);
	}
    }
    pthread_mutex_unlock(&pcm.strm_mtx);
//S_printf("PCM: time=%f\n", samp.tstamp);

    if (!pcm.playing)
	pcm_start_output();
}

static void pcm_remove_samples(double time)
{
    #define GUARD_SAMPS 1
    int i;
    struct sample s;
    for (i = 0; i < pcm.num_streams; i++) {
	if (STREAM_INACTIVE(i))
	    continue;
	while (rng_count(&pcm.stream[i].buffer) >= pcm.stream[i].channels *
		(GUARD_SAMPS + 1)) {
	    /* we leave GUARD_SAMPS samples below the timestamp untouched */
	    rng_peek(&pcm.stream[i].buffer, pcm.stream[i].channels *
		    GUARD_SAMPS, &s);
	    if (s.tstamp > time)
		break;
	    rng_remove(&pcm.stream[i].buffer, pcm.stream[i].channels, NULL);
	}
    }
}

static int pcm_get_samples(double time,
		struct sample samp[MAX_STREAMS][SNDBUF_CHANS], int *idxs,
		int out_channels, int id)
{
    int i, j, ret = 0;
    struct sample s[SNDBUF_CHANS], prev_s[SNDBUF_CHANS];

    for (i = 0; i < pcm.num_streams; i++) {
	for (j = 0; j < out_channels; j++)
	    samp[i][j] = mute_samp;
	if (STREAM_INACTIVE(i) || pcm.stream[i].id != id)
	    continue;

//    S_printf("PCM: stream %i fillup: %i\n", i, rng_count(&pcm.stream[i].buffer));
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
//        S_printf("PCM: stream %i time=%lli, req_time=%lli\n", i, s.tstamp, time);
		memcpy(samp[i], prev_s, sizeof(struct sample) * out_channels);
		if (time >= pcm.stream[i].start_time)
		    ret++;
		break;
	    }
	    memcpy(prev_s, s, sizeof(struct sample) * pcm.stream[i].channels);
	    if (out_channels == 2 && pcm.stream[i].channels == 1)
		prev_s[1] = prev_s[0];
	    idxs[i] += pcm.stream[i].channels;
	}
    }
    return ret;
}

static void pcm_mix_samples(struct sample in[][2], void *out, int channels,
			    int format)
{
    int i, j;
    int value[2] = { 0, 0 };

    for (j = 0; j < channels; j++) {
	for (i = 0; i < pcm.num_streams; i++) {
	    if (in[i][j].format == PCM_FORMAT_NONE)
		continue;
	    value[j] += sample_to_S16(in[i][j].data, in[i][j].format);
	}
	S16_to_sample(pcm_samp_cutoff(value[j], PCM_FORMAT_S16_LE),
		out + j * pcm_format_size(format), format);
    }
}

size_t pcm_data_get(void *data, size_t size,
			   struct player_params *params)
{
    int samp_sz, idxs[MAX_STREAMS], handle, id, ret = 0;
    long long now;
    double start_time, stop_time, frame_period, frag_period, time;
    struct sample samp[MAX_STREAMS][2];

    if (size > SND_BUFFER_SIZE) {
	S_printf("PCM: size %zi is too much\n", size);
	size = SND_BUFFER_SIZE;
    }
    now = GETusTIME(0);
    handle = params->handle;
    id = pcm.players[handle].player.id;
    start_time = pcm.players[handle].time;
    frag_period = pcm_frag_period(size, params);
    stop_time = start_time + frag_period;
    if (start_time < now - MAX_BUFFER_DELAY) {
	error("PCM: \"%s\" too large delay, start=%f min=%f d=%f\n",
		  pcm.players[handle].player.name, start_time,
		  now - MAX_BUFFER_DELAY, now - MAX_BUFFER_DELAY - start_time);
	start_time = now - INIT_BUFFER_DELAY;
	stop_time = start_time + frag_period;
    }
    if (start_time > now - MIN_READ_DELAY) {
	error("PCM: \"%s\" too small delay, stop=%f max=%f d=%f\n",
		  pcm.players[handle].player.name, stop_time,
		  now - MIN_BUFFER_DELAY, stop_time -
		  (now - MIN_BUFFER_DELAY));
	start_time = now - INIT_BUFFER_DELAY;
	stop_time = start_time + frag_period;
    }
    if (stop_time > now - MIN_BUFFER_DELAY) {
	size_t new_size;
	S_printf("PCM: \"%s\" too small delay, stop=%f max=%f d=%f\n",
		  pcm.players[handle].player.name, stop_time,
		  now - MIN_BUFFER_DELAY, stop_time -
		  (now - MIN_BUFFER_DELAY));
	stop_time = now - MIN_BUFFER_DELAY;
	frag_period = stop_time - start_time;
	new_size = pcm_frag_size(frag_period, params);
	assert(new_size <= size);
	size = new_size;
    }
    S_printf("PCM: going to process %zi bytes for %s (st=%f stp=%f d=%f)\n",
	 size, pcm.players[handle].player.name, start_time,
	 stop_time, now - start_time);

    pthread_mutex_lock(&pcm.strm_mtx);
    frame_period = pcm_frame_period_us(params->rate);
    time = start_time;
    samp_sz = pcm_format_size(params->format);
    memset(idxs, 0, sizeof(idxs));
    pcm.out_buf.idx = 0;

    while (pcm.out_buf.idx < size) {
	pcm_get_samples(time, samp, idxs, params->channels, id);
	if (pcm.out_buf.idx + samp_sz * params->channels >
		RAW_BUFFER_SIZE) {
	    error("PCM: output buffer overflowed\n");
	    break;
	}
	pcm_mix_samples(samp, pcm.out_buf.data + pcm.out_buf.idx,
			    params->channels, params->format);
	pcm.out_buf.idx += samp_sz * params->channels;
	time += frame_period;
    }
    if (fabs(time - stop_time) > frame_period)
	error("PCM: time=%f stop_time=%f p=%f\n",
		    time, stop_time, frame_period);

    /* feed the data to player */
    if (data)
	memcpy(data, pcm.out_buf.data, pcm.out_buf.idx);
    pcm.players[handle].time = stop_time;

    ret = pcm.out_buf.idx;
    pthread_mutex_unlock(&pcm.strm_mtx);
    if (ret != size)
	error("PCM: requested=%zi prepared=%i\n", size, ret);
    return ret;
}

static void pcm_advance_time(double stop_time)
{
    int i;
    pcm.time = stop_time;
    pthread_mutex_lock(&pcm.strm_mtx);
    /* remove processed samples from input buffers (last sample stays) */
    pcm_remove_samples(stop_time);
    for (i = 0; i < pcm.num_streams; i++) {
	if (STREAM_INACTIVE(i))
	    continue;
	S_printf("PCM: stream %i fillup2: %i\n", i,
		 rng_count(&pcm.stream[i].buffer));
	pcm_handle_get(i, stop_time + MAX_BUFFER_PERIOD);
    }
    pthread_mutex_unlock(&pcm.strm_mtx);

    if (!count_active_streams() && pcm.playing)
	pcm_stop_output();
}

int pcm_register_player(struct pcm_player player)
{
    S_printf("PCM: registering clocked player: %s\n", player.name);
    if (pcm.num_players >= MAX_PLAYERS) {
	error("PCM: attempt to register more than %i clocked player\n",
	      MAX_PLAYERS);
	return 0;
    }
    pcm.players[pcm.num_players].player = player;
    return pcm.num_players++;
}

void pcm_timer(void)
{
    int i;
    long long now = GETusTIME(0);
    for (i = 0; i < pcm.num_players; i++) {
	struct pcm_player_wr *p = &pcm.players[i];
	if (p->opened && p->player.timer) {
	    double delta = now - INIT_BUFFER_DELAY - pcm.players[i].time;
	    p->player.timer(delta, p->player.arg);
	}
    }
    pcm_advance_time(now - MAX_BUFFER_DELAY);
}

void pcm_reset(void)
{
    int i;
    S_printf("PCM: reset\n");
    if (pcm.playing)
	pcm_stop_output();
    pthread_mutex_lock(&pcm.strm_mtx);
    for (i = 0; i < pcm.num_streams; i++)
	pcm_reset_stream(i);
    pthread_mutex_unlock(&pcm.strm_mtx);
}

void pcm_done(void)
{
    int i;
    pthread_mutex_lock(&pcm.strm_mtx);
    for (i = 0; i < pcm.num_streams; i++) {
	if (pcm.stream[i].state == SNDBUF_STATE_PLAYING ||
		pcm.stream[i].state == SNDBUF_STATE_STALLED)
	    pcm_flush(i);
    }
    pthread_mutex_unlock(&pcm.strm_mtx);
    if (pcm.playing)
	pcm_stop_output();
    for (i = 0; i < pcm.num_players; i++) {
	struct pcm_player_wr *p = &pcm.players[i];
	if (p->opened) {
	    if (p->player.close)
		p->player.close(p->player.arg);
	    p->opened = 0;
	}
    }
    for (i = 0; i < pcm.num_streams; i++)
	rng_destroy(&pcm.stream[i].buffer);
    pthread_mutex_destroy(&pcm.strm_mtx);
}
