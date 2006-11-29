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
#include "utilities.h"
#include "timers.h"
#include "sound/sound.h"
#include "sound/midi.h"

struct midi_out_plugin_wr {
  struct midi_out_plugin plugin;
  int initialized;
};
struct midi_in_plugin_wr {
  struct midi_in_plugin plugin;
  int initialized;
};

#define MAX_OUT_PLUGINS 5
/* support only 1 input plugin for now to avoid the concurrent writes */
#define MAX_IN_PLUGINS 1
static struct midi_out_plugin_wr out[MAX_OUT_PLUGINS];
static struct midi_in_plugin_wr in[MAX_IN_PLUGINS];
static int out_registered = 0, in_registered = 0;
static struct rng_s midi_in;

void midi_write(unsigned char val)
{
  int i;
  for (i = 0; i < out_registered; i++)
    if (out[i].initialized)
      out[i].plugin.write(val);
//  idle(0, 0, 0, 0, "midi");
}

void midi_init(void)
{
  int i;
  rng_init(&midi_in, 64, 1);
  for (i = 0; i < out_registered; i++)
    out[i].initialized = out[i].plugin.init();
  for (i = 0; i < in_registered; i++)
    in[i].initialized = in[i].plugin.init();
}

void midi_done(void)
{
  int i;
  for (i = 0; i < out_registered; i++)
    if (out[i].initialized)
      out[i].plugin.done();
  for (i = 0; i < in_registered; i++)
    if (in[i].initialized)
      in[i].plugin.done();
  rng_destroy(&midi_in);
}

void midi_reset(void)
{
  int i;
  for (i = 0; i < out_registered; i++)
    if (out[i].initialized)
      out[i].plugin.reset();
  for (i = 0; i < in_registered; i++)
    if (in[i].initialized)
      in[i].plugin.reset();
}

void midi_stop(void)
{
  int i;
  for (i = 0; i < out_registered; i++)
    if (out[i].plugin.stop && out[i].initialized)
      out[i].plugin.stop();
  for (i = 0; i < in_registered; i++)
    if (in[i].plugin.stop && in[i].initialized)
      in[i].plugin.stop();
}

void midi_put_data(char *buf, size_t size)
{
  rng_add(&midi_in, size, buf);

  run_new_sb();
}

int midi_get_data_byte(char *buf)
{
  if (!rng_count(&midi_in))
    return 0;
  return rng_get(&midi_in, buf);
}

int midi_register_output_plugin(struct midi_out_plugin plugin)
{
  int index;
  S_printf("MIDI: Registering output plugin: %s\n", plugin.name);
  if (out_registered >= MAX_OUT_PLUGINS) {
    error("Cannot register midi plugin %s\n", plugin.name);
    return 0;
  }
  index = out_registered++;
  out[index].plugin = plugin;
  out[index].initialized = 0;
  return 1;
}

int midi_register_input_plugin(struct midi_in_plugin plugin)
{
  int index;
  S_printf("MIDI: Registering input plugin: %s\n", plugin.name);
  if (in_registered >= MAX_IN_PLUGINS) {
    error("Cannot register midi plugin %s\n", plugin.name);
    return 0;
  }
  index = in_registered++;
  in[index].plugin = plugin;
  in[index].initialized = 0;
  return 1;
}
