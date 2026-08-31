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
 * Purpose: fluidsynth midi synth
 *
 * Author: Stas Sergeev
 *
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#define FLUIDSYNTH_API
#define FLUID_DEPRECATED
#include <fluidsynth/types.h>
#include <fluidsynth/misc.h>
#include <fluidsynth/event.h>
#include <fluidsynth/seqbind.h>
#include <fluidsynth/audio.h>
#include <fluidsynth/settings.h>
#include <fluidsynth/synth.h>
#include <fluidsynth/log.h>
#if defined(__APPLE__) || defined(__ANDROID__) /* to redefine sem_init() and related functions */
#include "utilities.h"
#else
#include <semaphore.h>
#endif
#include "emu.h"
#include "init.h"
#include "timers.h"
#include "sound/midi.h"
#include "sound/sound.h"
#include "midi/fluid_midi.h"
#include "midi/mt32remap.h"

#define midoflus_name "flus"
#define midoflus_longname "MIDI Output: FluidSynth device"
#define midoflus_name_mt32 "openmt32"
#define midoflus_longname_mt32 "MIDI Output: FluidSynth/openmt32 device"
static const int flus_format = PCM_FORMAT_S16_LE;
static const float flus_srate = 44100.0;
#define FLUS_CHANNELS 2
#define FLUS_MAX_BUF 512
#define FLUS_MIN_BUF 128

struct flu_state {
    fluid_settings_t* settings;
    fluid_synth_t* synth;
    fluid_sequencer_t* sequencer;
    fluid_midi_parser_t* parser;
    fluid_seq_id_t synthSeqID;
    mt32_t *mt;
    int mt32_msec;
    int output_running;
    double mf_time_base;
    int pcm_stream;
    int pcm_running;
    pthread_t syn_thr;
    sem_t syn_sem;
    int inited;
};
static struct flu_state flus[ST_MAX];

static pthread_mutex_t syn_mtx = PTHREAD_MUTEX_INITIALIZER;
static void *synth_thread(void *arg);

static int do_flu_init(struct flu_state *fs, const char *sfont,
    const char *pcm_name)
{
    int ret;

    fs->settings = new_fluid_settings();
    fluid_settings_setint(fs->settings, "synth.lock-memory", 0);
    fluid_settings_setnum(fs->settings, "synth.gain", config.fluid_volume / 4.0);
    fluid_settings_setnum(fs->settings, "synth.sample-rate", flus_srate);
#if 0
    fluid_settings_setint(fs->settings, "synth.verbose", TRUE);
    fluid_set_log_function(FLUID_DBG, fluid_default_log_function, NULL);
#endif
    fs->synth = new_fluid_synth(fs->settings);
#ifdef FE_NOMASK_ENV
    /* workaround for:
     * https://github.com/libsndfile/libsndfile/issues/1157
     */
    fedisableexcept(FE_DIVBYZERO);
#endif
    ret = fluid_synth_sfload(fs->synth, sfont, TRUE);
#ifdef FE_NOMASK_ENV
    fesetenv(&dosemu_fenv);
#endif
    if (ret == FLUID_FAILED) {
	error("fluidsynth: cannot load soundfont %s\n", sfont);
	goto err2;
    }
    S_printf("fluidsynth: loaded soundfont %s ID=%i\n", sfont, ret);
    fluid_settings_setstr(fs->settings, "synth.midi-bank-select", "gm");
    fs->sequencer = new_fluid_sequencer2(0);
    fs->parser = new_fluid_midi_parser();
    fs->synthSeqID = fluid_sequencer_register_fluidsynth(fs->sequencer, fs->synth);

    fs->pcm_stream = pcm_allocate_stream(FLUS_CHANNELS, pcm_name,
	    (void*)MC_MIDI);

    sem_init(&fs->syn_sem, 0, 0);
    pthread_create(&fs->syn_thr, NULL, synth_thread, fs);
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(__GLIBC__)
    pthread_setname_np(fs->syn_thr, "dosemu: fluid");
#endif
    fs->inited++;

    return 0;

err2:
    delete_fluid_synth(fs->synth);
    delete_fluid_settings(fs->settings);
    return -1;
}

