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
#include "midi/fluid_midi.h"
#include "emu.h"
#include "init.h"
#include "timers.h"
#include "sound/midi.h"
#include "midi/mt32remap.h"

#define midofile_name "MIDI Output: midi file"
#define midofile_name_mt32 "MIDI Output: midi file mt32"
struct mid_state {
    fluid_midi_parser_t* parser;
    mt32_t *mt;
    int output_running;
    long long mf_time_base;
    char *midibuf;
    size_t midi_pos;
    FILE *fp;
    long track_size_pos;
    double last_time;
    int tempo;
    int ticks_per_quarter_note;
    char *fname;
};
static struct mid_state mids[ST_MAX];

#define IGNORE_TEMPO_EVENTS 0
#define TICKS_OFFSET 12

#define BE_SHORT htons
#define BE_LONG htonl

static size_t m_fwrite(struct mid_state *ms, const void *ptr, size_t size);
#define M_FWRITE_STR(m, s) m_fwrite(m, s, sizeof(s) - 1)
#define M_FWRITE1(m, a) do { \
    uint8_t __arg[1]; \
    __arg[0] = a; \
    m_fwrite(m, __arg, 1); \
} while (0)
#define M_FWRITE2(m, a, b) do { \
    uint8_t __arg[2]; \
    __arg[0] = a; \
    __arg[1] = b; \
    m_fwrite(m, __arg, 2); \
} while (0)
#define M_FWRITE3(m, a, b, c) do { \
    uint8_t __arg[3]; \
    __arg[0] = a; \
    __arg[1] = b; \
    __arg[2] = c; \
    m_fwrite(m, __arg, 3); \
} while (0)

static size_t m_fwrite(struct mid_state *ms, const void *ptr, size_t size)
{
    return fwrite(ptr, size, 1, ms->fp);
}

static void write_midi_header(struct mid_state *ms)
{
    /* Write out MID file header.
     * The file will have a single track, with the configured number of
     * ticks per quarter note.
     */
    M_FWRITE_STR(ms, "MThd");
    M_FWRITE_STR(ms, "\0\0\0\6");		/* header size */
    M_FWRITE_STR(ms, "\0\0");		/* single track format */
    M_FWRITE_STR(ms, "\0\1");		/* #tracks = 1 */
    M_FWRITE_STR(ms, "\0\0");		/* #ticks / quarter note written later */
}

static void finalize_midi_header(struct mid_state *ms)
{
    uint16_t tpqn = BE_SHORT(ms->ticks_per_quarter_note);

    fflush(ms->fp);
    memcpy(ms->midibuf + TICKS_OFFSET, &tpqn, 2);	/* #ticks / quarter note */
}

static void set_tempo(struct mid_state *ms)
{
    M_FWRITE_STR(ms, "\xff\x51\3");
    M_FWRITE3(ms, ms->tempo >> 16, ms->tempo >> 8, ms->tempo);
}

static void set_time_sig(struct mid_state *ms)
{
    /* Set the time sig to 4/4 */
    M_FWRITE_STR(ms, "\xff\x58\4\4\x2\x18\x08");
}

static void midout_write_delta_time(struct mid_state *ms, int32_t time)
{
    int32_t delta_time;
    unsigned char c[4];
    int idx;
    int started_printing = 0;

#if !IGNORE_TEMPO_EVENTS
    double div;
    delta_time = time - ms->last_time;
    div = ms->tempo / (double)ms->ticks_per_quarter_note;
    delta_time /= div;
    ms->last_time += delta_time * div;
#else
    delta_time = time - ms->last_time;
    ms->last_time += delta_time;
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
	    M_FWRITE1(ms, c[idx] | 0x80);
	}
    }
    M_FWRITE1(ms, c[3]);
}

