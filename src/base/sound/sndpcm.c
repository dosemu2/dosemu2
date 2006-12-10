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
#include "timers.h"
#include "sound/sound.h"
#include "sound/sndpcm.h"
#include "nullsnd.h"
#include <string.h>
#include <math.h>
#include <limits.h>
#include <assert.h>


#define SND_BUFFER_SIZE 200000	/* enough to hold 2.2s of 44100/stereo */
#define RAW_BUFFER_SIZE 400000	/* can hold 2.2s of 44100/stereo/16bit */
#define BUFFER_DELAY 200000.0

enum {
    SNDBUF_STATE_INACTIVE,
    SNDBUF_STATE_PLAYING,
    SNDBUF_STATE_FLUSHING
};

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
};

#define MAX_STREAMS 10
struct pcm_struct {
    struct stream stream[MAX_STREAMS];
    int num_streams;
    int playing;
    struct raw_buffer out_buf;
} pcm;

struct clocked_player_wr {
    struct clocked_player player;
    double time;
};

struct unclocked_player_wr {
    struct unclocked_player player;
    struct player_params params;
    int opened;
};

#define MAX_PLAYERS 10
struct players_struct {
    struct clocked_player_wr clocked;
    struct unclocked_player_wr unclocked[MAX_PLAYERS];
    int num_clocked, num_unclocked;
} players = {
.num_clocked = 0,.num_unclocked = 0};


int pcm_init(void)
{
    int i, have_clc = 0;
    S_printf("PCM: init\n");
    memset(&pcm, 0, sizeof(pcm));
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
#endif
    if (players.num_clocked) {
	if (!(have_clc = players.clocked.player.open())) {
	    error("PCM: \"%s\" failed to open\n",
		  players.clocked.player.name);
	    players.num_clocked--;
	}
    }
    if (!have_clc) {
	S_printf("PCM: no clocked players available, using NULL device\n");
	if (!nullsnd_init() || !players.num_clocked ||
	    !players.clocked.player.open()) {
	    error("PCM: BUG: cannot get a timing source!\n");
	    return 0;
	}
    }
    for (i = 0; i < players.num_unclocked; i++) {
	players.unclocked[i].opened =
	    players.unclocked[i].player.open(&players.unclocked[i].params);
    }
    return 1;
}

static void pcm_reset_stream(int strm_idx)
{
    rng_clear(&pcm.stream[strm_idx].buffer);
    pcm.stream[strm_idx].state = SNDBUF_STATE_INACTIVE;
    pcm.stream[strm_idx].mode = PCM_MODE_NORMAL;
}

int pcm_allocate_stream(int channels, char *name)
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

static void pcm_truncate_stream(int strm_idx)
{
 /*FIXME*/
}

static int count_active_streams(void)
{
    int i, ret = 0;
    for (i = 0; i < pcm.num_streams; i++)
	if (pcm.stream[i].state != SNDBUF_STATE_INACTIVE)
	    ret++;
    return ret;
}

double pcm_samp_period(double rate, int channels)
{
    return 1000000 / (rate * channels);
}

double pcm_frag_period(int size, struct player_params *params)
{
    int samp_sz, nsamps;
    samp_sz = pcm_format_size(params->format);
    nsamps = size / samp_sz;
    return nsamps * pcm_samp_period(params->rate, params->channels);
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

static void pcm_start_output(void)
{
    players.clocked.time = 0;
    players.clocked.player.start();
    pcm.playing = 1;
}

static void pcm_stop_output(void)
{
    players.clocked.player.stop();
    pcm.playing = 0;
}

static void pcm_handle_get(int strm_idx, double time)
{
    double fillup = calc_buffer_fillup(strm_idx, time);
    S_printf("PCM: Buffer %i fillup=%f\n", strm_idx, fillup);
    switch (pcm.stream[strm_idx].state) {

    case SNDBUF_STATE_INACTIVE:
	error("PCM: getting data from inactive buffer (strm=%i)\n",
	      strm_idx);
	break;

    case SNDBUF_STATE_PLAYING:
#if 1
	if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW) {
#define ADJ_PERIOD 2000000
	    double raw_delay = BUFFER_DELAY * 2;
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
	    if (!(pcm.stream[strm_idx].flags & PCM_FLAG_RAW) &&
		pcm.stream[strm_idx].mode == PCM_MODE_NORMAL)
		error("PCM: ERROR: buffer on stream %i exhausted (%s)\n",
		      strm_idx, pcm.stream[strm_idx].name);
	    pcm_reset_stream(strm_idx);
	}
	break;

    case SNDBUF_STATE_FLUSHING:
	if (rng_count(&pcm.stream[strm_idx].buffer) <
	    pcm.stream[strm_idx].channels * 2 && fillup == 0)
	    pcm_reset_stream(strm_idx);
	break;

    }
}

