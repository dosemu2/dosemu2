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
#ifndef _SOUND_H
#define _SOUND_H

/* This is the correct way to run an SB timer tick */
extern void run_new_sb(void);

extern void sound_new_init(void);
extern void sound_new_reset(void);
extern void sound_new_done(void);

struct player_params {
  int rate;
  int format;
  int channels;
};

struct player_callbacks {
  size_t (*get_data)(void *buf, size_t size, struct player_params *params);
};

struct clocked_player {
  const char *name;
  int (*open)(void);
  void (*close)(void);
  void (*start)(void);
  void (*stop)(void);
  void (*timer)(void);
  void (*lock)(void);
  void (*unlock)(void);
};

struct unclocked_player {
  const char *name;
  int (*open)(struct player_params *params);
  void (*close)(void);
  void (*timer)(void);
  size_t (*write)(void *buf, size_t size);
};

extern int pcm_register_clocked_player(struct clocked_player player,
    struct player_callbacks *callbacks);
extern int pcm_register_unclocked_player(struct unclocked_player player);

/** PCM sample format */
enum _PCM_format {
	PCM_FORMAT_NONE,
	/** Signed 8 bit */
	PCM_FORMAT_S8,
	/** Unsigned 8 bit */
	PCM_FORMAT_U8,
	/** Signed 16 bit Little Endian */
	PCM_FORMAT_S16_LE,
	/** Unsigned 16 bit Little Endian */
	PCM_FORMAT_U16_LE,
	/** Signed 32 bit Little Endian */
	PCM_FORMAT_S32_LE,
	/** Unsigned 32 bit Little Endian */
	PCM_FORMAT_U32_LE,
	/** Float 32 bit Little Endian, Range -1.0 to 1.0 */
	PCM_FORMAT_FLOAT_LE,
	/** Ima-ADPCM */
	PCM_FORMAT_IMA_ADPCM,
};

#endif		/* EMU_SOUND_H */
