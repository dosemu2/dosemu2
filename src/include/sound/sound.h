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
#ifndef __SOUND_H__
#define __SOUND_H__

/* This is the correct way to run an SB timer tick */
extern void run_new_sb(void);
extern void run_new_sound(void);

extern void sound_new_init(void);
extern void sound_new_reset(void);
extern void sound_new_done(void);

struct player_params {
  int rate;
  int format;
  int channels;
  int handle;
};

#define PCM_F_PASSTHRU 1
#define PCM_F_EXPLICIT 2

typedef struct {
  const char *name;
  int (*open)(void *);
  void (*close)(void *);
  void *arg;

  int selected:1;
  int flags;
  int weight;
} pcm_base;

struct pcm_holder {
  const pcm_base *plugin;
  int id;
  int initialized:1;
  int failed:1;
};

struct pcm_player {
  pcm_base;
  void (*start)(void *);
  void (*stop)(void *);
  void (*timer)(double, void *);
  int id;
};

extern int pcm_register_player(struct pcm_player player);
extern void pcm_reset_player(int handle);
extern void pcm_init_plugins(struct pcm_holder *plu, int num);

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

enum { PCM_ID_P, PCM_ID_R, PCM_ID_MAX };

typedef int16_t sndbuf_t;
#define SNDBUF_CHANS 2

extern int pcm_init(void);
extern void pcm_done(void);
extern void pcm_reset(void);
extern int pcm_allocate_stream(int channels, char *name, int id);
extern void pcm_set_flag(int strm_idx, int flag);
extern void pcm_clear_flag(int strm_idx, int flag);
extern int pcm_flush(int strm_idx);
extern int pcm_samp_cutoff(int val, int format);
extern int pcm_get_format(int is_16, int is_signed);
extern int pcm_frag_size(double period, struct player_params *params);
extern double pcm_frame_period_us(int rate);
extern double pcm_frag_period(int size, struct player_params *params);
extern void pcm_write_interleaved(sndbuf_t ptr[][SNDBUF_CHANS],
	int frames, int rate, int format, int nchans, int strm_idx);
extern int pcm_format_size(int format);
extern void pcm_timer(void);
extern void pcm_prepare_stream(int strm_idx);
extern double pcm_time_lock(int strm_idx);
extern void pcm_time_unlock(int strm_idx);
extern double pcm_get_stream_time(int strm_idx);

size_t pcm_data_get(void *data, size_t size, struct player_params *params);
int pcm_data_get_interleaved(sndbuf_t buf[][SNDBUF_CHANS], int nframes,
	struct player_params *params);

#define PCM_FLAG_RAW 1
#define PCM_FLAG_POST 2
#define PCM_FLAG_SLTS 4

#endif
