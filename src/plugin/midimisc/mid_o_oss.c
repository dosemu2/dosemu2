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
#include "emu.h"
#include "init.h"
#include "sound/midi.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
/* declare this here to avoid warnings from soundcard.h */
static void seqbuf_dump(void);
#include <sys/soundcard.h>
SEQ_DEFINEBUF(128);

static int seq_fd = -1;
#define midooss_name "oss_midi"
#define midooss_longname "MIDI Output: OSS sequencer"

static void seqbuf_dump(void)
{
    if (_seqbufptr) {
	if (seq_fd != -1)
	    write(seq_fd, _seqbuf, _seqbufptr);
	_seqbufptr = 0;
    }
}

static int midooss_init(void *arg)
{
    char *name = "/dev/sequencer";
    seq_fd = RPT_SYSCALL(open(name, O_WRONLY));
    if (seq_fd == -1) {
	S_printf("%s: unable to open %s for writing: %s\n",
		 midooss_name, name, strerror(errno));
	return 0;
    }
    return 1;
}

static void midooss_done(void *arg)
{
    if (seq_fd == -1)
	return;
    close(seq_fd);
    seq_fd = -1;
}

static void midooss_write(unsigned char val)
{
    SEQ_MIDIOUT(0, val);
    SEQ_DUMPBUF();
}

static int midooss_cfg(void *arg)
{
    return pcm_parse_cfg(config.midi_driver, midooss_name);
}

static const struct midi_out_plugin midooss = {
    .name = midooss_name,
    .longname = midooss_longname,
    .open = midooss_init,
    .close = midooss_done,
    .write = midooss_write,
    .get_cfg = midooss_cfg,
    .stype = ST_GM,
};

CONSTRUCTOR(static void midooss_register(void))
{
    midi_register_output_plugin(&midooss);
}
