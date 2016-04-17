/* This is a rip-off of the seqbind.h found in fluidsynth sources,
 * done by stsp for dosemu2 project. */

/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#ifndef _FLUIDSYNTH_SEQBIND_RIP_H
#define _FLUIDSYNTH_SEQBIND_RIP_H

#ifndef FLUIDSYNTH_API
#define FLUIDSYNTH_API
#endif

/**
 * @file seqbind.h
 * @brief Functions for binding sequencer objects to other subsystems.
 */

FLUIDSYNTH_API
void* fluid_sequencer_register_fluidsynth2(fluid_sequencer_t* seq, fluid_synth_t* synth);
FLUIDSYNTH_API int
fluid_sequencer_add_midi_data_to_buffer(void* priv, unsigned char* data,
		int length);

#endif /* _FLUIDSYNTH_SEQBIND_H */

