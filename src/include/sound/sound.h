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

#include <inttypes.h>
#include <stddef.h>

typedef int16_t sndbuf_t;
#define SNDBUF_CHANS 2

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
  const char *longname;
  int (*get_cfg)(void *);
  int (*open)(void *);
  void (*close)(void *);

  int flags;
  int weight;
} pcm_base;

#define PCM_CF_ENABLED 1

typedef struct {
  pcm_base;
  void (*start)(void *);
  void (*stop)(void *);
} pcm_plugin_base;

struct pcm_holder {
  const pcm_base *plugin;
  void *arg;
  int opened:1;
  int failed:1;
  int cfg_flags;
  void *priv;
};

struct pcm_player {
  pcm_plugin_base;
  void (*timer)(double, void *);
  int id;
};

struct pcm_recorder {
  pcm_plugin_base;
  void *id2;
};

typedef int (*efp_process)(int handle, sndbuf_t buf[][SNDBUF_CHANS],
	int nframes, int channels, int format, int srate);

struct pcm_efp {
  pcm_base;
  void (*start)(int);
  void (*stop)(int);
  int (*setup)(int, int, float, void *);
  efp_process process;
};

enum EfpType { EFP_NONE, EFP_HPF };

extern int pcm_register_player(const struct pcm_player *player, void *arg);
extern int pcm_register_recorder(const struct pcm_recorder *player, void *arg);
extern int pcm_register_efp(const struct pcm_efp *efp, enum EfpType type,
	void *arg);
extern void pcm_reset_player(int handle);
extern int pcm_init_plugins(struct pcm_holder *plu, int num);
extern void pcm_deinit_plugins(struct pcm_holder *plu, int num);
extern int pcm_setup_efp(int handle, enum EfpType type, int param1, int param2,
	float param3);
extern int pcm_setup_hpf(struct player_params *params);
extern int pcm_parse_cfg(const char *string, const char *name);
extern char *pcm_parse_params(const char *string, const char *name,
	const char *param);

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

#define PCM_ID_P (1 << 0)
#define PCM_ID_R (1 << 1)
#define PCM_ID_MAX     2
#define PCM_ID_ANY 0xff

extern int pcm_init(void);
extern void pcm_done(void);
extern int pcm_allocate_stream(int channels, char *name, void *vol_arg);
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
extern int pcm_start_input(void *id);
extern void pcm_stop_input(void *id);
extern void pcm_set_volume_cb(double (*get_vol)(int, int, int, void *));
extern void pcm_set_connected_cb(int (*is_connected)(int, void *));
extern void pcm_set_checkid2_cb(int (*checkid2)(void *, void *));

size_t pcm_data_get(void *data, size_t size, struct player_params *params);
int pcm_data_get_interleaved(sndbuf_t buf[][SNDBUF_CHANS], int nframes,
	struct player_params *params);

#define PCM_FLAG_RAW 1
#define PCM_FLAG_POST 2
#define PCM_FLAG_SLTS 4

enum MixChan { MC_NONE, MC_MASTER, MC_VOICE, MC_MIDI, MC_CD, MC_LINE,
	MC_MIC, MC_PCSP };
enum MixSubChan { MSC_L, MSC_R, MSC_LR, MSC_RL, MSC_MONO_L, MSC_MONO_R };
enum MixRet { MR_UNSUP, MR_DISABLED, MR_OK };

#endif