static void pcm_handle_write(int strm_idx, double time)
{
    switch (pcm.stream[strm_idx].state) {

    case SNDBUF_STATE_FLUSHING:
	pcm_truncate_stream(strm_idx);
	break;

    case SNDBUF_STATE_INACTIVE:
	pcm.stream[strm_idx].start_time = time;
	S_printf("PCM: stream %i (%s) started, time=%f delta=%f\n",
		 strm_idx, pcm.stream[strm_idx].name, time,
		 time - players.clocked.time);
	break;

    }
    pcm.stream[strm_idx].state = SNDBUF_STATE_PLAYING;
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

    }
}

int pcm_flush(int strm_idx)
{
    S_printf("PCM: flushing stream %i (%s)\n", strm_idx,
	     pcm.stream[strm_idx].name);
    pcm_handle_flush(strm_idx);
    return 1;
}

double pcm_get_stream_time(int strm_idx)
{
    struct sample samp;
    long long now = GETusTIME(0);
    if (pcm.stream[strm_idx].state == SNDBUF_STATE_INACTIVE ||
	!peek_last_sample(strm_idx, &samp))
	return now;
    return samp.tstamp;
}

static double pcm_calc_tstamp(double rate, int strm_idx)
{
    double time, period, tstamp;
    if (rate == 0)
	return GETusTIME(0);
    time = pcm_get_stream_time(strm_idx);
    if (pcm.stream[strm_idx].state == SNDBUF_STATE_INACTIVE)
	return time;
    period = pcm_samp_period(rate, pcm.stream[strm_idx].channels);
    tstamp = time + period;
    if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW) {
	long long now = GETusTIME(0);
	if (tstamp < now)
	    tstamp = now;
    }
    return tstamp;
}

void pcm_write_samples(void *ptr, size_t size, double rate, int format,
		       int strm_idx)
{
    int i;
    struct sample samp;
    int num_samples = size / pcm_format_size(format);
    if (rng_count(&pcm.stream[strm_idx].buffer) + num_samples >=
	SND_BUFFER_SIZE) {
	error("Sound buffer %i overflowed (%s)\n", strm_idx,
	      pcm.stream[strm_idx].name);
	pcm_reset_stream(strm_idx);
//    return;
    }

    if (pcm.stream[strm_idx].flags & PCM_FLAG_RAW)
	rate /= pcm.stream[strm_idx].raw_speed_adj;

    players.clocked.player.lock();
    for (i = 0; i < num_samples; i++) {
	samp.tstamp = pcm_calc_tstamp(rate, strm_idx);
	samp.format = format;
	memcpy(samp.data, ptr + i * pcm_format_size(format),
	       pcm_format_size(format));
	rng_put(&pcm.stream[strm_idx].buffer, &samp);
	pcm_handle_write(strm_idx, samp.tstamp);
    }
    players.clocked.player.unlock();
//S_printf("PCM: time=%f\n", samp.tstamp);

    if (!pcm.playing)
	pcm_start_output();
}