static int midoflus_init(void *arg)
{
    int err;
    char *sfont = NULL;
    const char *def_sfonts[] = {
	"/usr/share/soundfonts/default.sf2",		// fedora
	DATADIR "/soundfonts/default.sf2",
	"/usr/share/sounds/sf2/default-GM.sf2",		// ubuntu
	"/usr/share/soundfonts/FluidR3_GM.sf2",		// fedora
	DATADIR "/soundfonts/FluidR3_GM.sf2",	// termux
	"/usr/share/sounds/sf2/FluidR3_GM.sf2.flac",	// ubuntu
	"/usr/share/sounds/sf2/FluidR3_GM.sf2",		// debian
	NULL };
    const char *adrivers[] = { NULL };
    fluid_settings_t *settings = new_fluid_settings();

    fluid_audio_driver_register(adrivers);

    if (config.fluid_sfont && config.fluid_sfont[0]) {
	if (access(config.fluid_sfont, R_OK) == 0)
	    sfont = strdup(config.fluid_sfont);
	else
	    error("soundfont %s missing\n", config.fluid_sfont);
    } else {
	int ret = fluid_settings_dupstr(settings, "synth.default-soundfont",
		&sfont);
	if (ret == FLUID_FAILED) {
	    error("Your fluidsynth is too old\n");
	} else if (access(sfont, R_OK) != 0) {
	    warn("fluidsynth sound font unavailable at %s\n", sfont);
	    free(sfont);
	    sfont = NULL;
	}
	if (!sfont) {
	    int i = 0;

	    while (def_sfonts[i]) {
		if (access(def_sfonts[i], R_OK) == 0) {
		    sfont = strdup(def_sfonts[i]);
		    break;
		}
		i++;
	    }
	    if (!sfont)
		error("soundfonts not found\n");
	}
    }
    delete_fluid_settings(settings);

    if (!sfont)
	return 0;
    err = do_flu_init(&flus[ST_GM], sfont, "MIDI");
    free(sfont);
    if (err)
	return 0;
    return 1;
}

static int midoflus_init_mt32(void *arg)
{
    int err;
    char *sfont_mt32 = NULL;
    const char *def_sfonts_mt32[] = {
	"/usr/share/sounds/openmt32/OpenMT32.sf3",	// ubuntu
	NULL };
    const char *adrivers[] = { NULL };

    fluid_audio_driver_register(adrivers);

    if (config.fluid_sfont_mt32 && config.fluid_sfont_mt32[0]) {
	if (access(config.fluid_sfont_mt32, R_OK) == 0) {
	    sfont_mt32 = strdup(config.fluid_sfont_mt32);
	} else {
	    error("MT32 soundfont %s missing\n", config.fluid_sfont_mt32);
	    return 0;
	}
    } else {
	int i = 0;

	while (def_sfonts_mt32[i]) {
	    if (access(def_sfonts_mt32[i], R_OK) == 0) {
		sfont_mt32 = strdup(def_sfonts_mt32[i]);
		break;
	    }
	    i++;
	}
	if (!sfont_mt32) {
	    error("MT32 soundfonts not found\n");
	    return 0;
	}
    }
    assert(sfont_mt32);
    err = do_flu_init(&flus[ST_MT32], sfont_mt32, "MT32");
    free(sfont_mt32);
    if (err)
	return 0;
    flus[ST_MT32].mt = mt32remap_init();
    return 1;
}

static void do_done(struct flu_state *fs)
{
    if (!fs->inited)
	return;
    pthread_cancel(fs->syn_thr);
    pthread_join(fs->syn_thr, NULL);
    sem_destroy(&fs->syn_sem);
    delete_fluid_midi_parser(fs->parser);
    delete_fluid_sequencer(fs->sequencer);
    delete_fluid_synth(fs->synth);
    delete_fluid_settings(fs->settings);
    if (fs->mt)
	mt32remap_done(fs->mt);
}

static void midoflus_done(void *arg)
{
    do_done(&flus[ST_GM]);
}

static void midoflus_done_mt32(void *arg)
{
    do_done(&flus[ST_MT32]);
}

