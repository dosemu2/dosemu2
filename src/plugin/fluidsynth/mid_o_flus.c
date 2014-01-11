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
#include <fluidsynth.h>
#include "fluid_midi.h"
#include "emu.h"
#include "init.h"
#include "timers.h"
#include "sound/midi.h"
#include "sound/sound.h"
#include "sound/sndpcm.h"


static const char *midoflus_name = "MIDI Output: FluidSynth device";
static const float flus_srate = 44100;
static const int flus_format = PCM_FORMAT_S16_LE;
static const char *sfont = "/usr/share/soundfonts/default.sf2";
#define FLUS_CHANNELS 2
#define FLUS_MAX_BUF 512
#define FLUS_MIN_BUF 128

static fluid_settings_t* settings;
static fluid_synth_t* synth;
static fluid_sequencer_t* sequencer;
static short synthSeqID;
static fluid_midi_parser_t* parser;
static int pcm_stream, pcm_running;
static double mf_time_cur, mf_time_base;


static int midoflus_init(void)
{
    int ret;
    settings = new_fluid_settings();
    fluid_settings_setint(settings, "synth.lock-memory", 0);
    fluid_settings_setnum(settings, "synth.sample-rate", flus_srate);

    synth = new_fluid_synth(settings);
    ret = fluid_synth_sfload(synth, sfont, TRUE);
    if (ret == FLUID_FAILED) {
	warn("fluidsynth: cannot load soundfont %s\n", sfont);
	delete_fluid_synth(synth);
	delete_fluid_settings(settings);
	return 0;
    }
    S_printf("fluidsynth: loaded soundfont %s ID=%i\n", sfont, ret);
    sequencer = new_fluid_sequencer2(0);
    synthSeqID = fluid_sequencer_register_fluidsynth(sequencer, synth);
    parser = new_fluid_midi_parser();

    pcm_stream = pcm_allocate_stream(FLUS_CHANNELS, "MIDI", PCM_ID_P);

    return 1;
}

static void midoflus_done(void)
{
    delete_fluid_midi_parser(parser);
    delete_fluid_sequencer(sequencer);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

static void midoflus_reset(void)
{
}

static void midoflus_start(void)
{
    S_printf("MIDI: starting fluidsynth\n");
    mf_time_cur = GETusTIME(0);
    mf_time_base = mf_time_cur;
    fluid_sequencer_process(sequencer, 0);
}

static void midoflus_write(unsigned char val)
{
    fluid_midi_event_t* event;

    if (mf_time_cur == 0)
	midoflus_start();

    event = fluid_midi_parser_parse(parser, val);
    if (event != NULL) {
	int ret;
	unsigned long long now = GETusTIME(0);
	int msec = (now - mf_time_base) / 1000;
	fluid_sequencer_process(sequencer, msec);
	if (debug_level('S') >= 5)
	    S_printf("MIDI: sending event to fluidsynth, msec=%i\n", msec);
	ret = fluid_sequencer_add_midi_event_to_buffer(sequencer, event);
	if (ret != FLUID_OK)
	    S_printf("MIDI: failed sending midi event\n");
    }
}

static void mf_process_samples(int nframes)
{
    sndbuf_t buf[FLUS_MAX_BUF][FLUS_CHANNELS];
    int ret = fluid_synth_write_s16(synth, nframes, buf, 0, 2, buf, 1, 2);
    if (ret != FLUID_OK) {
	error("MIDI: fluidsynth failed\n");
	return;
    }
    pcm_running = 1;
    pcm_write_interleaved(buf, nframes, flus_srate, flus_format,
	    FLUS_CHANNELS, pcm_stream);
}

static void process_samples(long long now, int min_buf)
{
    int nframes, retry;
    double period;
    if (mf_time_cur == 0)
	return;
    do {
	retry = 0;
	period = pcm_frame_period_us(flus_srate);
	nframes = (now - mf_time_cur) / period;
	if (nframes > FLUS_MAX_BUF) {
	    nframes = FLUS_MAX_BUF;
	    retry = 1;
	}
	if (nframes >= min_buf) {
	    mf_process_samples(nframes);
	    mf_time_cur += nframes * period;
	    if (debug_level('S') >= 5)
		S_printf("MIDI: processed %i samples with fluidsynth\n", nframes);
	}
    } while (retry);
}

static void midoflus_stop(void)
{
    long long now;
    int msec;
    now = GETusTIME(0);
    msec = (now - mf_time_base) / 1000;
    S_printf("MIDI: stopping fluidsynth at msec=%i\n", msec);
    /* advance past last event */
    fluid_sequencer_process(sequencer, msec);
    /* shut down all active notes */
    fluid_synth_system_reset(synth);
    if (pcm_running)
	pcm_flush(pcm_stream);
    pcm_running = 0;
    mf_time_cur = 0;
}

static void midoflus_timer(void)
{
    if (mf_time_cur == 0)
	return;
    process_samples(GETusTIME(0), FLUS_MIN_BUF);
}

CONSTRUCTOR(static int midoflus_register(void))
{
    struct midi_out_plugin midoflus;
    midoflus.name = midoflus_name;
    midoflus.init = midoflus_init;
    midoflus.done = midoflus_done;
    midoflus.reset = midoflus_reset;
    midoflus.write = midoflus_write;
    midoflus.stop = midoflus_stop;
    midoflus.timer = midoflus_timer;
#if 1
    return midi_register_output_plugin(midoflus);
#else
    return 0;
#endif
}