static void start_midi_track(struct mid_state *ms)
{
    /* Write out track header.
     * The track will have a large length (0x7fffffff) because we don't know at
     * this time how big it will really be.
     */
    M_FWRITE_STR(ms, "MTrk");
    fflush(ms->fp);
    ms->track_size_pos = ms->midi_pos;
    M_FWRITE_STR(ms, "\x7f\xff\xff\xff");	/* #chunks */

    ms->last_time = 0;

#if !IGNORE_TEMPO_EVENTS
    ms->tempo = 500000;
#else
    ms->tempo = ms->ticks_per_quarter_note;
#endif

    midout_write_delta_time(ms, 0);
    set_tempo(ms);
    midout_write_delta_time(ms, 0);
    set_time_sig(ms);
}

static void end_midi_track(struct mid_state *ms)
{
    int32_t track_bytes;
    /* Send (with delta-time of 0) "0xff 0x2f 0x0" to finish the track. */
    M_FWRITE_STR(ms, "\0\xff\x2f\0");

    fflush(ms->fp);

    track_bytes = BE_LONG(ms->midi_pos - ms->track_size_pos - 4);
    memcpy(ms->midibuf + ms->track_size_pos, &track_bytes, 4);
}

static int open_output(struct mid_state *ms)
{
    ms->fp = open_memstream(&ms->midibuf, &ms->midi_pos);

    ms->ticks_per_quarter_note = 144;
    write_midi_header(ms);

    return 0;
}

static void close_output(struct mid_state *ms)
{
    int fd;
    finalize_midi_header(ms);

    fclose(ms->fp);
    fd = open(ms->fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
	error("MIDI: failed to open %s: %s\n", config.midi_file,
		strerror(errno));
	goto out;
    }
    write(fd, ms->midibuf, ms->midi_pos);
    close(fd);
out:
    free(ms->midibuf);
}

static void midout_noteon(struct mid_state *ms, int chn, int note, int vel, int32_t time)
{
    midout_write_delta_time(ms, time);
    M_FWRITE3(ms, (chn & 0x0f) | NOTE_ON, note & 0x7f, vel & 0x7f);
}

static void midout_noteoff(struct mid_state *ms, int chn, int note, int vel, int32_t time)
{
    midout_write_delta_time(ms, time);
    M_FWRITE3(ms, (chn & 0x0f) | NOTE_OFF, note & 0x7f, vel & 0x7f);
}

static void midout_control(struct mid_state *ms, int chn, int control, int value, int32_t time)
{
    midout_write_delta_time(ms, time);
    M_FWRITE3(ms, (chn & 0x0f) | CONTROL_CHANGE, control & 0x7f, value & 0x7f);
}

static void midout_keypressure(struct mid_state *ms, int chn, int control, int value, int32_t time)
{
    midout_write_delta_time(ms, time);
    M_FWRITE3(ms, (chn & 0x0f) | KEY_PRESSURE, control & 0x7f, value & 0x7f);
}

static void midout_channelpressure(struct mid_state *ms, int chn, int vel, int32_t time)
{
    midout_write_delta_time(ms, time);
    M_FWRITE2(ms, (chn & 0x0f) | CHANNEL_PRESSURE, vel & 0x7f);
}

static void midout_bender(struct mid_state *ms, int chn, int pitch, int32_t time)
{
    midout_write_delta_time(ms, time);
    M_FWRITE3(ms, (chn & 0x0f) | PITCH_BEND, pitch & 0x7f, (pitch >> 7) & 0x7f);
}

static void midout_program(struct mid_state *ms, int chn, int pgm, int32_t time)
{
    midout_write_delta_time(ms, time);
    M_FWRITE2(ms, (chn & 0x0f) | PROGRAM_CHANGE, pgm & 0x7f);
}

