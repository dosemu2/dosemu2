/* This is a rip-off of the fluid_midi.c found in fluidsynth sources,
 * done by stsp for dosemu2 project.
 *
 * Note: fluidsynth parser API is both buggy:
 * http://sourceforge.net/p/fluidsynth/tickets/150/
 * and no longer exported:
 * https://sourceforge.net/p/fluidsynth/tickets/109/
 * So just rip off the code and use it privately.
 */

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

#include "fluid_midi.h"
#include "fluid_compat.h"

static int fluid_midi_event_length(unsigned char event);

/**
 * Create a MIDI event structure.
 * @return New MIDI event structure or NULL when out of memory.
 */
fluid_midi_event_t *
new_fluid_midi_event ()
{
    fluid_midi_event_t* evt;
    evt = FLUID_NEW(fluid_midi_event_t);
    if (evt == NULL) {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }
    evt->dtime = 0;
    evt->type = 0;
    evt->channel = 0;
    evt->param1 = 0;
    evt->param2 = 0;
    evt->next = NULL;
    evt->paramptr = NULL;
    return evt;
}

/**
 * Delete MIDI event structure.
 * @param evt MIDI event structure
 * @return Always returns #FLUID_OK
 */
int
delete_fluid_midi_event(fluid_midi_event_t *evt)
{
    fluid_midi_event_t *temp;

    while (evt) {
        temp = evt->next;

        /* Dynamic SYSEX event? - free (param2 indicates if dynamic) */
        if (evt->type == MIDI_SYSEX && evt->paramptr && evt->param2)
            FLUID_FREE (evt->paramptr);

        FLUID_FREE(evt);
        evt = temp;
    }
    return FLUID_OK;
}

/**
 * Get the event type field of a MIDI event structure.
 * @param evt MIDI event structure
 * @return Event type field (MIDI status byte without channel)
 */
int
fluid_midi_event_get_type(fluid_midi_event_t *evt)
{
    return evt->type;
}

/**
 * Set the event type field of a MIDI event structure.
 * @param evt MIDI event structure
 * @param type Event type field (MIDI status byte without channel)
 * @return Always returns #FLUID_OK
 */
int
fluid_midi_event_set_type(fluid_midi_event_t *evt, int type)
{
    evt->type = type;
    return FLUID_OK;
}

/**
 * Get the channel field of a MIDI event structure.
 * @param evt MIDI event structure
 * @return Channel field
 */
int
fluid_midi_event_get_channel(fluid_midi_event_t *evt)
{
    return evt->channel;
}

/**
 * Set the channel field of a MIDI event structure.
 * @param evt MIDI event structure
 * @param chan MIDI channel field
 * @return Always returns #FLUID_OK
 */
int
fluid_midi_event_set_channel(fluid_midi_event_t *evt, int chan)
{
    evt->channel = chan;
    return FLUID_OK;
}

/**
 * Get the key field of a MIDI event structure.
 * @param evt MIDI event structure
 * @return MIDI note number (0-127)
 */
int
fluid_midi_event_get_key(fluid_midi_event_t *evt)
{
    return evt->param1;
}

/**
 * Set the key field of a MIDI event structure.
 * @param evt MIDI event structure
 * @param v MIDI note number (0-127)
 * @return Always returns #FLUID_OK
 */
int
fluid_midi_event_set_key(fluid_midi_event_t *evt, int v)
{
    evt->param1 = v;
    return FLUID_OK;
}

/**
 * Get the velocity field of a MIDI event structure.
 * @param evt MIDI event structure
 * @return MIDI velocity number (0-127)
 */
int
fluid_midi_event_get_velocity(fluid_midi_event_t *evt)
{
    return evt->param2;
}

/**
 * Set the velocity field of a MIDI event structure.
 * @param evt MIDI event structure
 * @param v MIDI velocity value
 * @return Always returns #FLUID_OK
 */
int
fluid_midi_event_set_velocity(fluid_midi_event_t *evt, int v)
{
    evt->param2 = v;
    return FLUID_OK;
}

