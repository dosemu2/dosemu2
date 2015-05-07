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

#include "sound/sound.h"

#define MIDI_F_PASSTHRU 1
#define MIDI_F_EXPLICIT 2

#define MIDI_W_PREFERRED 1
#define MIDI_W_PCM 2

struct midi_out_plugin {
  pcm_base;
  void (*write)(unsigned char);
  void (*stop)(void);
  void (*run)(void);
  int selected:1;
  int flags;
  int weight;
};

struct midi_in_plugin {
  pcm_base;
  void (*stop)(void);
  int selected:1;
};

extern void midi_write(unsigned char val);
extern void midi_init(void);
extern void midi_done(void);
extern void midi_reset(void);
extern void midi_stop(void);
extern void midi_timer(void);
extern void midi_put_data(unsigned char *buf, size_t size);
extern int midi_get_data_byte(unsigned char *buf);
extern int midi_register_output_plugin(struct midi_out_plugin plugin);
extern int midi_register_input_plugin(struct midi_in_plugin plugin);
