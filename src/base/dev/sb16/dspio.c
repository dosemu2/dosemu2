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
 * Purpose:
 *   DSP I/O layer, DMA and DAC handling. Also MIDI sometimes.
 *   Currently used by SB16, but may be used with anything, e.g. GUS.
 *
 * Author: Stas Sergeev.
 *
 */

#include "emu.h"
#include "timers.h"
#include "sound/sound.h"
#include "sound/sndpcm.h"
#include "sound/midi.h"
#include "adlib.h"
#include "dma.h"
#include "sb16.h"
#include "dspio.h"
#include <string.h>
#include <stdlib.h>

#define DAC_BASE_FREQ 5625

struct dspio_dma {
    int running;
    int num;
    int broken_hdma;
    int rate;
    int is16bit;
    int stereo;
    int samp_signed;
    int input;
    int silence;
};

struct dspio_state {
    double input_time_cur, output_time_cur, midi_time_cur;
    int dma_strm, dac_strm;
    int input_running, output_running, dac_running, speaker;
    struct dspio_dma dma;
};

#define DSPIO ((struct dspio_state *)dspio)

static int dma8_get_format(int is_signed)
{
    return is_signed ? PCM_FORMAT_S8 : PCM_FORMAT_U8;
}

static int dma16_get_format(int is_signed)
{
    return is_signed ? PCM_FORMAT_S16_LE : PCM_FORMAT_U16_LE;
}

static int dma_get_format(int is_16, int is_signed)
{
    return is_16 ? dma16_get_format(is_signed) :
	dma8_get_format(is_signed);
}

static void dma_get_silence(int is_signed, int is16bit, void *ptr)
{
    if (is16bit) {
	Bit16u *tmp16 = ptr;
	*tmp16 = is_signed ? 0 : 0x8000;
    } else {
	Bit8u *tmp8 = ptr;
	*tmp8 = is_signed ? 0 : 0x80;
    }
}

void dspio_toggle_speaker(void *dspio, int on)
{
    if (!on && DSPIO->speaker) {
	if (DSPIO->output_running) {
	    pcm_flush(DSPIO->dma_strm);
	}
	if (DSPIO->dac_running) {
	    pcm_flush(DSPIO->dac_strm);
	    DSPIO->dac_running = 0;
	}
    }
    DSPIO->speaker = on;
}

int dspio_get_speaker_state(void *dspio)
{
    return DSPIO->speaker;
}

void *dspio_init(void)
{
    struct dspio_state *dspio;
    dspio = malloc(sizeof(struct dspio_state));
    if (!dspio)
	return NULL;
    pcm_init();
    dspio->dac_strm = pcm_allocate_stream(1, "SB DAC");
    pcm_set_flag(dspio->dac_strm, PCM_FLAG_RAW);
    dspio->dma_strm = pcm_allocate_stream(2, "SB DMA");

    adlib_init();
    midi_init();

    return dspio;
}

void dspio_reset(void *dspio)
{
    memset(&DSPIO->dma, 0, sizeof(struct dspio_dma));
    DSPIO->input_running =
	DSPIO->output_running = DSPIO->dac_running = DSPIO->speaker = 0;

    pcm_reset();
    midi_reset();
}

void dspio_done(void *dspio)
{
    pcm_done();
    midi_done();
    free(dspio);
}

void dspio_stop_midi(void *dspio)
{
    DSPIO->midi_time_cur = GETusTIME(0);
    midi_stop();
}

Bit32u dspio_get_midi_in_time(void *dspio)
{
    Bit32u delta = GETusTIME(0) - DSPIO->midi_time_cur;
    S_printf("SB: midi clock, delta=%i\n", delta);
    return delta;
}

static void dspio_start_output(struct dspio_state *state)
{
    if (state->output_running)
	return;
    S_printf("SB: starting output\n");
    /* We would need real time here, but the HACK is to use stream time instead.
     * That compensates the hack of dspio_process_dma() */
    state->output_time_cur = pcm_get_stream_time(state->dma_strm);
    state->output_running = 1;
}