/**
 * Get the control number of a MIDI event structure.
 * @param evt MIDI event structure
 * @return MIDI control number
 */
int
fluid_midi_event_get_control(fluid_midi_event_t *evt)
{
    return evt->param1;
}

/**
 * Set the control field of a MIDI event structure.
 * @param evt MIDI event structure
 * @param v MIDI control number
 * @return Always returns #FLUID_OK
 */
int
fluid_midi_event_set_control(fluid_midi_event_t *evt, int v)
{
    evt->param1 = v;
    return FLUID_OK;
}

/**
 * Get the value field from a MIDI event structure.
 * @param evt MIDI event structure
 * @return Value field
 */
int
fluid_midi_event_get_value(fluid_midi_event_t *evt)
{
    return evt->param2;
}

/**
 * Set the value field of a MIDI event structure.
 * @param evt MIDI event structure
 * @param v Value to assign
 * @return Always returns #FLUID_OK
 */
int
fluid_midi_event_set_value(fluid_midi_event_t *evt, int v)
{
    evt->param2 = v;
    return FLUID_OK;
}

/**
 * Get the program field of a MIDI event structure.
 * @param evt MIDI event structure
 * @return MIDI program number (0-127)
 */
int
fluid_midi_event_get_program(fluid_midi_event_t *evt)
{
    return evt->param1;
}

/**
 * Set the program field of a MIDI event structure.
 * @param evt MIDI event structure
 * @param val MIDI program number (0-127)
 * @return Always returns #FLUID_OK
 */
int
fluid_midi_event_set_program(fluid_midi_event_t *evt, int val)
{
    evt->param1 = val;
    return FLUID_OK;
}

/**
 * Get the pitch field of a MIDI event structure.
 * @param evt MIDI event structure
 * @return Pitch value (14 bit value, 0-16383, 8192 is center)
 */
int
fluid_midi_event_get_pitch(fluid_midi_event_t *evt)
{
    return evt->param1;
}

/**
 * Set the pitch field of a MIDI event structure.
 * @param evt MIDI event structure
 * @param val Pitch value (14 bit value, 0-16383, 8192 is center)
 * @return Always returns FLUID_OK
 */
int
fluid_midi_event_set_pitch(fluid_midi_event_t *evt, int val)
{
    evt->param1 = val;
    return FLUID_OK;
}

/**
 * Assign sysex data to a MIDI event structure.
 * @param evt MIDI event structure
 * @param data Pointer to SYSEX data
 * @param size Size of SYSEX data
 * @param dynamic TRUE if the SYSEX data has been dynamically allocated and
 *   should be freed when the event is freed (only applies if event gets destroyed
 *   with delete_fluid_midi_event())
 * @return Always returns #FLUID_OK
 *
 * NOTE: Unlike the other event assignment functions, this one sets evt->type.
 */
int
fluid_midi_event_set_sysex(fluid_midi_event_t *evt, void *data, int size, int dynamic)
{
    evt->type = MIDI_SYSEX;
    evt->paramptr = data;
    evt->param1 = size;
    evt->param2 = dynamic;
    return FLUID_OK;
}

/************************************************************************
 *       MIDI PARSER
 *
 */

/*
 * new_fluid_midi_parser
 */
fluid_midi_parser_t *
new_fluid_midi_parser ()
{
    fluid_midi_parser_t *parser;
    parser = FLUID_NEW(fluid_midi_parser_t);
    if (parser == NULL) {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }
    parser->status = 0; /* As long as the status is 0, the parser won't do anything -> no need to initialize all the fields. */
    return parser;
}

/*
 * delete_fluid_midi_parser
 */
int
delete_fluid_midi_parser(fluid_midi_parser_t *parser)
{
    FLUID_FREE(parser);
    return FLUID_OK;
}

/**
 * Parse a MIDI stream one character at a time.
 * @param parser Parser instance
 * @param c Next character in MIDI stream
 * @return A parsed MIDI event or NULL if none.  Event is internal and should
 *   not be modified or freed and is only valid until next call to this function.
 */
