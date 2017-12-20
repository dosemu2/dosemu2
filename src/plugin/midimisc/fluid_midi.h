/* This is a rip-off of the fluid_midi.h found in fluidsynth sources,
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

#ifndef _FLUID_MIDI_H
#define _FLUID_MIDI_H

typedef struct _fluid_midi_parser_t fluid_midi_parser_t;
typedef struct _fluid_midi_event_t fluid_midi_event_t;          /**< MIDI event*/

fluid_midi_parser_t* new_fluid_midi_parser(void);
int delete_fluid_midi_parser(fluid_midi_parser_t* parser);
fluid_midi_event_t* fluid_midi_parser_parse(fluid_midi_parser_t* parser, unsigned char c);

enum fluid_midi_event_type {
  /* channel messages */
  NOTE_OFF = 0x80,
  NOTE_ON = 0x90,
  KEY_PRESSURE = 0xa0,
  CONTROL_CHANGE = 0xb0,
  PROGRAM_CHANGE = 0xc0,
  CHANNEL_PRESSURE = 0xd0,
  PITCH_BEND = 0xe0,
  /* system exclusive */
  MIDI_SYSEX = 0xf0,
  /* system common - never in midi files */
  MIDI_TIME_CODE = 0xf1,
  MIDI_SONG_POSITION = 0xf2,
  MIDI_SONG_SELECT = 0xf3,
  MIDI_TUNE_REQUEST = 0xf6,
  MIDI_EOX = 0xf7,
  /* system real-time - never in midi files */
  MIDI_SYNC = 0xf8,
  MIDI_TICK = 0xf9,
  MIDI_START = 0xfa,
  MIDI_CONTINUE = 0xfb,
  MIDI_STOP = 0xfc,
  MIDI_ACTIVE_SENSING = 0xfe,
  MIDI_SYSTEM_RESET = 0xff,
  /* meta event - for midi files only */
  MIDI_META_EVENT = 0xff
};

/*
 * fluid_midi_event_t
 */
struct _fluid_midi_event_t {
  fluid_midi_event_t* next; /* Link to next event */
  void *paramptr;           /* Pointer parameter (for SYSEX data), size is stored to param1, param2 indicates if pointer should be freed (dynamic if TRUE) */
  unsigned int dtime;       /* Delay (ticks) between this and previous event. midi tracks. */
  unsigned int param1;      /* First parameter */
  unsigned int param2;      /* Second parameter */
  unsigned char type;       /* MIDI event type */
  unsigned char channel;    /* MIDI channel */
};

#define FLUID_MIDI_PARSER_MAX_DATA_SIZE 1024    /**< Maximum size of MIDI parameters/data (largest is SYSEX data) */

/*
 * fluid_midi_parser_t
 */
struct _fluid_midi_parser_t {
  unsigned char status;           /* Identifies the type of event, that is currently received ('Noteon', 'Pitch Bend' etc). */
  unsigned char channel;          /* The channel of the event that is received (in case of a channel event) */
  unsigned int nr_bytes;          /* How many bytes have been read for the current event? */
  unsigned int nr_bytes_total;    /* How many bytes does the current event type include? */
  unsigned char data[FLUID_MIDI_PARSER_MAX_DATA_SIZE]; /* The parameters or SYSEX data */
  fluid_midi_event_t event;        /* The event, that is returned to the MIDI driver. */
};

#ifndef FLUIDSYNTH_API
#define FLUIDSYNTH_API
#endif

FLUIDSYNTH_API fluid_midi_event_t* new_fluid_midi_event(void);
FLUIDSYNTH_API int delete_fluid_midi_event(fluid_midi_event_t* event);

FLUIDSYNTH_API int fluid_midi_event_set_type(fluid_midi_event_t* evt, int type);
FLUIDSYNTH_API int fluid_midi_event_get_type(fluid_midi_event_t* evt);
FLUIDSYNTH_API int fluid_midi_event_set_channel(fluid_midi_event_t* evt, int chan);
FLUIDSYNTH_API int fluid_midi_event_get_channel(fluid_midi_event_t* evt);
FLUIDSYNTH_API int fluid_midi_event_get_key(fluid_midi_event_t* evt);
FLUIDSYNTH_API int fluid_midi_event_set_key(fluid_midi_event_t* evt, int key);
FLUIDSYNTH_API int fluid_midi_event_get_velocity(fluid_midi_event_t* evt);
FLUIDSYNTH_API int fluid_midi_event_set_velocity(fluid_midi_event_t* evt, int vel);
FLUIDSYNTH_API int fluid_midi_event_get_control(fluid_midi_event_t* evt);
FLUIDSYNTH_API int fluid_midi_event_set_control(fluid_midi_event_t* evt, int ctrl);
FLUIDSYNTH_API int fluid_midi_event_get_value(fluid_midi_event_t* evt);
FLUIDSYNTH_API int fluid_midi_event_set_value(fluid_midi_event_t* evt, int val);
FLUIDSYNTH_API int fluid_midi_event_get_program(fluid_midi_event_t* evt);
FLUIDSYNTH_API int fluid_midi_event_set_program(fluid_midi_event_t* evt, int val);
FLUIDSYNTH_API int fluid_midi_event_get_pitch(fluid_midi_event_t* evt);
FLUIDSYNTH_API int fluid_midi_event_set_pitch(fluid_midi_event_t* evt, int val);
FLUIDSYNTH_API int fluid_midi_event_set_sysex(fluid_midi_event_t* evt, void *data,
                                              int size, int dynamic);

#endif /* _FLUID_MIDI_H */