static int pcm_get_samples(double time, struct sample samp[MAX_STREAMS][2],
			   int shift, int *idxs)
{
    int i, j, ret = 0, idxs2[MAX_STREAMS] = { 0, };
    struct sample s, s2[2];

    if (!idxs)
	idxs = idxs2;
    for (i = 0; i < pcm.num_streams; i++) {
	if (samp)
	    for (j = 0; j < pcm.stream[i].channels; j++)
		samp[i][j].format = PCM_FORMAT_NONE;
	if (pcm.stream[i].state == SNDBUF_STATE_INACTIVE)
	    continue;

//    S_printf("PCM: stream %i fillup: %i\n", i, rng_count(&pcm.stream[i].buffer));
	for (j = 0; j < pcm.stream[i].channels; j++) {
	    if (idxs[i] >= pcm.stream[i].channels)
		rng_peek(&pcm.stream[i].buffer,
			 idxs[i] - pcm.stream[i].channels + j, &s2[j]);
	    else
		s2[j] = mute_samp;
	}
	while (rng_count(&pcm.stream[i].buffer) - idxs[i] >=
	       pcm.stream[i].channels) {
	    rng_peek(&pcm.stream[i].buffer, idxs[i], &s);
	    if (s.tstamp > time) {
//        S_printf("PCM: stream %i time=%lli, req_time=%lli\n", i, s.tstamp, time);
		if (samp)
		    for (j = 0; j < pcm.stream[i].channels; j++)
			samp[i][j] = s2[j];
		if (time >= pcm.stream[i].start_time)
		    ret++;
		break;
	    }
	    if (shift && idxs[i] >= pcm.stream[i].channels) {
		for (j = 0; j < pcm.stream[i].channels; j++)
		    rng_get(&pcm.stream[i].buffer, NULL);
		idxs[i] -= pcm.stream[i].channels;
	    }
	    for (j = 0; j < pcm.stream[i].channels; j++)
		rng_peek(&pcm.stream[i].buffer, idxs[i]++, &s2[j]);
	}
    }
    return ret;
}

static void pcm_process_channels(struct sample samp[][2], int out_channels)
{
    int i;
    for (i = 0; i < pcm.num_streams; i++) {
	if (out_channels == 2 && pcm.stream[i].channels == 1)
	    samp[i][1] = samp[i][0];
    }
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
	if (value[j] > SHRT_MAX)
	    value[j] = SHRT_MAX;
	if (value[j] < SHRT_MIN)
	    value[j] = SHRT_MIN;
	S16_to_sample(value[j], out + j * pcm_format_size(format), format);
    }
}

/* this is called by the clocked player. It prepares the data for
 * him, and, just in case, feeds it to all the unclocked players too. */
static size_t pcm_data_get(void *data, size_t size,
			   struct player_params *params)
{
    int i, samp_sz, have_data, idxs[MAX_STREAMS], ret = 0;
    long long now;
    double start_time, stop_time, samp_period, frag_period, time;
    struct player_params *player_parms;
    struct sample samp[MAX_STREAMS][2];

    if (size > SND_BUFFER_SIZE) {
	error("PCM: size %i is too much\n", size);
	return 0;
    }
    now = GETusTIME(0);
    start_time = players.clocked.time;
    frag_period = pcm_frag_period(size, params);
    stop_time = start_time + frag_period;
    if (start_time == 0 ||
	start_time < now - BUFFER_DELAY * 3
	|| stop_time > now - BUFFER_DELAY / 2) {
	if (start_time != 0)
	    error("PCM: \"%s\" out of sync\n",
		  players.clocked.player.name);
	start_time = now - BUFFER_DELAY * 2;
	stop_time = start_time + frag_period;
    }
    S_printf("PCM: going to process %zu bytes for %i players (st=%f stp=%f d=%f)\n",
	 size, players.num_clocked + players.num_unclocked, start_time,
	 stop_time, now - start_time);

