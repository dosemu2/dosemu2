/*
 *  Copyright (C) 2015 Stas Sergeev <stsp@users.sourceforge.net>
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
 * Purpose: midi file writer
 *
 * Author: Stas Sergeev
 *
 * This code is a mix of fluidsynth output plugin and the file output
 * plugin from timidity. The timidity file output plugin, in turn, is
 * a port of dosemu's midid file output code by Rob Komar and Stas Sergeev.
 * So this code made a really long way.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fluidsynth.h>
#include "fluid_midi.h"
#include "emu.h"
#include "init.h"
#include "timers.h"
#include "sound/midi.h"


#define midofile_name "MIDI Output: midi file"
static fluid_midi_parser_t* parser;
static int output_running;
static long long mf_time_base;

#define IGNORE_TEMPO_EVENTS 0

static char *midibuf;
static size_t midi_pos;
static FILE *fp;

static long track_size_pos;
static double last_time;

static int tempo;
static int ticks_per_quarter_note;
#define TICKS_OFFSET 12

#define BE_SHORT htons
#define BE_LONG htonl

static size_t m_fwrite(const void *ptr, size_t size);
#define M_FWRITE_STR(s) m_fwrite(s, sizeof(s) - 1)
#define M_FWRITE1(a) do { \
    uint8_t __arg[1]; \
    __arg[0] = a; \
    m_fwrite(__arg, 1); \
} while (0)
#define M_FWRITE2(a, b) do { \
    uint8_t __arg[2]; \
    __arg[0] = a; \
    __arg[1] = b; \
    m_fwrite(__arg, 2); \
} while (0)
#define M_FWRITE3(a, b, c) do { \
    uint8_t __arg[3]; \
    __arg[0] = a; \
    __arg[1] = b; \
    __arg[2] = c; \
    m_fwrite(__arg, 3); \
} while (0)

static size_t m_fwrite(const void *ptr, size_t size)
{
    return fwrite(ptr, size, 1, fp);
}

static void write_midi_header(void)
{
    /* Write out MID file header.
     * The file will have a single track, with the configured number of
     * ticks per quarter note.
     */
    M_FWRITE_STR("MThd");
    M_FWRITE_STR("\0\0\0\6");		/* header size */
    M_FWRITE_STR("\0\0");		/* single track format */
    M_FWRITE_STR("\0\1");		/* #tracks = 1 */
    M_FWRITE_STR("\0\0");		/* #ticks / quarter note written later */
}

static void finalize_midi_header(void)
{
    uint16_t tpqn = BE_SHORT(ticks_per_quarter_note);

    fflush(fp);
    memcpy(midibuf + TICKS_OFFSET, &tpqn, 2);	/* #ticks / quarter note */
}

static void set_tempo(void)
{
    M_FWRITE_STR("\xff\x51\3");
    M_FWRITE3(tempo >> 16, tempo >> 8, tempo);
}

static void set_time_sig(void)
{
    /* Set the time sig to 4/4 */
    M_FWRITE_STR("\xff\x58\4\4\x2\x18\x08");
}

static void midout_write_delta_time(int32_t time)
{
    int32_t delta_time;
    unsigned char c[4];
    int idx;
    int started_printing = 0;

#if !IGNORE_TEMPO_EVENTS
    double div;
    delta_time = time - last_time;
    div = tempo / (double)ticks_per_quarter_note;
    delta_time /= div;
    last_time += delta_time * div;
#else
    delta_time = time - last_time;
    last_time += delta_time;
#endif

    /* We have to divide the number of ticks into 7-bit segments, and only write
     * the non-zero segments starting with the most significant (except for the
     * least significant segment, which we always write).  The most significant bit
     * is set to 1 in all but the least significant segment.
     */
    c[0] = (delta_time >> 21) & 0x7f;
    c[1] = (delta_time >> 14) & 0x7f;
    c[2] = (delta_time >> 7) & 0x7f;
    c[3] = (delta_time) & 0x7f;

    for (idx = 0; idx < 3; idx++) {
	if (started_printing || c[idx]) {
	    started_printing = 1;
	    M_FWRITE1(c[idx] | 0x80);
	}
    }
    M_FWRITE1(c[3]);
}

static void start_midi_track(void)
{
    /* Write out track header.
     * The track will have a large length (0x7fffffff) because we don't know at
     * this time how big it will really be.
     */
    M_FWRITE_STR("MTrk");
    fflush(fp);
    track_size_pos = midi_pos;
    M_FWRITE_STR("\x7f\xff\xff\xff");	/* #chunks */

    last_time = 0;

#if !IGNORE_TEMPO_EVENTS
    tempo = 500000;
#else
    tempo = ticks_per_quarter_note;
#endif

    midout_write_delta_time(0);
    set_tempo();
    midout_write_delta_time(0);
    set_time_sig();
}

static void end_midi_track(void)
{
    int32_t track_bytes;
    /* Send (with delta-time of 0) "0xff 0x2f 0x0" to finish the track. */
    M_FWRITE_STR("\0\xff\x2f\0");

    fflush(fp);

    track_bytes = BE_LONG(midi_pos - track_size_pos - 4);
    memcpy(midibuf + track_size_pos, &track_bytes, 4);
}

static int open_output(void)
{
    fp = open_memstream(&midibuf, &midi_pos);

    ticks_per_quarter_note = 144;
    write_midi_header();

    return 0;
}

static void close_output(void)
{
    int fd;
    finalize_midi_header();

    fclose(fp);
    fd = open(config.midi_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
	error("MIDI: failed to open %s: %s\n", config.midi_file,
		strerror(errno));
	goto out;
    }
    write(fd, midibuf, midi_pos);
    close(fd);
out:
    free(midibuf);
}

