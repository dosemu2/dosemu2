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
#ifndef __SB16_H__
#define __SB16_H__

#include "ringbuf.h"		// for rng_s
#include "sound/sound.h"

#define SB_NONE  0x000
#define SB_ID	 0x105
#define SB20_ID	 0x201
#define SBPRO_ID 0x300
#define SB16_ID 0x405

/* bochs and the old dosemu code disagree on that value.
 * Of course I trust bochs. :) */
#define SB16_ID82 (2 << 5)

#define SB_IRQ_8BIT           1
#define SB_IRQ_16BIT          2
#define SB_IRQ_MIDI           SB_IRQ_8BIT
#define SB_IRQ_MPU401         4
#define SB_IRQ_DSP (SB_IRQ_8BIT | SB_IRQ_16BIT | SB_IRQ_MIDI)
#define SB_IRQ_ALL (SB_IRQ_DSP | SB_IRQ_MPU401)

extern int sb_get_dma_num(void);
extern int sb_get_hdma_num(void);
extern int sb_dma_active(void);
extern int sb_dma_16bit(void);
extern int sb_dma_adpcm(void);
extern int sb_dma_adpcm_ref(void);
extern int sb_fifo_enabled(void);
extern int sb_dma_samp_signed(void);
extern int sb_dma_samp_stereo(void);
extern int sb_dma_input(void);
extern int sb_dma_silence(void);
extern int sb_get_dma_sampling_rate(void);
extern int sb_get_dma_data(void *ptr, int is16bit);
extern void sb_handle_dma(void);
extern void sb_dma_nack(void);
extern void sb_handle_dma_timeout(void);
extern int sb_input_enabled(void);
extern void sb_handle_midi_data(void);

enum MixRet sb_mixer_get_input_volume(enum MixChan ch, enum MixSubChan sc,
	double *r_vol);
enum MixRet sb_mixer_get_output_volume(enum MixChan ch, enum MixSubChan sc,
	double *r_vol);
int sb_mixer_get_chan_num(enum MixChan ch);
int sb_is_input_connected(enum MixChan ch);
int sb_is_output_connected(enum MixChan ch);

#define SB_CHAN_L 0
#define SB_CHAN_R 1

#endif