static void midout_tempo(struct mid_state *ms, int chn, int a, int b, int32_t time)
{
    midout_write_delta_time(ms, time);
    ms->tempo = (a << 16) | (b << 8) | chn;
    set_tempo(ms);
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

static void midout_timesig(struct mid_state *ms, int chn, int a, int b, int32_t time)
{
    if (chn == 0) {
	if (!b)
	    return;
	b = find_bit(b);
	midout_write_delta_time(ms, time);
	M_FWRITE_STR(ms, "\xff\x58\4");
    }
    M_FWRITE2(ms, a, b);
}
#endif

static void midout_sysex(struct mid_state *ms, void *data, int len, int32_t time)
{
    uint8_t a;
    int l1 = len + 1;
    midout_write_delta_time(ms, time);
    M_FWRITE1(ms, 0xf0);
    a = l1 & 0x7f;
    if (l1 != a) {
	uint8_t b = ((l1 >> 7) & 0x7f) | 0x80;
	M_FWRITE1(ms, b);
    }
    M_FWRITE1(ms, a);
    m_fwrite(ms, data, len);
    M_FWRITE1(ms, 0xf7);
}

static void do_event(struct mid_state *ms, fluid_midi_event_t *ev, int32_t time)
{
    int ch = fluid_midi_event_get_channel(ev);
    switch (fluid_midi_event_get_type(ev)) {
    case NOTE_ON:
	midout_noteon(ms, ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_velocity(ev), time);
	break;
    case NOTE_OFF:
	midout_noteoff(ms, ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_velocity(ev), time);
	break;
    case KEY_PRESSURE:
	midout_keypressure(ms, ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_value(ev), time);
	break;
    case PROGRAM_CHANGE:
	midout_program(ms, ch, fluid_midi_event_get_program(ev), time);
	break;
    case CHANNEL_PRESSURE:
	midout_channelpressure(ms, ch, fluid_midi_event_get_value(ev), time);
	break;
    case PITCH_BEND:
	midout_bender(ms, ch, fluid_midi_event_get_pitch(ev), time);
	break;
    case CONTROL_CHANGE:
	midout_control(ms, ch, fluid_midi_event_get_control(ev),
		fluid_midi_event_get_value(ev), time);
	break;
    case MIDI_TIME_CODE:
#if !IGNORE_TEMPO_EVENTS
	midout_tempo(ms, ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_value(ev), time);
#endif
	break;
#if 0
    case ME_TIMESIG:
	midout_timesig(ms, ch, ev->a, ev->b, time);
	break;
#endif
    case MIDI_SYSEX:
	midout_sysex(ms, ev->paramptr, ev->param1, time);
	break;
    }
}

static int midofile_init(void *arg)
{
    struct mid_state *ms = &mids[ST_GM];
    ms->mt = NULL;
    ms->parser = new_fluid_midi_parser();
    ms->fname = config.midi_file;
    return 1;
}

static int midofile_init_mt32(void *arg)
{
    struct mid_state *ms = &mids[ST_MT32];
    ms->mt = mt32remap_init();
    if (!ms->mt)
	return 0;
    ms->parser = new_fluid_midi_parser();
    ms->fname = config.midi_file_mt32;
    return 1;
}

static void midofile_done(void *arg)
{
    struct mid_state *ms = &mids[ST_GM];
    delete_fluid_midi_parser(ms->parser);
}

static void midofile_done_mt32(void *arg)
{
    struct mid_state *ms = &mids[ST_MT32];
    delete_fluid_midi_parser(ms->parser);
    mt32remap_done(ms->mt);
}

static void midofile_start(struct mid_state *ms)
{
    S_printf("MIDI: starting fluidsynth\n");
    ms->mf_time_base = GETusTIME(0);
    open_output(ms);
    ms->output_running = 1;
    start_midi_track(ms);
}

static void do_write(void *arg, unsigned char *data, int len)
{
    struct mid_state *ms = arg;
    assert(ms->parser->nr_bytes == 0);
    for (int i = 0; i < len; i++) {
	fluid_midi_event_t *event = fluid_midi_parser_parse(ms->parser, data[i]);
	if (event != NULL) {
	    unsigned long long now = GETusTIME(0);
	    int32_t usec = now - ms->mf_time_base;
	    do_event(ms, event, usec);
	}
    }
    assert(ms->parser->nr_bytes == 0);
}

static int do_mt32_event(struct mid_state *ms, fluid_midi_event_t *ev)
{
    int ch = fluid_midi_event_get_channel(ev);
    int e = fluid_midi_event_get_type(ev);
    if (e >= 0x80 && e <= 0xe0 && !mt32remap_channel_assigned(ms->mt, ch))
	return 1;
    switch (e) {
    case NOTE_ON:
	mt32remap_noteon(ms->mt, ch, fluid_midi_event_get_key(ev),
		fluid_midi_event_get_velocity(ev), do_write, ms);
	break;
    case PROGRAM_CHANGE:
	mt32remap_program(ms->mt, ch, fluid_midi_event_get_program(ev));
	return 1;
    case MIDI_SYSEX:
	mt32remap_sysex(ms->mt, ev->paramptr, ev->param1);
	break;
    }
    return 0;
}

static void do_write_byte(struct mid_state *ms, unsigned char val, enum SynthType type)
{
    fluid_midi_event_t* event;

    if (!ms->output_running)
	midofile_start(ms);

    event = fluid_midi_parser_parse(ms->parser, val);
    if (event != NULL) {
	fluid_midi_event_t event2 = *event;
	if (type == ST_GM || do_mt32_event(ms, event) == 0) {
	    unsigned long long now = GETusTIME(0);
	    int32_t usec = now - ms->mf_time_base;
	    if (debug_level('S') >= 5)
		S_printf("MIDI: sending event to fluidsynth, usec=%i\n", usec);
	    do_event(ms, &event2, usec);
	}
    }
}

static void midofile_write(unsigned char val, enum SynthType type)
{
    struct mid_state *ms = &mids[ST_GM];
    do_write_byte(ms, val, ST_GM);
}

static void midofile_write_mt32(unsigned char val, enum SynthType type)
{
    struct mid_state *ms = &mids[ST_MT32];
    do_write_byte(ms, val, ST_MT32);
}

static void midofile_stop(void *arg)
{
    struct mid_state *ms = &mids[ST_GM];
    if (!ms->output_running)
	return;
    end_midi_track(ms);
    close_output(ms);
    ms->output_running = 0;
}

static void midofile_stop_mt32(void *arg)
{
    struct mid_state *ms = &mids[ST_MT32];
    if (!ms->output_running)
	return;
    end_midi_track(ms);
    close_output(ms);
    ms->output_running = 0;
}

static int midofile_get_cfg(void *arg)
{
    if (config.midi_file && config.midi_file[0])
	return PCM_CF_ENABLED;
    return 0;
}

static int midofile_get_cfg_mt32(void *arg)
{
    if (config.midi_file_mt32 && config.midi_file_mt32[0])
	return PCM_CF_ENABLED;
    return 0;
}

static const struct midi_out_plugin midofile
#ifdef __cplusplus
{
    midofile_name,
    NULL,
    midofile_get_cfg,
    midofile_init,
    midofile_done,
    0,
    midofile_write,
    midofile_stop,
    NULL,
    PCM_F_PASSTHRU | PCM_F_EXPLICIT,
};
#else
= {
    .name = midofile_name,
    .get_cfg = midofile_get_cfg,
    .open = midofile_init,
    .close = midofile_done,
    .write = midofile_write,
    .stop = midofile_stop,
    .flags = PCM_F_PASSTHRU | PCM_F_EXPLICIT,
};
#endif

static const struct midi_out_plugin midofile_mt32
#ifdef __cplusplus
{
    midofile_name_mt32,
    NULL,
    midofile_get_cfg_mt32,
    midofile_init,
    midofile_done,
    0,
    midofile_write_mt32,
    midofile_stop,
    NULL,
    PCM_F_PASSTHRU | PCM_F_EXPLICIT,
};
#else
= {
    .name = midofile_name_mt32,
    .get_cfg = midofile_get_cfg_mt32,
    .open = midofile_init_mt32,
    .close = midofile_done_mt32,
    .write = midofile_write_mt32,
    .stop = midofile_stop_mt32,
    .flags = PCM_F_PASSTHRU | PCM_F_EXPLICIT,
};
#endif

static void mt32_scrub(void)
{
    midi_register_output_plugin(&midofile, ST_ANY);
    midi_register_output_plugin(&midofile_mt32, ST_MT32);
}

CONSTRUCTOR(static void midoflus_register(void))
{
    register_config_scrub(mt32_scrub);
}