static void midoflus_start(struct flu_state *fs)
{
    S_printf("MIDI: starting fluidsynth\n");
    fs->mf_time_base = GETusTIME(0);
    assert(fs->sequencer);
    pthread_mutex_lock(&syn_mtx);
    pcm_prepare_stream(fs->pcm_stream);
    fluid_sequencer_process(fs->sequencer, 0);
    fs->output_running = 1;
    pthread_mutex_unlock(&syn_mtx);
}

static void do_write(void *arg, unsigned char *data, int len)
{
    struct flu_state *fs = arg;

    assert(fs->parser->nr_bytes == 0);
    for (int i = 0; i < len; i++) {
	fluid_midi_event_t *event = fluid_midi_parser_parse(fs->parser, data[i]);
	if (event != NULL) {
	    int ret;

	    fluid_sequencer_process(fs->sequencer, fs->mt32_msec++);
	    ret = fluid_sequencer_add_midi_event_to_buffer(fs->sequencer, event);
	    if (ret != FLUID_OK)
		S_printf("MIDI: failed sending midi data of size %i\n", len);
	}
    }
    assert(fs->parser->nr_bytes == 0);
}

static int do_mt32_event(struct flu_state *fs, fluid_midi_event_t *ev,
    int msec)
{
    int ch = fluid_midi_event_get_channel(ev);
    int e = fluid_midi_event_get_type(ev);

    if (e >= 0x80 && e <= 0xe0 && !mt32remap_channel_assigned(fs->mt, ch))
	return 1;
    fs->mt32_msec = msec;

    switch (e) {
    case NOTE_ON:
	mt32remap_noteon(fs->mt, ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_velocity(ev), do_write, fs);
	break;
    case PROGRAM_CHANGE:
	mt32remap_program(fs->mt, ch, fluid_midi_event_get_program(ev));
	return 1;
    case MIDI_SYSEX:
	mt32remap_sysex(fs->mt, ev->paramptr, ev->param1);
	break;
    }
    return 0;
}

static void midoflus_write(unsigned char val, enum SynthType type)
{
    int ret = FLUID_OK;
    struct flu_state *fs =&flus[type];
    fluid_midi_event_t* event;
    unsigned long long now = GETusTIME(0);
    int msec = (now - fs->mf_time_base) / 1000;

    if (!fs->output_running)
	midoflus_start(fs);

    assert(fs->parser);
    event = fluid_midi_parser_parse(fs->parser, val);
    if (event != NULL) {
	fluid_midi_event_t event2 = *event;
	pthread_mutex_lock(&syn_mtx);
	if (type == ST_GM || do_mt32_event(fs, event, msec) == 0) {
	    if (fs->mt32_msec > msec)
		msec = fs->mt32_msec++;
	    fluid_sequencer_process(fs->sequencer, msec);
	    ret = fluid_sequencer_add_midi_event_to_buffer(fs->sequencer, &event2);
	}
	if (ret != FLUID_OK)
	    S_printf("MIDI: failed sending midi event\n");
	pthread_mutex_unlock(&syn_mtx);
    }
}

static void mf_process_samples(struct flu_state *fs, int nframes)
{
    sndbuf_t buf[FLUS_MAX_BUF][FLUS_CHANNELS];
    int ret;
    ret = fluid_synth_write_s16(fs->synth, nframes, buf, 0, 2, buf, 1, 2);
    if (ret != FLUID_OK) {
	error("MIDI: fluidsynth failed\n");
	return;
    }
    fs->pcm_running = 1;
    pcm_write_interleaved(buf, nframes, flus_srate, flus_format,
	    FLUS_CHANNELS, fs->pcm_stream);
}

static void process_samples(struct flu_state *fs, long long now, int min_buf)
{
    int nframes, retry;
    double period, mf_time_cur;
    mf_time_cur = pcm_get_stream_time(fs->pcm_stream);
    do {
	retry = 0;
	period = pcm_frame_period_us(flus_srate);
	nframes = (now - mf_time_cur) / period;
	if (nframes > FLUS_MAX_BUF) {
	    nframes = FLUS_MAX_BUF;
	    retry = 1;
	}
	if (nframes >= min_buf) {
	    mf_process_samples(fs, nframes);
	    mf_time_cur = pcm_get_stream_time(fs->pcm_stream);
	    if (debug_level('S') >= 5)
		S_printf("MIDI: processed %i samples with fluidsynth\n", nframes);
	}
    } while (retry);
}