fluid_midi_event_t *
fluid_midi_parser_parse(fluid_midi_parser_t *parser, unsigned char c)
{
    fluid_midi_event_t *event;

    /* Real-time messages (0xF8-0xFF) can occur anywhere, even in the middle
     * of another message. */
    if (c >= 0xF8) {
        if (c == MIDI_SYSTEM_RESET) {
            parser->event.type = c;
            parser->status = 0; /* clear the status */
            return &parser->event;
        }

        return NULL;
    }

    /* Status byte? - If previous message not yet complete, it is discarded (re-sync). */
    if (c & 0x80) {
        /* Any status byte terminates SYSEX messages (not just 0xF7) */
        if (parser->status == MIDI_SYSEX && parser->nr_bytes > 0) {
            event = &parser->event;
            fluid_midi_event_set_sysex(event, parser->data, parser->nr_bytes,
                    FALSE);
        } else
            event = NULL;

        if (c < 0xF0) /* Voice category message? */
        {
            parser->channel = c & 0x0F;
            parser->status = c & 0xF0;

            /* The event consumes x bytes of data... (subtract 1 for the status byte) */
            parser->nr_bytes_total = fluid_midi_event_length(parser->status)
                    - 1;

            parser->nr_bytes = 0; /* 0  bytes read so far */
        } else if (c == MIDI_SYSEX) {
            parser->status = MIDI_SYSEX;
            parser->nr_bytes = 0;
        } else
            parser->status = 0; /* Discard other system messages (0xF1-0xF7) */

        return event; /* Return SYSEX event or NULL */
    }

    /* Data/parameter byte */

    /* Discard data bytes for events we don't care about */
    if (parser->status == 0)
        return NULL;

    /* Max data size exceeded? (SYSEX messages only really) */
    if (parser->nr_bytes == FLUID_MIDI_PARSER_MAX_DATA_SIZE) {
        parser->status = 0; /* Discard the rest of the message */
        return NULL;
    }

    /* Store next byte */
    parser->data[parser->nr_bytes++] = c;

    /* Do we still need more data to get this event complete? */
    if (parser->nr_bytes < parser->nr_bytes_total ||
            parser->status == MIDI_SYSEX)
        return NULL;

    /* Event is complete, return it.
     * Running status byte MIDI feature is also handled here. */
    parser->event.type = parser->status;
    parser->event.channel = parser->channel;
    parser->nr_bytes = 0; /* Reset data size, in case there are additional running status messages */

    switch (parser->status) {
        case NOTE_OFF:
        case NOTE_ON:
        case KEY_PRESSURE:
        case CONTROL_CHANGE:
        case PROGRAM_CHANGE:
        case CHANNEL_PRESSURE:
            parser->event.param1 = parser->data[0]; /* For example key number */
            parser->event.param2 = parser->data[1]; /* For example velocity */
            break;
        case PITCH_BEND:
            /* Pitch-bend is transmitted with 14-bit precision. */
            parser->event.param1 = (parser->data[1] << 7) | parser->data[0];
            break;
        default: /* Unlikely */
            return NULL;
    }

    return &parser->event;
}

/* Purpose:
 * Returns the length of a MIDI message. */
static int
fluid_midi_event_length(unsigned char event)
{
    switch (event & 0xF0) {
        case NOTE_OFF:
        case NOTE_ON:
        case KEY_PRESSURE:
        case CONTROL_CHANGE:
        case PITCH_BEND:
            return 3;
        case PROGRAM_CHANGE:
        case CHANNEL_PRESSURE:
            return 2;
    }
    switch (event) {
        case MIDI_TIME_CODE:
        case MIDI_SONG_SELECT:
        case 0xF4:
        case 0xF5:
            return 2;
        case MIDI_TUNE_REQUEST:
            return 1;
        case MIDI_SONG_POSITION:
            return 3;
    }
    return 1;
}
