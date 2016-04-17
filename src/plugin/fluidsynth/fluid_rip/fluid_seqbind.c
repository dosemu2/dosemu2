/* fluidsynth seqbind hacks done by stsp for dosemu2 project.
 * Since the fluidsynth folks do not accept my patches that add the
 * API I need, I have to emulate that API by some bad means.
 * https://sourceforge.net/p/fluidsynth/tickets/110/
 * The emulation is not accurate, for example there would be the
 * memory leaks on termination. */

#include <fluidsynth.h>
#include "fluid_midi.h"
#include "fluid_compat.h"
#include "seqbind.h"

struct _fluid_seqbind_priv_t {
	fluid_midi_parser_t* parser;
	fluid_sequencer_t* seq;
};
typedef struct _fluid_seqbind_priv_t fluid_seqbind_priv_t;

/**
 * Registers a synthesizer as a destination client of the given sequencer.
 * The \a synth is registered with the name "fluidsynth".
 * @param seq Sequencer instance
 * @param synth Synthesizer instance
 * @returns Sequencer client ID, or #FLUID_FAILED on error.
 */
void*
fluid_sequencer_register_fluidsynth2 (fluid_sequencer_t* seq, fluid_synth_t* synth)
{
	fluid_seqbind_priv_t* priv;
	fluid_midi_parser_t* parser;

	fluid_sequencer_register_fluidsynth(seq, synth);
	parser = new_fluid_midi_parser();
	if (parser == NULL) {
		fluid_log(FLUID_PANIC, "sequencer: Out of memory\n");
		return NULL;
	}
	priv = FLUID_NEW(fluid_seqbind_priv_t);
	if (priv == NULL) {
		fluid_log(FLUID_PANIC, "sequencer: Out of memory\n");
		delete_fluid_midi_parser(parser);
		return NULL;
	}

	priv->parser = parser;
	priv->seq = seq;

	return priv;
}

/**
 * Transforms an incoming midi data to a
 * sequencer event and adds it to the sequencer queue for sending as soon as possible
 * @param seq The sequencer, must be a valid #fluid_sequencer_t
 * @param data MIDI data
 * @param length MIDI data length
 * @return #FLUID_OK or #FLUID_FAILED
 * @since 1.1.0
 */
int
fluid_sequencer_add_midi_data_to_buffer(void* priv, unsigned char* data,
		int length)
{
	fluid_midi_event_t* event;
	int i, ret;
	fluid_seqbind_priv_t* seqpriv = priv;

	for (i = 0; i < length; i++) {
		event = fluid_midi_parser_parse(seqpriv->parser, data[i]);
		if (event != NULL) {
			ret = fluid_sequencer_add_midi_event_to_buffer(seqpriv->seq, event);
			if (ret != FLUID_OK)
				return ret;
		}
	}
	return FLUID_OK;
}
