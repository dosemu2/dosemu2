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
#include "sound/midi.h"
#include "sound.h"
#include "adlib.h"
#include "dma.h"
#include "sb16.h"
#include "dspio.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#define DAC_BASE_FREQ 5625
#define PCM_MAX_BUF 512

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
    int dsp_fifo_enabled;
    hitimer_t time_cur;
};

/*
 * MPU-401 interface state
 */

#define DEBUG_MPU401 0	/* Deep MPU401 state debug */
#define TRACE_MPU401 0	/* Trace MPU401 calls */
#define TIME_MPU401 0   /* Output timing information with all NOTE ON commands */

#define MPU401_VERSION 0x15
#define MPU401_REVISION 0x01
#define MPU401_TIMECONSTANT (60000000/1000.0f)
#define MPU401_RESETBUSY 27.0f

#define MPU401_EVENT_MAX 3
#define MPU401_EVENT_EOIHANDLER 0
#define MPU401_EVENT_SEQUENCER 1
#define MPU401_EVENT_RESETDONE 2

typedef enum { M_UART, M_INTELLIGENT } MpuMode;
typedef enum { T_OVERFLOW, T_MARK, T_MIDI_SYS, T_MIDI_NORM, T_COMMAND } MpuDataType;

/* Messages sent to MPU-401 from host */
#define MSG_EOX 0xf7
#define MSG_OVERFLOW 0xf8
#define MSG_MARK 0xfc

/* Messages sent to host from MPU-401 */
#define MSG_MPU_OVERFLOW 0xf8
#define MSG_MPU_COMMAND_REQ 0xf9
#define MSG_MPU_END 0xfc
#define MSG_MPU_CLOCK 0xfd
#define MSG_MPU_ACK 0xfe

struct mpu {
    uint8_t newest_data;
    boolean intelligent;
    MpuMode mode, prev_mode;
    uint8_t irq;
    int deferred_ticks;
    struct track {
	uint8_t counter;
	uint8_t value[8], sys_val;
	uint8_t vlength, length;
	MpuDataType type;
    } playbuf[8], condbuf;
    struct {
	boolean conductor, cond_req, cond_set, block_ack;
	boolean playing, reset;
	boolean wsd, wsm, wsd_start;
	boolean run_irq, irq_pending;
	boolean send_now;
	boolean eoi_scheduled;
	int data_onoff;
	int command_byte, cmd_pending_reset;
	uint8_t tmask, cmask, amask;
	uint16_t midi_mask;
	uint16_t req_mask;
	uint8_t channel, old_chan;
	int next_track;
    } state;
    struct {
	uint8_t timebase, old_timebase;
	uint8_t tempo, old_tempo;
	uint8_t tempo_rel, old_tempo_rel;
	uint8_t tempo_grad;
	uint8_t cth_rate, cth_counter;
	boolean clock_to_host, cth_active;
    } clock;
    struct event {
	boolean enabled;
	int us_interval;
	int us_remaining;
	int last_tick;
    } events[MPU401_EVENT_MAX];
};

struct dspio_state {
    double input_time_cur, midi_time_cur;
    int dma_strm, dac_strm;
    int input_running:1, output_running:1, dac_running:1, speaker:1;
    int pcm_input_running:1, lin_input_running:1, mic_input_running:1;
    int i_handle, i_started;
#define DSP_FIFO_SIZE 64
    struct rng_s fifo_in;
    struct rng_s fifo_out;
#define DSP_OUT_FIFO_TRIGGER 32
#define DSP_IN_FIFO_TRIGGER 32
#define MIDI_FIFO_SIZE 32
    struct rng_s midi_fifo_in;
    struct rng_s midi_fifo_out;
    struct mpu mpu;
    void (*trigger_mpu_irq)(boolean);
    struct dspio_dma dma;
};

#define DSPIO ((struct dspio_state *)dspio)

static void dspio_mpu401_reset(struct dspio_state *dspio);

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
	if (DSPIO->dac_running) {
	    pcm_flush(DSPIO->dac_strm);
	    DSPIO->dac_running = 0;
	}
	/* we don't flush PCM stream here because DSP uses PCM layer
	 * for timing, and timing is needed even when speaker is disabled... */
    }
    DSPIO->speaker = on;
}

int dspio_get_speaker_state(void *dspio)
{
    return DSPIO->speaker;
}

void dspio_write_midi(void *dspio, Bit8u value)
{
#if DEBUG_MPU401
    struct timeval tv;
    gettimeofday(&tv, NULL);
    S_printf("dspio_write_midi(%02x) %d.%d\n", value, tv.tv_sec, tv.tv_usec);
#endif
    rng_put(&DSPIO->midi_fifo_out, &value);
    run_sb();
}

void run_sound(void)
{
    if (!config.sound)
	return;
    dspio_run_synth();
    pcm_timer();
}

static int dspio_out_fifo_len(struct dspio_dma *dma)
{
    return dma->dsp_fifo_enabled ? DSP_OUT_FIFO_TRIGGER : 2;
}

static int dspio_in_fifo_len(struct dspio_dma *dma)
{
    return dma->dsp_fifo_enabled ? DSP_IN_FIFO_TRIGGER : 2;
}

static int dspio_output_fifo_filled(struct dspio_state *state)
{
    return rng_count(&state->fifo_out) >= dspio_out_fifo_len(&state->dma);
}

static int dspio_input_fifo_filled(struct dspio_state *state)
{
    return rng_count(&state->fifo_in) >= dspio_in_fifo_len(&state->dma);
}

static int dspio_input_fifo_empty(struct dspio_state *state)
{
    return !rng_count(&state->fifo_in);
}

static int dspio_midi_output_empty(struct dspio_state *state)
{
    return !rng_count(&state->midi_fifo_out);
}

static Bit8u dspio_get_midi_data(struct dspio_state *state)
{
    Bit8u val;
    assert(rng_get(&state->midi_fifo_out, &val) == 1);
    return val;
}

Bit8u dspio_get_midi_in_byte(void *dspio)
{
    Bit8u val;
    assert(rng_get(&DSPIO->midi_fifo_in, &val) == 1);
    return val;
}

void dspio_put_midi_in_byte(void *dspio, Bit8u val)
{
    S_printf("PUT MIDI BYTE %X\n", val);
    rng_put_const(&DSPIO->midi_fifo_in, val);
    S_printf("FILLUP now %d\n", dspio_get_midi_in_fillup(dspio));
}

int dspio_get_midi_in_fillup(void *dspio)
{
    return rng_count(&DSPIO->midi_fifo_in);
}

void dspio_clear_midi_in_fifo(void *dspio)
{
    rng_clear(&DSPIO->midi_fifo_in);
}

static int dspio_get_dma_data(struct dspio_state *state, void *ptr, int is16bit)
{
    if (sb_get_dma_data(ptr, is16bit))
	return 1;
    if (rng_count(&state->fifo_in)) {
	if (is16bit) {
	    rng_get(&state->fifo_in, ptr);
	} else {
	    Bit16u tmp;
	    rng_get(&state->fifo_in, &tmp);
	    *(Bit8u *) ptr = tmp;
	}
	return 1;
    }
    error("SB: input fifo empty\n");
    return 0;
}

static void dspio_put_dma_data(struct dspio_state *state, void *ptr, int is16bit)
{
    if (dspio_output_fifo_filled(state)) {
	error("SB: output fifo overflow\n");
	return;
    }
    if (is16bit) {
	rng_put(&state->fifo_out, ptr);
    } else {
	Bit16u tmp = *(Bit8u *) ptr;
	rng_put(&state->fifo_out, &tmp);
    }
}

static int dspio_get_output_sample(struct dspio_state *state, void *ptr,
	int is16bit)
{
    if (rng_count(&state->fifo_out)) {
	if (is16bit) {
	    rng_get(&state->fifo_out, ptr);
	} else {
	    Bit16u tmp;
	    rng_get(&state->fifo_out, &tmp);
	    *(Bit8u *) ptr = tmp;
	}
	return 1;
    }
    return 0;
}

static int dspio_put_input_sample(struct dspio_state *state, void *ptr,
	int is16bit)
{
    int ret;
    if (!sb_input_enabled())
	return 0;
    if (dspio_input_fifo_filled(state)) {
	S_printf("SB: ERROR: input fifo overflow\n");
	return 0;
    }
    if (is16bit) {
	ret = rng_put(&state->fifo_in, ptr);
    } else {
	Bit16u tmp = *(Bit8u *) ptr;
	ret = rng_put(&state->fifo_in, &tmp);
    }
    return ret;
}

