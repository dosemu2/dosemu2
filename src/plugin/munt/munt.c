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
 * Purpose: munt midi synth
 *
 * Author: Stas Sergeev
 *
 */

#include <mt32emu/c_interface/c_interface.h>
#include "emu.h"
#include "init.h"
#include "sound/midi.h"
#include "sound/sound.h"

#define midomunt_name "munt"
#define midomunt_longname "MIDI Output: munt device"

//static mt32emu_context *ctx;

static int midomunt_init(void *arg)
{
    return 0;
}

static void midomunt_done(void *arg)
{
}

static void midomunt_write(unsigned char val)
{
}

static void midomunt_stop(void *arg)
{
}

static void midomunt_run(void)
{
}

static int midomunt_cfg(void *arg)
{
    return pcm_parse_cfg(config.midi_driver, midomunt_name);
}

static int midomunt_owns(void *id, void *arg)
{
    return ((enum MixChan)id == MC_MIDI);
}

static const struct midi_out_plugin midomunt = {
    .name = midomunt_name,
    .longname = midomunt_longname,
    .open = midomunt_init,
    .close = midomunt_done,
    .write = midomunt_write,
    .stop = midomunt_stop,
    .run = midomunt_run,
    .get_cfg = midomunt_cfg,
    .stype = ST_GM,
    .weight = MIDI_W_PCM | MIDI_W_PREFERRED,
};

static const struct pcm_recorder recorder = {
    .name = midomunt_name,
    .longname = midomunt_longname,
    .owns = midomunt_owns,
    .flags = PCM_F_PASSTHRU,
};

CONSTRUCTOR(static void midomunt_register(void))
{
    pcm_register_recorder(&recorder, NULL);
    midi_register_output_plugin(&midomunt);
}