static void dspio_stop_output(struct dspio_state *state)
{
    if (!state->output_running)
	return;
    S_printf("SB: stopping output\n");
    if (state->speaker)
	pcm_flush(state->dma_strm);
    state->output_running = 0;
}

static void dspio_start_input(struct dspio_state *state)
{
    if (state->input_running)
	return;
    S_printf("SB: starting input\n");
    /* TODO */
    state->input_time_cur = GETusTIME(0);
    state->input_running = 1;
}

static void dspio_stop_input(struct dspio_state *state)
{
    if (!state->input_running)
	return;
    S_printf("SB: stopping input\n");
    /* TODO */
    state->input_running = 0;
}

static void dspio_run_dma(struct dspio_dma *dma)
{
    Bit8u dma_buf[2];
    dma_get_silence(dma->samp_signed, dma->is16bit, dma_buf);
    if (!dma->silence) {
	if (dma->input)
	    sb_get_dma_data(dma_buf, dma->is16bit);
	if (dma_pulse_DRQ(dma->num, dma_buf) != DMA_DACK)
	    S_printf("SB: DMA %i doesn't DACK!\n", dma->num);
	if (dma->broken_hdma)
	    dma_pulse_DRQ(dma->num, dma_buf + 1);
    }
    if (!dma->input)
	sb_put_dma_data(dma_buf, dma->is16bit);

    sb_handle_dma();
}

static void get_dma_params(struct dspio_dma *dma)
{
    int dma_16bit = sb_dma_16bit();
    int dma_num = dma_16bit ? sb_get_hdma_num() : sb_get_dma_num();
    int broken_hdma = (dma_16bit && dma_num == -1);
    if (broken_hdma) {
	dma_num = sb_get_dma_num();
	S_printf("SB: Warning: HDMA is broken, using 8-bit DMA channel %i\n",
	     dma_num);
    }

    dma->num = dma_num;
    dma->is16bit = dma_16bit;
    dma->broken_hdma = broken_hdma;
    dma->rate = sb_get_dma_sampling_rate();
    dma->stereo = sb_dma_samp_stereo();
    dma->samp_signed = sb_dma_samp_signed();
    dma->input = sb_dma_input();
    dma->silence = sb_dma_silence();
}

static int dspio_fill_output(struct dspio_state *state)
{
    int dma_cnt = 0;
    while (state->dma.running && !sb_output_fifo_filled()) {
	dspio_run_dma(&state->dma);
	dma_cnt++;
    }
    if (!state->output_running && !sb_output_fifo_empty())
	dspio_start_output(state);
    return dma_cnt;
}

static int dspio_drain_input(struct dspio_state *state)
{
    int dma_cnt = 0;
    while (state->dma.running && !sb_input_fifo_empty()) {
	dspio_run_dma(&state->dma);
	dma_cnt++;
    }
    return dma_cnt;
}

void dspio_start_dma(void *dspio)
{
    int dma_cnt = 0;
    DSPIO->dma.running = 1;
    get_dma_params(&DSPIO->dma);

    if (DSPIO->dma.input) {
	dspio_start_input(DSPIO);
    } else {
	dma_cnt = dspio_fill_output(DSPIO);
	if (DSPIO->dma.running && sb_output_fifo_filled())
	    S_printf("SB: Output filled, processed %i DMA cycles\n",
		     dma_cnt);
	else
	    S_printf("SB: Output fillup incomplete (%i %i %i)\n",
		     DSPIO->dma.running, DSPIO->output_running, dma_cnt);
    }
}

void dspio_stop_dma(void *dspio)
{
    dspio_stop_input(DSPIO);
    DSPIO->dma.running = 0;
}