    for (i = 0; i < players.num_clocked + players.num_unclocked; i++) {
	player_parms = (i < players.num_clocked ?
			params : &players.unclocked[i - players.num_clocked].
			params);
	samp_period =
	    pcm_samp_period(player_parms->rate, player_parms->channels);
	time = start_time;
	samp_sz = pcm_format_size(player_parms->format);
	memset(idxs, 0, sizeof(idxs));
	pcm.out_buf.idx = 0;

	while (time < stop_time - (samp_period / 10.0)) {
	    have_data = pcm_get_samples(time, samp, 0, idxs);
	    if (!have_data && i >= players.num_clocked)
		break;
	    if (pcm.out_buf.idx + samp_sz * player_parms->channels >
		RAW_BUFFER_SIZE) {
		error("PCM: output buffer overflowed\n");
		break;
	    }
	    pcm_process_channels(samp, player_parms->channels);
	    pcm_mix_samples(samp, pcm.out_buf.data + pcm.out_buf.idx,
			    player_parms->channels, player_parms->format);
	    pcm.out_buf.idx += samp_sz * player_parms->channels;
	    time += samp_period * player_parms->channels;
	}

	/* feed the data to player */
	if (i < players.num_clocked) {
	    if (data)
		memcpy(data, pcm.out_buf.data, pcm.out_buf.idx);
	    ret = pcm.out_buf.idx;
	} else {
	    int j = i - players.num_clocked;
	    if (players.unclocked[j].opened && pcm.out_buf.idx) {
		S_printf("PCM: going to write %i bytes to player %i (%s)\n",
		     pcm.out_buf.idx, i, players.unclocked[j].player.name);
		players.unclocked[j].player.write(pcm.out_buf.data,
						  pcm.out_buf.idx);
	    }
	}
    }

    players.clocked.time = stop_time;

    /* remove processed samples from input buffers (last sample stays) */
    pcm_get_samples(stop_time, NULL, 1, NULL);
    for (i = 0; i < pcm.num_streams; i++) {
	if (pcm.stream[i].state == SNDBUF_STATE_INACTIVE)
	    continue;
	S_printf("PCM: stream %i fillup2: %i\n", i,
		 rng_count(&pcm.stream[i].buffer));
	pcm_handle_get(i, now + frag_period - BUFFER_DELAY);
    }

    if (!count_active_streams() && pcm.playing)
	pcm_stop_output();

    if (ret != size)
	error("PCM: requested=%i prepared=%i\n", size, ret);
    return ret;
}

int pcm_register_clocked_player(struct clocked_player player,
				struct player_callbacks *callbacks)
{
    S_printf("PCM: registering clocked player: %s\n", player.name);
    if (players.num_clocked) {
	error("PCM: attempt to register more than one clocked player\n");
	return 0;
    }
    players.clocked.player = player;
    callbacks->get_data = pcm_data_get;
    players.num_clocked++;
    return 1;
}

int pcm_register_unclocked_player(struct unclocked_player player)
{
    S_printf("PCM: registering unclocked player: %s\n", player.name);
    if (players.num_unclocked >= MAX_PLAYERS) {
	error("PCM: attempt to register more than %i unclocked player\n",
	      MAX_PLAYERS);
	return 0;
    }
    players.unclocked[players.num_unclocked].player = player;
    players.num_unclocked++;
    return 1;
}

void pcm_timer(void)
{
    int i;
    for (i = 0; i < players.num_unclocked; i++)
	if (players.unclocked[i].player.timer)
	    players.unclocked[i].player.timer();
    if (players.clocked.player.timer)
	players.clocked.player.timer();
}

void pcm_reset(void)
{
    int i;
    S_printf("PCM: reset\n");
    if (pcm.playing)
	pcm_stop_output();
    for (i = 0; i < pcm.num_streams; i++)
	pcm_reset_stream(i);
}

void pcm_done(void)
{
    int i;
    for (i = 0; i < pcm.num_streams; i++)
	if (pcm.stream[i].state == SNDBUF_STATE_PLAYING)
	    pcm_flush(i);
    if (pcm.playing)
	pcm_stop_output();
    players.clocked.player.close();
    for (i = 0; i < players.num_unclocked; i++)
	players.unclocked[i].player.close();
    for (i = 0; i < pcm.num_streams; i++)
	rng_destroy(&pcm.stream[i].buffer);
}
