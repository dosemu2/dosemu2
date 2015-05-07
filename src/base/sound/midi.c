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
#include "ringbuf.h"
#include "timers.h"
#include "sound/sound.h"
#include "sound/midi.h"

struct midi_out_plugin_wr {
  struct midi_out_plugin plugin;
  int initialized:1;
  int failed:1;
};
struct midi_in_plugin_wr {
  struct midi_in_plugin plugin;
  int initialized:1;
};

#define MAX_OUT_PLUGINS 15
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
//  idle(0, 0, 0, "midi");
}

void midi_init(void)
{
  int i, sel, max_w, max_i;
  rng_init(&midi_in, 64, 1);

  sel = 0;
  for (i = 0; i < out_registered; i++) {
    if (out[i].plugin.selected) {
      out[i].initialized = out[i].plugin.open(out[i].plugin.arg);
      S_printf("MIDI: Initializing output plugin: %s: %s\n",
	  out[i].plugin.name, out[i].initialized ? "OK" : "Failed");
      sel++;
    }
  }
  if (!sel) do {
    max_w = -1;
    max_i = -1;
    for (i = 0; i < out_registered; i++) {
      if (out[i].initialized || out[i].failed ||
	    (out[i].plugin.flags & MIDI_F_EXPLICIT))
        continue;
      if (out[i].plugin.flags & MIDI_F_PASSTHRU) {
        out[i].initialized = out[i].plugin.open(out[i].plugin.arg);
        S_printf("MIDI: Initializing pass-through output plugin: %s: %s\n",
	    out[i].plugin.name, out[i].initialized ? "OK" : "Failed");
        if (!out[i].initialized)
          out[i].failed = 1;
      }
      if (out[i].plugin.weight > max_w) {
        max_w = out[i].plugin.weight;
        max_i = i;
      }
    }
    if (max_i != -1) {
      out[max_i].initialized = out[max_i].plugin.open(out[max_i].plugin.arg);
      S_printf("MIDI: Initializing output plugin: %s (w=%i): %s\n",
	  out[max_i].plugin.name, max_w,
	  out[max_i].initialized ? "OK" : "Failed");
      if (!out[max_i].initialized)
        out[max_i].failed = 1;
    }
  } while(max_i != -1 && !out[max_i].initialized);

  sel = 0;
  for (i = 0; i < in_registered; i++) {
    if (in[i].plugin.selected) {
      in[i].initialized = in[i].plugin.open(in[i].plugin.arg);
      S_printf("MIDI: Initializing input plugin: %s: %s\n",
	  in[i].plugin.name, in[i].initialized ? "OK" : "Failed");
      sel++;
    }
  }
  if (!sel) {
    for (i = 0; i < in_registered; i++) {
      in[i].initialized = in[i].plugin.open(in[i].plugin.arg);
      S_printf("MIDI: Initializing input plugin: %s: %s\n",
	  in[i].plugin.name, in[i].initialized ? "OK" : "Failed");
    }
  }
}

void midi_done(void)
{
  int i;
  for (i = 0; i < out_registered; i++) {
    if (out[i].initialized) {
      if (out[i].plugin.stop)
        out[i].plugin.stop();
      out[i].plugin.close(out[i].plugin.arg);
    }
  }
  for (i = 0; i < in_registered; i++) {
    if (in[i].initialized) {
      if (in[i].plugin.stop)
        in[i].plugin.stop();
      in[i].plugin.close(in[i].plugin.arg);
    }
  }
  rng_destroy(&midi_in);
}

void midi_reset(void)
{
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

void midi_timer(void)
{
  int i;
  for (i = 0; i < out_registered; i++)
    if (out[i].plugin.run && out[i].initialized)
      out[i].plugin.run();
}

void midi_put_data(unsigned char *buf, size_t size)
{
  rng_add(&midi_in, size, buf);

  run_new_sb();
}

int midi_get_data_byte(unsigned char *buf)
{
  if (!rng_count(&midi_in))
    return 0;
  return rng_get(&midi_in, buf);
}

int midi_register_output_plugin(struct midi_out_plugin plugin)
{
  int index;
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
  if (in_registered >= MAX_IN_PLUGINS) {
    error("Cannot register midi plugin %s\n", plugin.name);
    return 0;
  }
  index = in_registered++;
  in[index].plugin = plugin;
  in[index].initialized = 0;
  return 1;
}