static void dspio_process_dma(struct dspio_state *state)
{
    int dma_cnt, fifo_cnt;
    unsigned long long time_dst;
    double period;
    Bit16u buf;

    dma_cnt = fifo_cnt = 0;

    time_dst = GETusTIME(0);
    if (state->dma.running) {
	state->dma.stereo = sb_dma_samp_stereo();
	state->dma.rate = sb_get_dma_sampling_rate();
    }
    period = pcm_samp_period(state->dma.rate, state->dma.stereo + 1);

    while (state->output_running && (state->output_time_cur <= time_dst ||
				     fifo_cnt % (state->dma.stereo + 1))) {
	if (state->dma.running) {
	    dspio_run_dma(&state->dma);
	    dma_cnt++;
	}
	if (sb_get_output_sample(&buf, state->dma.is16bit)) {
	    if (state->speaker) {
		pcm_write_samples(&buf, 1 << state->dma.is16bit,
				  state->dma.rate,
				  dma_get_format(state->dma.is16bit,
						 state->dma.samp_signed),
				  state->dma_strm);
		if (!state->dma.stereo)
		    pcm_write_samples(&buf, 1 << state->dma.is16bit,
				      state->dma.rate,
				      dma_get_format(state->dma.is16bit,
						     state->dma.
						     samp_signed),
				      state->dma_strm);
	    }
	    state->output_time_cur += period;
	    fifo_cnt++;
	} else {
	    if (!sb_dma_active()) {
		dspio_stop_output(state);
	    } else {
		if (debug_level('S') > 7)
		    S_printf("SB: Output FIFO exhausted while DMA is still active (ol=%f)\n",
			 time_dst - state->output_time_cur);
		if (state->dma.running) {
		    error("SB: Output FIFO exhausted while DMA is running\n");
		} else {
		    /* DMA is active but currently not running and the FIFO is
		     * already exhausted. Normally we should flush the channel
		     * and stop the output timing.
		     * HACK: try to not flush the channel for as long as possible
		     * in a hope the PCM buffers are large enough to hold till
		     * the DMA is restarted. */
		    pcm_set_mode(state->dma_strm, PCM_MODE_POST);
		    /* awake dosemu */
		    reset_idle(0);
		}
	    }
	    break;
	}
    }
    if (state->dma.running && state->output_time_cur > time_dst - 1)
	pcm_set_mode(state->dma_strm, PCM_MODE_NORMAL);

    while (state->input_running && (state->input_time_cur <= time_dst ||
				    fifo_cnt % (state->dma.stereo + 1))) {
	dma_get_silence(state->dma.samp_signed, state->dma.is16bit, &buf);
	//if (!state->speaker)  /* TODO: input */
	sb_put_input_sample(&buf, state->dma.is16bit);
	if (state->dma.running) {
	    dspio_run_dma(&state->dma);
	    dma_cnt++;
	}
	state->input_time_cur += period;
	fifo_cnt++;
    }

    if (state->dma.running)
	dma_cnt += state->dma.input ? dspio_drain_input(state) :
	    dspio_fill_output(state);

    if (fifo_cnt || dma_cnt)
	S_printf("SB: Processed %i FIFO, %i DMA, or=%i dr=%i time=%lli period=%f\n",
	     fifo_cnt, dma_cnt, state->output_running, state->dma.running,
	     time_dst, period);
}

static void dspio_process_midi(void)
{
    Bit8u data;
    /* no timing for now */
    while (!sb_midi_output_empty()) {
	sb_get_midi_data(&data);
	midi_write(data);
    }

    while (midi_get_data_byte(&data)) {
	sb_put_midi_data(data);
    }
}

void dspio_timer(void *dspio)
{
    adlib_timer();
    dspio_process_dma(DSPIO);
    dspio_process_midi();
    pcm_timer();
}

void dspio_write_dac(void *dspio, Bit8u samp)
{
    if (DSPIO->speaker) {
	DSPIO->dac_running = 1;
	pcm_write_samples(&samp, 1, DAC_BASE_FREQ, PCM_FORMAT_U8,
			  DSPIO->dac_strm);
    }
}
