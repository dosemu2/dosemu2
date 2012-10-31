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
#include "sound/midi.h"
#include "sound/sound.h"
#include "sound/sndpcm.h"
#if GCC_VERSION_CODE < 3001
#define deprecated
#endif


static const char *midoflus_name = "MIDI Output: FluidSynth device";
static const float flus_srate = 44100;
#define FLUS_CHANNELS 2

static fluid_settings_t* settings;
static fluid_synth_t* synth;
static fluid_sequencer_t* sequencer;
static short synthSeqID;
static fluid_midi_router_t* router;
static fluid_midi_parser_t* parser;
static int pcm_stream;

static int midoflus_init(void)
{
    settings = new_fluid_settings();
    fluid_settings_setnum(settings, "synth.sample-rate", flus_srate);

    synth = new_fluid_synth(settings);
    sequencer = new_fluid_sequencer2(0);
    synthSeqID = fluid_sequencer_register_fluidsynth(sequencer, synth);
    /* creating router - just for this?? fluidsynth API is crazy */
    router = new_fluid_midi_router(settings, fluid_synth_handle_midi_event, synth);
    fluid_midi_router_clear_rules(router);
    parser = new_fluid_midi_parser();

    pcm_stream = pcm_allocate_stream(FLUS_CHANNELS, "MIDI");

    return 1;
}

static void midoflus_done(void)
{
    delete_fluid_midi_parser(parser);
    delete_fluid_midi_router(router);
    delete_fluid_sequencer(sequencer);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
}

static void midoflus_reset(void)
{
}

static void midoflus_write(unsigned char val)
{
    fluid_midi_event_t* event;
    event = fluid_midi_parser_parse(parser, val);
    if (event != NULL)
	fluid_midi_router_handle_midi_event(router, event);
}

static void midoflus_timer(void)
{
#if 0
    period = pcm_frame_period_us(opl3_rate);
    nframes = (now - mf_time_cur) / period;
    if (nframes > OPL3_MAX_BUF)
	nframes = OPL3_MAX_BUF;
    if (nframes) {
//	mf_process_samples(nframes);
	mf_time_cur += nframes * period;
	S_printf("MIDI: processed %i samples with fluidsynth\n", nframes);
    }
#endif
}

CONSTRUCTOR(static int midoflus_register(void))
{
    struct midi_out_plugin midoflus;
    midoflus.name = midoflus_name;
    midoflus.init = midoflus_init;
    midoflus.done = midoflus_done;
    midoflus.reset = midoflus_reset;
    midoflus.write = midoflus_write;
    midoflus.stop = NULL;
    midoflus.timer = midoflus_timer;
#if 0
    return midi_register_output_plugin(midoflus);
#else
    return 0;
#endif
}