static void midout_noteon(int chn, int note, int vel, int32_t time)
{
    midout_write_delta_time(time);
    M_FWRITE3((chn & 0x0f) | NOTE_ON, note & 0x7f, vel & 0x7f);
}

static void midout_noteoff(int chn, int note, int vel, int32_t time)
{
    midout_write_delta_time(time);
    M_FWRITE3((chn & 0x0f) | NOTE_OFF, note & 0x7f, vel & 0x7f);
}

static void midout_control(int chn, int control, int value, int32_t time)
{
    midout_write_delta_time(time);
    M_FWRITE3((chn & 0x0f) | CONTROL_CHANGE, control & 0x7f, value & 0x7f);
}

static void midout_keypressure(int chn, int control, int value, int32_t time)
{
    midout_write_delta_time(time);
    M_FWRITE3((chn & 0x0f) | KEY_PRESSURE, control & 0x7f, value & 0x7f);
}

static void midout_channelpressure(int chn, int vel, int32_t time)
{
    midout_write_delta_time(time);
    M_FWRITE2((chn & 0x0f) | CHANNEL_PRESSURE, vel & 0x7f);
}

static void midout_bender(int chn, int pitch, int32_t time)
{
    midout_write_delta_time(time);
    M_FWRITE3((chn & 0x0f) | PITCH_BEND, pitch & 0x7f, (pitch >> 7) & 0x7f);
}

static void midout_program(int chn, int pgm, int32_t time)
{
    midout_write_delta_time(time);
    M_FWRITE2((chn & 0x0f) | PROGRAM_CHANGE, pgm & 0x7f);
}

static void midout_tempo(int chn, int a, int b, int32_t time)
{
    midout_write_delta_time(time);
    tempo = (a << 16) | (b << 8) | chn;
    set_tempo();
}

#if 0
static int find_bit(int val)
{
    int i = 0;
    while (val) {
	if (val & 1)
	    return i;
	i++;
	val >>= 1;
    }
    return -1;
}

static void midout_timesig(int chn, int a, int b, int32_t time)
{
    if (chn == 0) {
	if (!b)
	    return;
	b = find_bit(b);
	midout_write_delta_time(time);
	M_FWRITE_STR("\xff\x58\4");
    }
    M_FWRITE2(a, b);
}
#endif

static void midout_sysex(void *data, int len, int32_t time)
{
    uint8_t a;
    int l1 = len + 1;
    midout_write_delta_time(time);
    M_FWRITE1(0xf0);
    a = l1 & 0x7f;
    if (l1 != a) {
	uint8_t b = ((l1 >> 7) & 0x7f) | 0x80;
	M_FWRITE1(b);
    }
    M_FWRITE1(a);
    m_fwrite(data, len);
    M_FWRITE1(0xf7);
}

static void do_event(fluid_midi_event_t *ev, int32_t time)
{
    int ch = fluid_midi_event_get_channel(ev);
    switch (fluid_midi_event_get_type(ev)) {
    case NOTE_ON:
	midout_noteon(ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_velocity(ev), time);
	break;
    case NOTE_OFF:
	midout_noteoff(ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_velocity(ev), time);
	break;
    case KEY_PRESSURE:
	midout_keypressure(ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_value(ev), time);
	break;
    case PROGRAM_CHANGE:
	midout_program(ch, fluid_midi_event_get_program(ev), time);
	break;
    case CHANNEL_PRESSURE:
	midout_channelpressure(ch, fluid_midi_event_get_value(ev), time);
	break;
    case PITCH_BEND:
	midout_bender(ch, fluid_midi_event_get_pitch(ev), time);
	break;
    case CONTROL_CHANGE:
	midout_control(ch, fluid_midi_event_get_control(ev),
		fluid_midi_event_get_value(ev), time);
	break;
    case MIDI_TIME_CODE:
#if !IGNORE_TEMPO_EVENTS
	midout_tempo(ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_value(ev), time);
#endif
	break;
#if 0
    case ME_TIMESIG:
	midout_timesig(ch, ev->a, ev->b, time);
	break;
#endif
    case MIDI_SYSEX:
	midout_sysex(ev->paramptr, ev->param1, time);
	break;
    }
}

static int midofile_init(void *arg)
{
    parser = new_fluid_midi_parser();
    return 1;
}

static void midofile_done(void *arg)
{
    delete_fluid_midi_parser(parser);
}

static void midofile_start(void)
{
    S_printf("MIDI: starting fluidsynth\n");
    mf_time_base = GETusTIME(0);
    open_output();
    output_running = 1;
    start_midi_track();
}

static void midofile_write(unsigned char val)
{
    fluid_midi_event_t* event;

    if (!output_running)
	midofile_start();

    event = fluid_midi_parser_parse(parser, val);
    if (event != NULL) {
	unsigned long long now = GETusTIME(0);
	int32_t usec = now - mf_time_base;
	if (debug_level('S') >= 5)
	    S_printf("MIDI: sending event to fluidsynth, usec=%i\n", usec);
	do_event(event, usec);
    }
}

static void midofile_stop(void *arg)
{
    if (!output_running)
	return;
    end_midi_track();
    close_output();
    output_running = 0;
}

static int midofile_get_cfg(void *arg)
{
    if (config.midi_file && config.midi_file[0])
	return PCM_CF_ENABLED;
    return 0;
}

static const struct midi_out_plugin midofile = {
    .name = midofile_name,
    .open = midofile_init,
    .close = midofile_done,
    .write = midofile_write,
    .stop = midofile_stop,
    .get_cfg = midofile_get_cfg,
    .stype = ST_ANY,
    .flags = PCM_F_PASSTHRU | PCM_F_EXPLICIT,
};

CONSTRUCTOR(static void midofile_register(void))
{
    midi_register_output_plugin(&midofile);
}