void dspio_clear_fifos(void *dspio)
{
    rng_clear(&DSPIO->fifo_in);
    rng_clear(&DSPIO->fifo_out);
    DSPIO->dma.dsp_fifo_enabled = 1;
}

static void dspio_i_start(void *arg)
{
    struct dspio_state *state = arg;
    state->i_started = 1;
}

static void dspio_i_stop(void *arg)
{
    struct dspio_state *state = arg;
    state->i_started = 0;
}

static const struct pcm_player player = {
    .name = "SB REC",
    .start = dspio_i_start,
    .stop = dspio_i_stop,
    .id = PCM_ID_R,
    .flags = PCM_F_PASSTHRU,
};

static double dspio_get_volume(int id, int chan_dst, int chan_src, void *arg);
int dspio_is_connected(int id, void *arg);

void *dspio_init(void (*trigger_mpu_irq)(boolean))
{
    struct dspio_state *state;
    state = malloc(sizeof(struct dspio_state));
    if (!state)
	return NULL;
    memset(&state->dma, 0, sizeof(struct dspio_dma));
    state->input_running = state->pcm_input_running =
	state->lin_input_running = state->mic_input_running =
	state->output_running = state->dac_running = state->speaker = 0;
    state->dma.dsp_fifo_enabled = 1;

    rng_init(&state->fifo_in, DSP_FIFO_SIZE, 2);
    rng_init(&state->fifo_out, DSP_FIFO_SIZE, 2);
    rng_init(&state->midi_fifo_in, MIDI_FIFO_SIZE, 1);
    rng_init(&state->midi_fifo_out, MIDI_FIFO_SIZE, 1);

    state->i_handle = pcm_register_player(&player, state);
    pcm_init();

    pcm_set_volume_cb(dspio_get_volume);
    pcm_set_connected_cb(dspio_is_connected);
    state->dac_strm = pcm_allocate_stream(1, "SB DAC", (void*)MC_VOICE);
    pcm_set_flag(state->dac_strm, PCM_FLAG_RAW);
    state->dma_strm = pcm_allocate_stream(2, "SB DMA", (void*)MC_VOICE);
    pcm_set_flag(state->dma_strm, PCM_FLAG_SLTS);

    state->trigger_mpu_irq = trigger_mpu_irq;
    /* XXX should these be configurable? */
    state->mpu.mode = M_UART;
    state->mpu.intelligent = TRUE;	//Default is on
    if (state->mpu.intelligent)
	dspio_mpu401_reset(state);
    midi_init();

    return state;
}

void dspio_reset(void *dspio)
{
}

void dspio_done(void *dspio)
{
    midi_done();
    /* shutdown midi before pcm as midi may use pcm */
    pcm_done();

    rng_destroy(&DSPIO->fifo_in);
    rng_destroy(&DSPIO->fifo_out);
    rng_destroy(&DSPIO->midi_fifo_in);
    rng_destroy(&DSPIO->midi_fifo_out);

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
    pcm_prepare_stream(state->dma_strm);
    state->output_running = 1;
}

static void dspio_stop_output(struct dspio_state *state)
{
    if (!state->output_running)
	return;
    S_printf("SB: stopping output\n");
    pcm_flush(state->dma_strm);
    state->output_running = 0;
}

static void dspio_start_input(struct dspio_state *state)
{
    if (state->input_running)
	return;
    S_printf("SB: starting input\n");
    state->input_time_cur = GETusTIME(0);
    state->input_running = 1;
    if (!state->dma.rate) {
	S_printf("SB: not starting recorder\n");
	return;
    }
    if (!state->pcm_input_running) {
	pcm_reset_player(state->i_handle);
	state->pcm_input_running = 1;
    }
}

static void dspio_stop_input(struct dspio_state *state)
{
    if (!state->input_running)
	return;
    S_printf("SB: stopping input\n");
    state->input_running = 0;
    if (!state->dma.rate) {
	S_printf("SB: not stopping recorder\n");
	return;
    }
    if (!sb_dma_active())
	state->pcm_input_running = 0;
}

int dspio_input_enable(void *dspio, enum MixChan mc)
{
    struct dspio_state *state = dspio;
    switch (mc) {
    case MC_LINE:
	if (state->lin_input_running)
	    return 0;
	pcm_start_input((void *)MC_LINE);
	state->lin_input_running = 1;
	S_printf("SB: enabled LINE\n");
	break;
    case MC_MIC:
	if (state->mic_input_running)
	    return 0;
	pcm_start_input((void *)MC_MIC);
	state->mic_input_running = 1;
	S_printf("SB: enabled MIC\n");
	break;
    default:
	return 0;
    }
    return 1;
}

int dspio_input_disable(void *dspio, enum MixChan mc)
{
    struct dspio_state *state = dspio;
    switch (mc) {
    case MC_LINE:
	if (!state->lin_input_running)
	    return 0;
	pcm_stop_input((void *)MC_LINE);
	state->lin_input_running = 0;
	S_printf("SB: disabled LINE\n");
	break;
    case MC_MIC:
	if (!state->mic_input_running)
	    return 0;
	pcm_stop_input((void *)MC_MIC);
	state->mic_input_running = 0;
	S_printf("SB: disabled MIC\n");
	break;
    default:
	return 0;
    }
    return 1;
}

static int do_run_dma(struct dspio_state *state)
{
    Bit8u dma_buf[2];
    struct dspio_dma *dma = &state->dma;

    dma_get_silence(dma->samp_signed, dma->is16bit, dma_buf);
    if (!dma->silence) {
	if (dma->input)
	    dspio_get_dma_data(state, dma_buf, dma->is16bit);
	if (dma_pulse_DRQ(dma->num, dma_buf) != DMA_DACK) {
	    S_printf("SB: DMA %i doesn't DACK!\n", dma->num);
	    return 0;
	}
	if (dma->broken_hdma) {
	    if (dma_pulse_DRQ(dma->num, dma_buf + 1) != DMA_DACK) {
		S_printf("SB: DMA (broken) %i doesn't DACK!\n", dma->num);
		return 0;
	    }
	}
    }
    if (!dma->input)
	dspio_put_dma_data(state, dma_buf, dma->is16bit);
    return 1;
}

static int dspio_run_dma(struct dspio_state *state)
{
#define DMA_TIMEOUT_US 100000
    int ret;
    struct dspio_dma *dma = &state->dma;
    hitimer_t now = GETusTIME(0);
    sb_dma_processing();	// notify that DMA busy
    ret = do_run_dma(state);
    if (ret) {
	sb_handle_dma();
	dma->time_cur = now;
    } else if (now - dma->time_cur > DMA_TIMEOUT_US) {
	S_printf("SB: Warning: DMA busy for too long, releasing\n");
//	error("SB: DMA timeout\n");
	sb_handle_dma_timeout();
    }
    return ret;
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
    dma->dsp_fifo_enabled = sb_fifo_enabled();
}

static int dspio_fill_output(struct dspio_state *state)
{
    int dma_cnt = 0;
    while (state->dma.running && !dspio_output_fifo_filled(state)) {
	if (!dspio_run_dma(state))
	    break;
	dma_cnt++;
    }
#if 0
    if (!state->output_running && !sb_output_fifo_empty())
#else
    /* incomplete fifo needs a timeout, so lets not deal with it at all.
     * Instead, deal with the filled fifo only. */
    if (dspio_output_fifo_filled(state))
#endif
	dspio_start_output(state);
    return dma_cnt;
}

static int dspio_drain_input(struct dspio_state *state)
{
    int dma_cnt = 0;
    while (state->dma.running && !dspio_input_fifo_empty(state)) {
	if (!dspio_run_dma(state))
	    break;
	dma_cnt++;
    }
    return dma_cnt;
}