static void do_stop(struct flu_state *fs)
{
    long long now;
    int msec;

    pthread_mutex_lock(&syn_mtx);
    if (!fs->output_running) {
	pthread_mutex_unlock(&syn_mtx);
	return;
    }
    now = GETusTIME(0);
    msec = (now - fs->mf_time_base) / 1000;
    S_printf("MIDI: stopping fluidsynth at msec=%i\n", msec);
    /* advance past last event */
    fluid_sequencer_process(fs->sequencer, msec);
    /* shut down all active notes */
    fluid_synth_system_reset(fs->synth);
    if (fs->pcm_running)
	pcm_flush(fs->pcm_stream);
    fs->pcm_running = 0;
    fs->output_running = 0;
    pthread_mutex_unlock(&syn_mtx);
}

static void midoflus_stop(void *arg)
{
    do_stop(&flus[ST_GM]);
}

static void midoflus_stop_mt32(void *arg)
{
    do_stop(&flus[ST_MT32]);
}

static void *synth_thread(void *arg)
{
    struct flu_state *fs = arg;
    while (1) {
	sem_wait(&fs->syn_sem);
	pthread_mutex_lock(&syn_mtx);
	if (!fs->output_running) {
	    pthread_mutex_unlock(&syn_mtx);
	    continue;
	}
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	process_samples(fs, GETusTIME(0), FLUS_MIN_BUF);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_mutex_unlock(&syn_mtx);
    }
    return NULL;
}

static void midoflus_run(void)
{
    struct flu_state *fs = &flus[ST_GM];
    if (!fs->output_running)
	return;
    sem_post(&fs->syn_sem);
}

static void midoflus_run_mt32(void)
{
    struct flu_state *fs = &flus[ST_MT32];
    if (!fs->output_running)
	return;
    sem_post(&fs->syn_sem);
}

static int midoflus_cfg(void *arg)
{
    return pcm_parse_cfg(config.midi_driver, midoflus_name);
}

static int midoflus_cfg_mt32(void *arg)
{
    return pcm_parse_cfg(config.midi_driver, midoflus_name_mt32);
}

static const struct midi_out_plugin midoflus
#ifdef __cplusplus
={
    midoflus_name,
    midoflus_longname,
    midoflus_cfg,
    midoflus_init,
    midoflus_done,
    MIDI_W_PCM | MIDI_W_PREFERRED,
    midoflus_write,
    midoflus_stop,
    midoflus_run,
    0
};
#else
= {
    .name = midoflus_name,
    .longname = midoflus_longname,
    .get_cfg = midoflus_cfg,
    .open = midoflus_init,
    .close = midoflus_done,
    .weight = MIDI_W_PCM | MIDI_W_PREFERRED,
    .write = midoflus_write,
    .stop = midoflus_stop,
    .run = midoflus_run,
};
#endif

static const struct midi_out_plugin midoflus_mt32
#ifdef __cplusplus
={
    midoflus_name_mt32,
    midoflus_longname_mt32,
    midoflus_cfg_mt32,
    midoflus_init_mt32,
    midoflus_done_mt32,
    MIDI_W_PCM | MIDI_W_PREFERRED,
    midoflus_write,
    midoflus_stop_mt32,
    midoflus_run_mt32,
    0
};
#else
= {
    .name = midoflus_name_mt32,
    .longname = midoflus_longname_mt32,
    .get_cfg = midoflus_cfg_mt32,
    .open = midoflus_init_mt32,
    .close = midoflus_done_mt32,
    .weight = MIDI_W_PCM | MIDI_W_PREFERRED,
    .write = midoflus_write,
    .stop = midoflus_stop_mt32,
    .run = midoflus_run_mt32,
};
#endif

static void mt32_scrub(void)
{
    midi_register_output_plugin(&midoflus, ST_GM);
    if (config.fluid_sfont_mt32 && config.fluid_sfont_mt32[0])
        midi_register_output_plugin(&midoflus_mt32, ST_MT32);
}

CONSTRUCTOR(static void midoflus_register(void))
{
    register_config_scrub(mt32_scrub);
}