void dspio_start_dma(void *dspio)
{
    int dma_cnt = 0;
    DSPIO->dma.running = 1;
    DSPIO->dma.time_cur = GETusTIME(0);
    get_dma_params(&DSPIO->dma);

    if (DSPIO->dma.input) {
	dspio_start_input(DSPIO);
    } else {
	dma_cnt = dspio_fill_output(DSPIO);
	if (DSPIO->dma.running && dspio_output_fifo_filled(DSPIO))
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

static int calc_nframes(struct dspio_state *state,
	hitimer_t time_beg, hitimer_t time_dst)
{
    int nfr;
    if (state->dma.rate) {
	nfr = (time_dst - time_beg) / pcm_frame_period_us(state->dma.rate) + 1;
	if (nfr < 0)	// happens because of get_stream_time() hack
	    nfr = 0;
	if (nfr > PCM_MAX_BUF)
	    nfr = PCM_MAX_BUF;
    } else {
	nfr = 1;
    }
    return nfr;
}

static void dspio_process_dma(struct dspio_state *state)
{
    int dma_cnt, nfr, in_fifo_cnt, out_fifo_cnt, i, j, tlocked;
    unsigned long long time_dst;
    double output_time_cur;
    sndbuf_t buf[PCM_MAX_BUF][SNDBUF_CHANS];
    static int warned;

    dma_cnt = in_fifo_cnt = out_fifo_cnt = 0;

    if (state->dma.running) {
	state->dma.stereo = sb_dma_samp_stereo();
	state->dma.rate = sb_get_dma_sampling_rate();
	state->dma.samp_signed = sb_dma_samp_signed();
	state->dma.dsp_fifo_enabled = sb_fifo_enabled();
	dma_cnt += state->dma.input ? dspio_drain_input(state) :
	    dspio_fill_output(state);
    }

    if (!state->output_running && !state->input_running)
	return;

    time_dst = GETusTIME(0);
    if (state->output_running) {
	output_time_cur = pcm_time_lock(state->dma_strm);
	tlocked = 1;
	nfr = calc_nframes(state, output_time_cur, time_dst);
    } else {
	nfr = 0;
	tlocked = 0;
    }
    for (i = 0; i < nfr; i++) {
	for (j = 0; j < state->dma.stereo + 1; j++) {
	    if (state->dma.running && !dspio_output_fifo_filled(state)) {
		if (!dspio_run_dma(state))
		    break;
		dma_cnt++;
	    }
	    if (!dspio_get_output_sample(state, &buf[i][j],
		    state->dma.is16bit)) {
		if (out_fifo_cnt && debug_level('S') >= 5)
		    S_printf("SB: no output samples\n");
		break;
	    }
#if 0
	    /* if speaker disabled, overwrite DMA data with silence */
	    /* on SB16 is not used */
	    if (!state->speaker)
		dma_get_silence(state->dma.samp_signed,
			state->dma.is16bit, &buf[i][j]);
#endif
	}
	if (j != state->dma.stereo + 1)
	    break;
	out_fifo_cnt++;
    }
    if (out_fifo_cnt && state->dma.rate) {
	pcm_write_interleaved(buf, out_fifo_cnt, state->dma.rate,
			  pcm_get_format(state->dma.is16bit,
					 state->dma.samp_signed),
			  state->dma.stereo + 1, state->dma_strm);
	output_time_cur = pcm_get_stream_time(state->dma_strm);
	if (state->dma.running && output_time_cur > time_dst - 1) {
	    pcm_clear_flag(state->dma_strm, PCM_FLAG_POST);
	    warned = 0;
	}
    }
    if (out_fifo_cnt < nfr) {
	/* not enough samples, see why */
	if (!sb_dma_active()) {
	    dspio_stop_output(state);
	} else {
	    if (nfr && (!warned || debug_level('S') >= 9)) {
		S_printf("SB: Output FIFO exhausted while DMA is still active (ol=%f)\n",
			 time_dst - output_time_cur);
		warned = 1;
	    }
	    if (state->dma.running)
		S_printf("SB: Output FIFO exhausted while DMA is running (no DACK?)\n");
	    /* DMA is active but currently not running and the FIFO is
	     * already exhausted. Normally we should flush the channel
	     * and stop the output timing.
	     * HACK: try to not flush the channel for as long as possible
	     * in a hope the PCM buffers are large enough to hold till
	     * the DMA is restarted. */
	    pcm_set_flag(state->dma_strm, PCM_FLAG_POST);
	    /* awake dosemu */
	    reset_idle(0);
	}
    }
    if (tlocked)
	pcm_time_unlock(state->dma_strm);

    /* TODO: sync also input time with PCM? */
    if (state->input_running)
	nfr = calc_nframes(state, state->input_time_cur, time_dst);
    else
	nfr = 0;
    if (nfr && state->i_started && sb_input_enabled()) {
	struct player_params params;
	params.rate = state->dma.rate;
	params.channels = state->dma.stereo + 1;
	params.format = pcm_get_format(state->dma.is16bit,
		state->dma.samp_signed);
	params.handle = state->i_handle;
	nfr = pcm_data_get_interleaved(buf, nfr, &params);
    }
    if (!state->i_started) {
	for (i = 0; i < nfr; i++) {
	    for (j = 0; j < state->dma.stereo + 1; j++)
		dma_get_silence(state->dma.samp_signed,
			state->dma.is16bit, &buf[i][j]);
	}
    }
    for (i = 0; i < nfr; i++) {
	for (j = 0; j < state->dma.stereo + 1; j++) {
	    if (sb_input_enabled()) {
		if (!dspio_put_input_sample(state, &buf[i][j],
			state->dma.is16bit))
		    break;
	    }
	}
	if (j == state->dma.stereo + 1)
	    in_fifo_cnt++;
	for (j = 0; j < state->dma.stereo + 1; j++) {
	    if (state->dma.running) {
		if (!dspio_run_dma(state))
		    break;
		dma_cnt++;
	    }
	}
	if (!state->input_running || (j != state->dma.stereo + 1))
	    break;
    }
    if (in_fifo_cnt) {
	if (state->dma.rate) {
	    state->input_time_cur += in_fifo_cnt *
		    pcm_frame_period_us(state->dma.rate);
	} else {
	    state->input_time_cur = time_dst;
	}
    }

    if (debug_level('S') >= 7 && (in_fifo_cnt || out_fifo_cnt || dma_cnt))
	S_printf("SB: Processed %i %i FIFO, %i DMA, or=%i dr=%i\n",
	     in_fifo_cnt, out_fifo_cnt, dma_cnt, state->output_running, state->dma.running);
}

static void dspio_mpu401_queue_databyte(struct dspio_state *dspio, Bit8u data)
{
#if TRACE_MPU401
    S_printf("MPU401: dspio_mpu401_queue_databyte(%02x)\n", data);
#endif
    /* Are we blocking the next ACK? */
    if (dspio->mpu.state.block_ack && data == MSG_MPU_ACK) {
#if DEBUG_MPU401
	S_printf("MPU401: Command's ACK blocked due to reset\n");
#endif
	dspio->mpu.state.block_ack = FALSE;
	return;
    }

    /* Assert IRQ on DSR transition to active. */
    if (!dspio_get_midi_in_fillup(dspio)) {
	if (dspio->mpu.intelligent)
	    dspio->mpu.state.irq_pending = TRUE;
	DSPIO->trigger_mpu_irq(TRUE);
    }
    dspio_put_midi_in_byte(dspio, data);
    /* Wake up dosemu to process DOS program's ISR. */
    /* Now this just makes the music play really fast.  Why?
    kill(0, SIGIO);
    */
}

static void dspio_mpu401_intelligentout(struct dspio_state *dspio, Bit8u chan)
{
#if TRACE_MPU401
    S_printf("MPU401: dspio_mpu401_intelligentout(%02x), %d\n", chan, dspio->mpu.playbuf[chan].type);
#endif
    Bitu val;
    switch (dspio->mpu.playbuf[chan].type) {
	case T_OVERFLOW:
	    break;
	case T_MARK:
	    val = dspio->mpu.playbuf[chan].sys_val;
#if 0
	    if (val == 0xf8) { /* Timing overflow */
		dspio->mpu.playbuf[chan].counter = 0xf0;
		dspio->mpu.state.req_mask |= (1 << chan);
	    } else
#endif
	    if (val == 0xfc) {
		dspio_write_midi(dspio, val);
		dspio->mpu.state.amask &= ~(1 << chan);
		dspio->mpu.state.req_mask &= ~(1 << chan);
	    }
	    break;
	case T_MIDI_NORM:
	{
#if TIME_MPU401
	    struct timeval tv;
	    gettimeofday(&tv, NULL);
	    boolean is_play = ((dspio->mpu.playbuf[chan].value[0] & 0xF0) == 0x90);
	    if (is_play)
		S_printf("MPU401: Track %d Note ON %d %d.%03d\n", chan, dspio->mpu.playbuf[chan].value[1], tv.tv_sec, tv.tv_usec/1000);
#endif
	    Bit8u i;
	    for (i = 0; i < dspio->mpu.playbuf[chan].vlength; i++)
		dspio_write_midi(dspio, dspio->mpu.playbuf[chan].value[i]);
	    break;
	}
	default:
	    S_printf("MPU401: Unknown MPU data type %d\n", dspio->mpu.playbuf[chan].type);
	    break;
    }
}

static void dspio_mpu401_resetdone(struct dspio_state *dspio)
{
#if DEBUG_MPU401
    S_printf("MPU401: dspio_mpu401_resetdone\n");
#endif
    dspio->mpu.state.reset = FALSE;
    /* Note: SB16 docs conflict with MPU-401 docs.  SB16 (which is always in
     * UART mode) docs say ACK is sent on RESET, but MPU-401 docs say no ACK is
     * sent on RESET issued while in UART mode.  DOSBox follows MPU-401, but
     * some games seem to rely on behavior from SB16 docs instead (e.g.
     * Gateway).  Not ACKing leads to delays in some games which wait a very
     * long time for the ACK.  Real SB16 behavior is worth investigating. */
    if (dspio->mpu.prev_mode == M_UART) {
	S_printf("MPU401: ACKing reset from UART mode instead of strict compatibility.\n");
	dspio_mpu401_queue_databyte(DSPIO, MSG_MPU_ACK);
    } else {
	dspio_mpu401_queue_databyte(DSPIO, MSG_MPU_ACK);
    }
    if (dspio->mpu.state.cmd_pending_reset) {
	/* Block the ACK of any command issued while we were in RESET. */
	DSPIO->mpu.state.block_ack = TRUE;
#if DEBUG_MPU401
	S_printf("MPU401: Running latched command %X following reset\n", dspio->mpu.state.cmd_pending_reset - 1);
#endif
	dspio_mpu401_io_write(dspio, config.mpu401_base + 1, dspio->mpu.state.cmd_pending_reset - 1);
	dspio->mpu.state.cmd_pending_reset = 0;
    }
}

static void dspio_mpu401_reset(struct dspio_state *dspio)
{
#if TRACE_MPU401
    S_printf("MPU401: dspio_mpu401_reset\n");
#endif
    dspio->trigger_mpu_irq(FALSE);
    dspio->mpu.mode = (dspio->mpu.intelligent ? M_INTELLIGENT : M_UART);
    dspio->mpu.events[MPU401_EVENT_EOIHANDLER].enabled = FALSE;
    dspio->mpu.state.eoi_scheduled = FALSE;
    dspio->mpu.state.wsd = FALSE;
    dspio->mpu.state.wsm = FALSE;
    dspio->mpu.state.conductor = FALSE;
    dspio->mpu.state.cond_req = FALSE;
    dspio->mpu.state.cond_set = FALSE;
    dspio->mpu.state.playing = FALSE;
    dspio->mpu.state.run_irq = FALSE;
    dspio->mpu.state.irq_pending = FALSE;
    dspio->mpu.state.cmask = 0xff;
    dspio->mpu.state.amask = dspio->mpu.state.tmask = 0;
    dspio->mpu.state.midi_mask = 0xffff;
    dspio->mpu.state.data_onoff = 0;
    dspio->mpu.state.command_byte = 0;
    dspio->mpu.state.block_ack = FALSE;
    dspio->mpu.clock.tempo = dspio->mpu.clock.old_tempo = 100;
    dspio->mpu.clock.timebase = dspio->mpu.clock.old_timebase = 120;
    dspio->mpu.clock.tempo_rel = dspio->mpu.clock.old_tempo_rel = 40;
    dspio->mpu.clock.tempo_grad = 0;
    dspio->mpu.clock.clock_to_host = FALSE;
    dspio->mpu.clock.cth_rate = 60;
    dspio->mpu.clock.cth_counter = 0;
    dspio_clear_midi_in_fifo(dspio);
    dspio->mpu.state.req_mask = 0;
    dspio->mpu.condbuf.counter = 0;
    dspio->mpu.condbuf.type = T_OVERFLOW;
    Bitu i;
    for (i = 0; i < 8; i++) {
	dspio->mpu.playbuf[i].type = T_OVERFLOW;
	dspio->mpu.playbuf[i].counter = 0;
    }
    for (i = 0; i < MPU401_EVENT_MAX; i++) {
	memset(&dspio->mpu.events[i], 0, sizeof(dspio->mpu.events[i]));
    }
    dspio->mpu.newest_data = 0;
    dspio->mpu.state.next_track = 0;
}

static void dspio_mpu401_updatetrack(struct dspio_state *dspio, Bit8u chan)
{
#if TRACE_MPU401
    S_printf("MPU401: dspio_mpu401_updatetrack(%02x)\n", chan);
#endif
    dspio_mpu401_intelligentout(dspio, chan);
    if (dspio->mpu.state.amask & (1 << chan)) {
	dspio->mpu.playbuf[chan].vlength = 0;
	dspio->mpu.playbuf[chan].type = T_OVERFLOW;
	dspio->mpu.playbuf[chan].counter = 0xf0;
	dspio->mpu.state.req_mask |= (1 << chan);
    } else {
	if (dspio->mpu.state.amask == 0 && !dspio->mpu.state.conductor)
	    dspio->mpu.state.req_mask |= (1 << 12);
    }
}

static void dspio_mpu401_updateconductor(struct dspio_state *dspio)
{
#if TRACE_MPU401
    S_printf("MPU401: dspio_mpu401_updateconductor\n");
#endif
    if (dspio->mpu.condbuf.value[0] == 0xfc) {
	dspio->mpu.condbuf.value[0] = 0;
	dspio->mpu.state.conductor = FALSE;
	dspio->mpu.state.req_mask &= ~(1 << 9);
	if (dspio->mpu.state.amask == 0)
	    dspio->mpu.state.req_mask |= (1 << 12);
	return;
    }
    dspio->mpu.condbuf.vlength = 0;
    dspio->mpu.condbuf.counter = 0xf0;
    dspio->mpu.state.req_mask |= (1 << 9);
}

// Updates counters and requests new data on "End of Input"
static void dspio_mpu401_eoihandler(struct dspio_state *dspio)
{
#if TRACE_MPU401
    S_printf("MPU401: dspio_mpu401_eoihandler (%d %d %d %d)\n", dspio->mpu.state.send_now, dspio->mpu.state.cond_req, dspio->mpu.state.playing, dspio->mpu.state.req_mask);
#endif
    dspio->mpu.state.eoi_scheduled = FALSE;
    if (dspio->mpu.state.send_now) {
	dspio->mpu.state.send_now = FALSE;
	if (dspio->mpu.state.cond_req)
	    dspio_mpu401_updateconductor(dspio);
	else
	    dspio_mpu401_updatetrack(dspio, dspio->mpu.state.channel);
    }
    dspio->mpu.state.irq_pending = FALSE;

    if (!dspio->mpu.state.playing || !dspio->mpu.state.req_mask) {
	return;
    }

#define MAX_TRACK 16
    int i = 0;
    do {
	if (dspio->mpu.state.req_mask & (1 << i)) {
	    dspio->mpu.state.req_mask &= ~(1 << i);
#if DEBUG_MPU401
	    S_printf("MPU401: Requesting next data for track #%d\n", i);
#endif
	    dspio_mpu401_queue_databyte(dspio, 0xf0 + i);
	    /* Request only one track at a time. */
	    break;
	}
	i++;
    } while (i < MAX_TRACK);
}

static void dspio_mpu401_eoihandlerdispatch(struct dspio_state *dspio)
{
#if TRACE_MPU401
    S_printf("MPU401: dspio_mpu401_eoihandlerdispatch\n");
#endif
    if (dspio->mpu.state.send_now) {
	dspio->mpu.state.eoi_scheduled = TRUE;
	dspio->mpu.events[MPU401_EVENT_EOIHANDLER].us_remaining = 60;
	dspio->mpu.events[MPU401_EVENT_EOIHANDLER].last_tick = GETusTIME(0);
	dspio->mpu.events[MPU401_EVENT_EOIHANDLER].enabled = TRUE;
    }
    else if (!dspio->mpu.state.eoi_scheduled)
	dspio_mpu401_eoihandler(dspio);
}

static void dspio_process_midi(struct dspio_state *dspio)
{
#if DEBUG_MPU401
    S_printf("MPU401: dspio_process_midi()\n");
#endif
    int sequencer_ticks = 0;
    int timer_correction = 0;
#if DEBUG_MPU401
    int prev_last_tick = dspio->mpu.events[MPU401_EVENT_SEQUENCER].last_tick;
#endif
    int i;
    for (i = 0; i < MPU401_EVENT_MAX; i++) {
	if (dspio->mpu.events[i].enabled) {
	    int us_tick = GETusTIME(0);
	    /* Sequencer is periodic, not one-shot, so ensure that we account
	     * for any extra events that may have fired while we were scheduled
	     * out.  Maybe we miss some state transitions this way, but at
	     * least the track counters stay accurate. */
	    int delta = us_tick - dspio->mpu.events[i].last_tick;
	    if (delta < 0) /* rollover */
		delta = (INT_MAX + delta) + 1;

	    if (dspio->mpu.events[i].us_interval != 0 && delta > dspio->mpu.events[i].us_interval) {
		/* More than one event, queue extras and adjust delta. */
		while (delta > dspio->mpu.events[i].us_interval) {
		    dspio->mpu.events[i].us_remaining -= dspio->mpu.events[i].us_interval;
		    sequencer_ticks++;
		    delta -= dspio->mpu.events[i].us_interval;
		}
	    }
	    /* Process a (normal) delta that is less than an interval. */
	    dspio->mpu.events[i].us_remaining -= delta;
	    if (dspio->mpu.events[i].us_interval != 0 && dspio->mpu.events[i].us_remaining < 0) {
		sequencer_ticks++;
		/* Next event timer already began; add the remainder of
		 * this interval to next one. */
		timer_correction = dspio->mpu.events[i].us_remaining;
	    }
#if DEBUG_MPU401
	    S_printf("MPU401 timer: normal tick %d %d %d %d %d %d %d\n", i, sequencer_ticks, dspio->mpu.events[i].us_remaining, delta, dspio->mpu.events[MPU401_EVENT_SEQUENCER].us_interval, us_tick, dspio->mpu.events[MPU401_EVENT_SEQUENCER].last_tick);
#endif
	    dspio->mpu.events[i].last_tick = us_tick;
	}
    }

    if (dspio->mpu.events[MPU401_EVENT_SEQUENCER].enabled) {
	/* The following nasty housekeeping tries to make up sequencer
	 * ticks lost to IRQ latency.  The reason is that an interrupt to
	 * the DOS program could be delayed 10ms since we wait for the next
	 * SIGALRM to run its ISR; meanwhile, the sequencer is prevented
	 * from updating via irq_pending flag, since it's still waiting for
	 * the input it already requested from the DOS program.
	 *
	 * Also process any ticks that were missed due to host OS
	 * rescheduling. */

	/* Defer any sequencer ticks if an IRQ is still pending reply from
	 * the DOS program's ISR _and_ some HW counter is active. */
	if (dspio->mpu.state.irq_pending &&
		(dspio->mpu.state.amask ||
		 dspio->mpu.state.conductor ||
		 dspio->mpu.clock.clock_to_host)) {
#if DEBUG_MPU401
	    S_printf("MPU401: Defer %d ticks => %d\n", sequencer_ticks, dspio->mpu.deferred_ticks);
#endif
	    dspio->mpu.deferred_ticks += sequencer_ticks;
	    dspio->mpu.events[MPU401_EVENT_SEQUENCER].enabled = FALSE;
	    Bitu new_time = dspio->mpu.clock.tempo * dspio->mpu.clock.timebase;
	    if (new_time != 0) {
		int interval = 1000.0 * MPU401_TIMECONSTANT / new_time;
		dspio->mpu.events[MPU401_EVENT_SEQUENCER].us_interval = interval;
		dspio->mpu.events[MPU401_EVENT_SEQUENCER].us_remaining = interval + timer_correction;
#if DEBUG_MPU401
		S_printf("MPU401 timer reset: %d %d %d %d %d\n", new_time, interval, timer_correction, GETusTIME(0), prev_last_tick);
#endif
		dspio->mpu.events[MPU401_EVENT_SEQUENCER].enabled = TRUE;
	    }
	} else {
#if DEBUG_MPU401
	    S_printf("MPU401: Advance Intelligent state, %d %d (%d %d %d %d %d %d %d %d)\n", dspio->mpu.state.irq_pending, dspio->mpu.state.amask,
		    dspio->mpu.playbuf[0].counter, dspio->mpu.playbuf[1].counter, dspio->mpu.playbuf[2].counter, dspio->mpu.playbuf[3].counter,
		    dspio->mpu.playbuf[4].counter, dspio->mpu.playbuf[5].counter, dspio->mpu.playbuf[6].counter, dspio->mpu.playbuf[7].counter);
#endif
	    /* If we have any ticks, we assume to defer all but one. */
	    if (sequencer_ticks > 1) {
		dspio->mpu.deferred_ticks += sequencer_ticks - 1;
		sequencer_ticks = 1;
	    }

	    /* Advance as much as possible but drain no sequencer
	     * counter below 1. */
	    int max_advance = 255;
	    for (int i = 0; i < 8; i++) {
		if (dspio->mpu.state.amask & (1 << i)) {
		    if (dspio->mpu.playbuf[i].counter - 1 < max_advance) {
			max_advance = dspio->mpu.playbuf[i].counter - 1;
		    }
		}
	    }
	    if (dspio->mpu.state.conductor) {
		if (dspio->mpu.condbuf.counter - 1 < max_advance) {
		    max_advance = dspio->mpu.condbuf.counter - 1;
		}
	    }
	    if (dspio->mpu.clock.clock_to_host) {
		int cth_left = dspio->mpu.clock.cth_rate - dspio->mpu.clock.cth_counter;
		if (cth_left - 1 < max_advance) {
		    max_advance = cth_left - 1;
		}
	    }
#if DEBUG_MPU401
	    S_printf("MPU401: sequencer_ticks %d, deferred_ticks %d, max_advance %d\n", sequencer_ticks, dspio->mpu.deferred_ticks, max_advance);
#endif

	    if (max_advance > 0) {
		/* Process any extra sequencer ticks from this update or from a
		 * previous one, if we can. */
		int extra_ticks = 0;
		if (extra_ticks + dspio->mpu.deferred_ticks > max_advance) {
		    dspio->mpu.deferred_ticks -= max_advance - extra_ticks;
		    extra_ticks = max_advance;
		} else {
		    extra_ticks += dspio->mpu.deferred_ticks;
		    dspio->mpu.deferred_ticks = 0;
		}

		if (extra_ticks) {
		    S_printf("MPU401: draining %d ticks deferred due to IRQ latency\n", extra_ticks);
		}

		while (extra_ticks--) {
		    for (int i = 0; i < 8; i++) {
			if (dspio->mpu.state.amask & (1 << i)) {
			    dspio->mpu.playbuf[i].counter--;
			}
		    }
		    if (dspio->mpu.state.conductor)
			dspio->mpu.condbuf.counter--;

		    if (dspio->mpu.clock.clock_to_host)
			dspio->mpu.clock.cth_counter++;
		}
	    }

	    if (sequencer_ticks) {
		dspio->mpu.events[MPU401_EVENT_SEQUENCER].enabled = FALSE;

		/* Process a normal tick and handle counter expiry. */
		Bitu i;
		for (i = 0; i < 8; i++) {
		    if (dspio->mpu.state.amask & (1 << i)) {
			if (dspio->mpu.playbuf[i].counter > 0)
			    dspio->mpu.playbuf[i].counter--;
			if (dspio->mpu.playbuf[i].counter == 0)
			    dspio_mpu401_updatetrack(dspio, i);
		    }
		}
		if (dspio->mpu.state.conductor) {
		    if (dspio->mpu.condbuf.counter > 0)
			dspio->mpu.condbuf.counter--;
		    if (dspio->mpu.condbuf.counter == 0)
			dspio_mpu401_updateconductor(dspio);
		}
		if (dspio->mpu.clock.clock_to_host) {
		    dspio->mpu.clock.cth_counter++;
		    if (dspio->mpu.clock.cth_counter >= dspio->mpu.clock.cth_rate) {
			dspio->mpu.clock.cth_counter = 0;
			dspio->mpu.state.req_mask |= (1 << 13);
		    }
		}
		if (!dspio->mpu.state.irq_pending && dspio->mpu.state.req_mask)
		    dspio_mpu401_eoihandler(dspio);

		/* Sequencer timer had fired, so reset it */
		Bitu new_time = dspio->mpu.clock.tempo * dspio->mpu.clock.timebase;
		if (new_time != 0) {
		    int interval = 1000.0 * MPU401_TIMECONSTANT / new_time;
		    dspio->mpu.events[MPU401_EVENT_SEQUENCER].us_interval = interval;
		    dspio->mpu.events[MPU401_EVENT_SEQUENCER].us_remaining = interval + timer_correction;
#if DEBUG_MPU401
		    S_printf("MPU401 timer reset: %d %d %d %d %d\n", new_time, interval, timer_correction, GETusTIME(0), prev_last_tick);
#endif
		    dspio->mpu.events[MPU401_EVENT_SEQUENCER].enabled = TRUE;
		}
	    }
	}
    }

    if (dspio->mpu.events[MPU401_EVENT_RESETDONE].enabled &&
	dspio->mpu.events[MPU401_EVENT_RESETDONE].us_remaining <= 0) {
	dspio->mpu.events[MPU401_EVENT_RESETDONE].enabled = FALSE;
	dspio_mpu401_resetdone(dspio);
    }

    if (dspio->mpu.events[MPU401_EVENT_EOIHANDLER].enabled &&
	dspio->mpu.events[MPU401_EVENT_EOIHANDLER].us_remaining <= 0) {
	dspio->mpu.events[MPU401_EVENT_EOIHANDLER].enabled = FALSE;
	dspio_mpu401_eoihandler(dspio);
    }
    /* End hardware state advance.*/

    Bit8u data;

    while (!dspio_midi_output_empty(dspio)) {
	data = dspio_get_midi_data(dspio);
	midi_write(data);
    }

    while (midi_get_data_byte(&data)) {
	dspio_put_midi_in_byte(dspio, data);
	sb_handle_midi_data(); /* will assert IRQ */
    }
}

static void dspio_mpu401_writedata(struct dspio_state *dspio, Bitu val)
{
    static Bitu length, cnt, posd;
#if TRACE_MPU401
    S_printf("MPU401: dspio_mpu401_writedata(%02x) %d %d %d %d %d %d %d %d %d\n", val, dspio->mpu.state.command_byte, dspio->mpu.state.wsd, dspio->mpu.state.wsm, dspio->mpu.state.cond_req, dspio->mpu.state.data_onoff, length, cnt, posd, dspio->mpu.state.channel);
#endif
    if (dspio->mpu.mode == M_UART) {
	/* UART mode: just send out the byte. */
	dspio_write_midi(dspio, val);
	return;
    }
    switch (dspio->mpu.state.command_byte) {	/* 0xe# command data */
	case 0x00: /* No pending command */
	    break;
	case 0xe0:	/* Set tempo */
	    dspio->mpu.state.command_byte = 0;
	    dspio->mpu.clock.tempo = val;
#if DEBUG_MPU401
	    S_printf("MPU401: Tempo %d\n", dspio->mpu.clock.tempo);
#endif
	    return;
	case 0xe1:	/* Set relative tempo */
	    dspio->mpu.state.command_byte = 0;
	    if (val != 0x40) // default value
		S_printf("MPU401: Relative tempo change not implemented\n");
	    return;
	case 0xe7:	/* Set internal clock to host interval */
	    dspio->mpu.state.command_byte = 0;
	    dspio->mpu.clock.cth_rate = val >> 2;
	    return;
	case 0xec:	/* Set active track mask */
	    dspio->mpu.state.command_byte = 0;
	    dspio->mpu.state.tmask = val;
	    return;
	case 0xed: /* Set play counter mask */
	    dspio->mpu.state.command_byte = 0;
	    dspio->mpu.state.cmask = val;
	    return;
	case 0xee: /* Set 1-8 MIDI channel mask */
	    dspio->mpu.state.command_byte = 0;
	    dspio->mpu.state.midi_mask &= 0xff00;
	    dspio->mpu.state.midi_mask |= val;
	    return;
	case 0xef: /* Set 9-16 MIDI channel mask */
	    dspio->mpu.state.command_byte = 0;
	    dspio->mpu.state.midi_mask &= 0x00ff;
	    dspio->mpu.state.midi_mask |= ((Bit16u)val) << 8;
	    return;
	//case 0xe2:	/* Set graduation for relative tempo */
	//case 0xe4:	/* Set metronome */
	//case 0xe6:	/* Set metronome measure length */
	default:
	    dspio->mpu.state.command_byte = 0;
	    return;
    }

    if (dspio->mpu.state.wsd) {	/* Directly send MIDI message */
	if (dspio->mpu.state.wsd_start) {
	    dspio->mpu.state.wsd_start = 0;
	    cnt = 0;
	    switch (val & 0xf0) {
		case 0xc0:
		case 0xd0:
		    dspio->mpu.playbuf[dspio->mpu.state.channel].value[0] = val;
		    length = 2;
		    break;
		case 0x80:
		case 0x90:
		case 0xa0:
		case 0xb0:
		case 0xe0:
		    dspio->mpu.playbuf[dspio->mpu.state.channel].value[0] = val;
		    length = 3;
		    break;
		case 0xf0:
		    S_printf("MPU401: Illegal WSD byte\n");
		    dspio->mpu.state.wsd = 0;
		    dspio->mpu.state.channel = dspio->mpu.state.old_chan;
		    return;
		default: /* MIDI with running status */
		    cnt++;
		    dspio_write_midi(dspio, dspio->mpu.playbuf[dspio->mpu.state.channel].value[0]);
	    }
	}
	if (cnt < length) {
	    dspio_write_midi(dspio, val);
	    cnt++;
	}
	if (cnt == length) {
	    dspio->mpu.state.wsd = 0;
	    dspio->mpu.state.channel = dspio->mpu.state.old_chan;
	}
	return;
    }
    if (dspio->mpu.state.wsm) {	/* Directly send system message */
	if (val == MSG_EOX) {
	    dspio_write_midi(dspio, MSG_EOX);
	    dspio->mpu.state.wsm = 0;
	    return;
	}
	if (dspio->mpu.state.wsd_start) {
	    dspio->mpu.state.wsd_start = 0;
	    cnt = 0;
	    switch (val) {
		case 0xf2: { length = 3; break; }
		case 0xf3: { length = 2; break; }
		case 0xf6: { length = 1; break; }
		case 0xf0:
		default: { length = 0; break; }
	    }
	}
	if (!length || cnt < length) {
	    dspio_write_midi(dspio, val);
	    cnt++;
	}
	if (cnt == length)
	    dspio->mpu.state.wsm = 0;
	return;
    }
    if (dspio->mpu.state.cond_req) { /* Command */
	switch (dspio->mpu.state.data_onoff) {
	    case -1:
		return;
	    case 0:
		dspio->mpu.condbuf.vlength = 0;
		if (val < 0xf0) /* Timing byte */
		    dspio->mpu.state.data_onoff++;
		else {
		    dspio->mpu.state.data_onoff = -1;
		    dspio_mpu401_eoihandlerdispatch(dspio);
		    return;
		}
		if (val == 0)
		    dspio->mpu.state.send_now = TRUE;
		else
		    dspio->mpu.state.send_now = FALSE;
		dspio->mpu.condbuf.counter = val;
		break;
	    case 1: /* Command byte #1 */
		dspio->mpu.condbuf.type = T_COMMAND;
		if (val == 0xf8 || val == 0xf9)
		    dspio->mpu.condbuf.type = T_OVERFLOW;
		dspio->mpu.condbuf.value[dspio->mpu.condbuf.vlength] = val;
		dspio->mpu.condbuf.vlength++;
		if ((val & 0xf0) != 0xe0)
		    dspio_mpu401_eoihandlerdispatch(dspio);
		else
		    dspio->mpu.state.data_onoff++;
		break;
	    case 2:/* Command byte #2 */
		dspio->mpu.condbuf.value[dspio->mpu.condbuf.vlength] = val;
		dspio->mpu.condbuf.vlength++;
		dspio_mpu401_eoihandlerdispatch(dspio);
		break;
	}
	return;
    }
    switch (dspio->mpu.state.data_onoff) { /* Data */
	case -1:
	    return;
	case 0:
	    if (val < 0xf0) /* Timing byte */
		dspio->mpu.state.data_onoff = 1;
	    else {
		if (val == 0xFE) /* Host to instrument active sense */
		    dspio_write_midi(DSPIO, val);
		dspio->mpu.state.data_onoff = -1;
		dspio_mpu401_eoihandlerdispatch(dspio);
		return;
	    }
	    if (val == 0)
		dspio->mpu.state.send_now = TRUE;
	    else
		dspio->mpu.state.send_now = FALSE;
	    dspio->mpu.playbuf[dspio->mpu.state.channel].counter = val;
	    break;
	case 1: /* MIDI */
	    posd = ++dspio->mpu.playbuf[dspio->mpu.state.channel].vlength;
	    if (posd == 1) {
		switch (val & 0xf0) {
		    case 0xf0: /* System message or mark */
			if (val > 0xf7) {
			    dspio->mpu.playbuf[dspio->mpu.state.channel].type = T_MARK;
			    dspio->mpu.playbuf[dspio->mpu.state.channel].sys_val = val;
			    length = 1;
			} else {
			    S_printf("MPU401: Illegal message\n");
			    dspio->mpu.playbuf[dspio->mpu.state.channel].type = T_MIDI_SYS;
			    dspio->mpu.playbuf[dspio->mpu.state.channel].sys_val = val;
			    length = 1;
			}
			break;
		    case 0xc0:
		    case 0xd0: /* MIDI Message */
			dspio->mpu.playbuf[dspio->mpu.state.channel].type = T_MIDI_NORM;
			length = dspio->mpu.playbuf[dspio->mpu.state.channel].length = 2;
			break;
		    case 0x80:
		    case 0x90:
		    case 0xa0:
		    case 0xb0:
		    case 0xe0:
			dspio->mpu.playbuf[dspio->mpu.state.channel].type = T_MIDI_NORM;
			length = dspio->mpu.playbuf[dspio->mpu.state.channel].length = 3;
			break;
		    default: /* MIDI data with running status */
			posd++;
			dspio->mpu.playbuf[dspio->mpu.state.channel].vlength++;
			dspio->mpu.playbuf[dspio->mpu.state.channel].type = T_MIDI_NORM;
			length = dspio->mpu.playbuf[dspio->mpu.state.channel].length;
			break;
		}
	    }
	    if (!(posd == 1 && val >= 0xf0))
		dspio->mpu.playbuf[dspio->mpu.state.channel].value[posd - 1] = val;
	    if (posd == length)
		dspio_mpu401_eoihandlerdispatch(dspio);
	    break;
	default:
	    S_printf("MPU401: Illegal MIDI data state %d\n", dspio->mpu.state.data_onoff);
	    break;
    }
}

Bit8u dspio_mpu401_io_read(void *dspio, ioport_t port)
{
#if TRACE_MPU401
    S_printf("MPU401: dspio_mpu401_io_read(0x%x)\n", port);
#endif

    switch (port - config.mpu401_base) {
    case 0:
	/* Read data port */
	if (dspio_get_midi_in_fillup(DSPIO)) {
	    DSPIO->mpu.newest_data = dspio_get_midi_in_byte(DSPIO);
#if DEBUG_MPU401
	    S_printf("MPU401: Read data port = 0x%02x\n", DSPIO->mpu.newest_data);
#endif
	}
	else {
	    S_printf("MPU401: ERROR: No data to read, repeating previous data\n");
	}
	if (dspio_get_midi_in_fillup(DSPIO)) {
#if DEBUG_MPU401
	    S_printf("MPU401: %i bytes still in queue\n", dspio_get_midi_in_fillup(DSPIO));
#endif
	}

	if (!DSPIO->mpu.intelligent) {
	    return DSPIO->mpu.newest_data;
	}

	if (dspio_get_midi_in_fillup(DSPIO) == 0)
	    DSPIO->trigger_mpu_irq(FALSE);

	if (DSPIO->mpu.newest_data >= 0xf0 && DSPIO->mpu.newest_data <= 0xf7) { /* MIDI data request */
	    DSPIO->mpu.state.channel = DSPIO->mpu.newest_data & 7;
	    DSPIO->mpu.state.data_onoff = 0;
	    DSPIO->mpu.state.cond_req = FALSE;
	}
	if (DSPIO->mpu.newest_data == MSG_MPU_COMMAND_REQ) {
	    DSPIO->mpu.state.data_onoff = 0;
	    DSPIO->mpu.state.cond_req = TRUE;
	    if (DSPIO->mpu.condbuf.type != T_OVERFLOW) {
		DSPIO->mpu.state.block_ack = TRUE;
		dspio_mpu401_io_write(DSPIO, config.mpu401_base + 1, DSPIO->mpu.condbuf.value[0]);
		if (DSPIO->mpu.state.command_byte)
		    dspio_mpu401_io_write(DSPIO, config.mpu401_base, DSPIO->mpu.condbuf.value[1]);
	    }
	    DSPIO->mpu.condbuf.type = T_OVERFLOW;
	}
	if (DSPIO->mpu.newest_data == MSG_MPU_END || DSPIO->mpu.newest_data == MSG_MPU_CLOCK || DSPIO->mpu.newest_data == MSG_MPU_ACK) {
	    DSPIO->mpu.state.data_onoff = -1;
	    dspio_mpu401_eoihandlerdispatch(DSPIO);
	}

	return DSPIO->mpu.newest_data;
    case 1:
	/* Read status port */
	/* 0x40=OUTPUT_AVAIL; 0x80=INPUT_AVAIL */
	{
	uint8_t ret = 0x3f; /* Bits 6 and 7 clear */
	if (DSPIO->mpu.state.cmd_pending_reset || DSPIO->mpu.state.reset)
	    ret |= 0x40;
	if (!dspio_get_midi_in_fillup(DSPIO))
	    ret |= 0x80;
#if DEBUG_MPU401
	S_printf("MPU401: Read status port = 0x%02x\n", ret);
#endif
	return ret;
	}
    default:
	break;
    }

    S_printf("MPU401: Bogus read port 0x%x\n", port);
    return 0;
}

void dspio_mpu401_io_write(void *dspio, ioport_t port, Bit8u value)
{
#if TRACE_MPU401
    char ascii[5];
    ascii[0] = '\0';
    if ((char)value >= ' ' && (char)value <= '~') {
	if (snprintf(ascii, sizeof(ascii), " '%c'", value) < sizeof(ascii))
	    ascii[sizeof(ascii)-1] = '\0';
    }
    S_printf("MPU401: dspio_mpu401_io_write(0x%x) <- $%02X%s\n", port, value, ascii);
#endif
    switch (port - config.mpu401_base) {
    case 0:
	/* Write data port */
#if DEBUG_MPU401
	S_printf("MPU401: Write 0x%02x to data port\n", value);
#endif
	dspio_mpu401_writedata(DSPIO, value);
	break;
    case 1:
	/* Write command port */
#if DEBUG_MPU401
	S_printf("MPU401: Write 0x%02x to command port\n", value);
#endif

	if (DSPIO->mpu.state.reset) {
	    /* Latch the last received command, to run when reset is done. */
	    if (DSPIO->mpu.state.cmd_pending_reset || (value != 0x3f && value != 0xff)) {
		DSPIO->mpu.state.cmd_pending_reset = value + 1;
		return;
	    }
	    S_printf("MPU401: Double-reset!\n");
	    DSPIO->mpu.events[MPU401_EVENT_RESETDONE].enabled = FALSE;
	    DSPIO->mpu.state.reset = FALSE;
	}

	/* In UART mode, the only valid command is Reset. */
	if (value == 0xff) {	/* Reset MPU-401 */
	    S_printf("MPU401: Reset %X\n", value);
	    DSPIO->mpu.prev_mode = DSPIO->mpu.mode;
	    DSPIO->mpu.state.reset = TRUE;
	    dspio_mpu401_reset(DSPIO);
	    DSPIO->mpu.events[MPU401_EVENT_RESETDONE].us_remaining = 1000.0 * MPU401_RESETBUSY;
	    DSPIO->mpu.events[MPU401_EVENT_RESETDONE].last_tick = GETusTIME(0);
	    DSPIO->mpu.events[MPU401_EVENT_RESETDONE].enabled = TRUE;
	    return;
	} else if (DSPIO->mpu.mode != M_UART && value == 0x3f) { /* Enter UART mode */
	    S_printf("MPU401: Set UART mode %X\n", value);
	    DSPIO->mpu.mode = M_UART;
#if DEBUG_MPU401
	    S_printf("MPU401: Acking command %X\n", value);
#endif
	    dspio_mpu401_queue_databyte(DSPIO, MSG_MPU_ACK);
	    return;
	}

	/* Command was neither reset nor UART mode, so run it, but only if
	 * not in UART mode since no commands below are valid in UART mode. */
	if (DSPIO->mpu.mode == M_UART) {
	    return;
	}

	/* MPU-401 Intelligent mode adapted from DOSBox */
	if (value <= 0x2f) {
	    switch (value & 3) {    /* MIDI stop, start, continue */
		case 1:
		    dspio_write_midi(DSPIO, 0xfc);
		    break;
		case 2:
		    dspio_write_midi(DSPIO, 0xfa);
		    break;
		case 3:
		    dspio_write_midi(DSPIO, 0xfb);
		    break;
	    }

	    if (value & 0x20)
		S_printf("MPU401: Unhandled Recording Command %x\n", value);

	    switch (value & 0xc) {
		case 0x4:   /* Stop */
		    DSPIO->mpu.events[MPU401_EVENT_SEQUENCER].enabled = FALSE;
		    DSPIO->mpu.state.playing = FALSE;
		    Bit8u i;
		    for (i = 0xb0; i < 0xbf; i++) { /* All notes off */
			dspio_write_midi(DSPIO, i);
			dspio_write_midi(DSPIO, 0x7b);
			dspio_write_midi(DSPIO, 0);
		    }
		    break;
		case 0x8:   /* Play */
		    S_printf("MPU401: Intelligent mode playback started\n");
		    DSPIO->mpu.state.playing = TRUE;
		    DSPIO->mpu.events[MPU401_EVENT_SEQUENCER].us_remaining =
			    DSPIO->mpu.events[MPU401_EVENT_SEQUENCER].us_interval = 1000.0 * (MPU401_TIMECONSTANT / (DSPIO->mpu.clock.tempo * DSPIO->mpu.clock.timebase));
		    DSPIO->mpu.events[MPU401_EVENT_SEQUENCER].last_tick = GETusTIME(0);
		    DSPIO->mpu.events[MPU401_EVENT_SEQUENCER].enabled = TRUE;
		    dspio_clear_midi_in_fifo(DSPIO);
		    break;
	    }
	}
	else if (value >= 0xa0 && value <= 0xa7) {  /* Request play counter */
	    if (DSPIO->mpu.state.cmask & (1 << (value & 7)))
		dspio_mpu401_queue_databyte(DSPIO, DSPIO->mpu.playbuf[value & 7].counter);
	}
	else if (value >= 0xd0 && value <= 0xd7) {  /* Send data */
	    DSPIO->mpu.state.old_chan = DSPIO->mpu.state.channel;
	    DSPIO->mpu.state.channel = value & 7;
	    DSPIO->mpu.state.wsd = TRUE;
	    DSPIO->mpu.state.wsm = FALSE;
	    DSPIO->mpu.state.wsd_start = TRUE;
	}
	else
	switch (value) {
	    case 0xdf:	/* Send system message */
		DSPIO->mpu.state.wsd = FALSE;
		DSPIO->mpu.state.wsm = TRUE;
		DSPIO->mpu.state.wsd_start = TRUE;
		break;
	    case 0x8e:	/* Conductor */
		DSPIO->mpu.state.cond_set = FALSE;
		break;
	    case 0x8f:
		DSPIO->mpu.state.cond_set = TRUE;
		break;
	    case 0x94: /* Clock to host */
		DSPIO->mpu.clock.clock_to_host = FALSE;
		break;
	    case 0x95:
		DSPIO->mpu.clock.clock_to_host = TRUE;
		break;
	    /* Internal timebase */
	    case 0xc2: case 0xc3: case 0xc4: case 0xc5:
	    case 0xc6: case 0xc7: case 0xc8:
		DSPIO->mpu.clock.timebase = 48 + (24 * (value - 0xc2));
#if DEBUG_MPU401
		S_printf("MPU401: Timebase %d\n", DSPIO->mpu.clock.timebase);
#endif
		break;
	    /* Commands with data byte */
	    case 0xe0: case 0xe1: case 0xe2: case 0xe4: case 0xe6:
	    case 0xe7: case 0xec: case 0xed: case 0xee: case 0xef:
		DSPIO->mpu.state.command_byte = value;
		break;
	    /* Commands 0xa# returning data */
	    case 0xab:	/* Request and clear recording counter */
		dspio_mpu401_queue_databyte(DSPIO, MSG_MPU_ACK);
		dspio_mpu401_queue_databyte(DSPIO, 0);
		return;
	    case 0xac:	/* Request version */
		dspio_mpu401_queue_databyte(DSPIO, MSG_MPU_ACK);
		dspio_mpu401_queue_databyte(DSPIO, MPU401_VERSION);
		return;
	    case 0xad:	/* Request revision */
		dspio_mpu401_queue_databyte(DSPIO, MSG_MPU_ACK);
		dspio_mpu401_queue_databyte(DSPIO, MPU401_REVISION);
		return;
	    case 0xaf:	/* Request tempo */
		dspio_mpu401_queue_databyte(DSPIO, MSG_MPU_ACK);
		dspio_mpu401_queue_databyte(DSPIO, DSPIO->mpu.clock.tempo);
		return;
	    case 0xb1:	/* Reset relative tempo */
		DSPIO->mpu.clock.tempo_rel = 40;
		break;
	    case 0xb9:	/* Clear play map */
	    case 0xb8:	/* Clear play counters */
		{
		    Bit8u i;
		    for (i = 0xb0; i < 0xbf; i++) { /* All notes off */
			dspio_write_midi(DSPIO, i);
			dspio_write_midi(DSPIO, 0x7b);
			dspio_write_midi(DSPIO, 0);
		    }
		    for (i = 0; i < 8; i++) {
			DSPIO->mpu.playbuf[i].counter = 0;
			DSPIO->mpu.playbuf[i].type = T_OVERFLOW;
		    }
		    DSPIO->mpu.condbuf.counter = 0;
		    DSPIO->mpu.condbuf.type = T_OVERFLOW;
		    if (!(DSPIO->mpu.state.conductor = DSPIO->mpu.state.cond_set))
			DSPIO->mpu.state.cond_req = FALSE;
		    DSPIO->mpu.state.amask = DSPIO->mpu.state.tmask;
		    DSPIO->mpu.state.req_mask = 0;
		    DSPIO->mpu.state.irq_pending = TRUE;
		}
		break;
	    case 0xff: /* Reset MPU-401 and UART mode already handled */
	    case 0x3f:
		break;
	    default:
		S_printf("MPU401: Unhandled command %X\n", value);
	}

	// ACK every MPU-401 command that didn't already _return_ above.
#if DEBUG_MPU401
	S_printf("MPU401: Acking command %X\n", value);
#endif
	dspio_mpu401_queue_databyte(DSPIO, MSG_MPU_ACK);
    }
}

void dspio_run_synth(void)
{
    adlib_timer();
    midi_timer();
}

void dspio_timer(void *dspio)
{
    dspio_process_dma(DSPIO);
    dspio_process_midi(DSPIO);
}

void dspio_write_dac(void *dspio, Bit8u samp)
{
    sndbuf_t buf[1][SNDBUF_CHANS];
#if 0
    /* on SB16 speaker control does not exist */
    if (!DSPIO->speaker)
	return;
#endif
    buf[0][0] = samp;
    DSPIO->dac_running = 1;
    pcm_write_interleaved(buf, 1, DAC_BASE_FREQ, PCM_FORMAT_U8,
			  1, DSPIO->dac_strm);
}

static double dspio_get_volume(int id, int chan_dst, int chan_src, void *arg)
{
    double vol;
    enum MixSubChan msc;
    enum MixRet mr = MR_UNSUP;
    enum MixChan mc = (long)arg;
    int chans = sb_mixer_get_chan_num(mc);

    if (chan_src >= chans)
	return 0;
    if (mc == MC_NONE)
	return 1.0;
    switch (chan_dst) {
    case SB_CHAN_L:
	switch (chan_src) {
	case SB_CHAN_L:
	    msc = (chans == 1 ? MSC_MONO_L : MSC_L);
	    break;
	case SB_CHAN_R:
	    msc = MSC_RL;
	    break;
	default:
	    return 0;
	}
	break;
    case SB_CHAN_R:
	switch (chan_src) {
	case SB_CHAN_L:
	    msc = (chans == 1 ? MSC_MONO_R : MSC_LR);
	    break;
	case SB_CHAN_R:
	    msc = MSC_R;
	    break;
	default:
	    return 0;
	}
	break;
    default:
	return 0;
    }

    switch (id) {
    case PCM_ID_P:
	mr = sb_mixer_get_output_volume(mc, msc, &vol);
	break;
    case PCM_ID_R:
	mr = sb_mixer_get_input_volume(mc, msc, &vol);
	break;
    }

    if (mr != MR_OK)
	return 0;
    return vol;
}

int dspio_is_connected(int id, void *arg)
{
    enum MixChan mc = (long)arg;

    if (mc == MC_NONE)	// connect anonymous streams only to playback (P)
	return (id == PCM_ID_P);
    switch (id) {
    case PCM_ID_P:
	return sb_is_output_connected(mc);
    case PCM_ID_R:
	return sb_is_input_connected(mc);
    }
    return 0;
}
