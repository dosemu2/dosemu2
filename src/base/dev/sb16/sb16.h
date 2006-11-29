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

#include "utilities.h"		// for rng_s

#define SB_NONE  0x000
#define SB_OLD	 0x105
#define SB_20	 0x201
#define SB_PRO	 0x300
#define SB_16	 0x405
#define SB_AWE32 0x40C

/*
 * Various Status values
 */

#define SB_DATA_AVAIL    0x80
#define SB_DATA_UNAVAIL  0x00

#define SB_WRITE_AVAIL   0x00
#define SB_WRITE_UNAVAIL 0x80

#define SB_IRQ_8BIT           1
#define SB_IRQ_16BIT          2
#define SB_IRQ_MIDI           SB_IRQ_8BIT
#define SB_IRQ_MPU401         4
#define SB_IRQ_DSP (SB_IRQ_8BIT | SB_IRQ_16BIT | SB_IRQ_MIDI)
#define SB_IRQ_ALL (SB_IRQ_DSP | SB_IRQ_MPU401)

/* 
 * DSP information / states 
 */
struct sb_struct {
  uint16_t rate;		/* The current sample rate for input */
  uint8_t  test;		/* Storage for the test value */
  uint8_t  reset_val;
  uint8_t  paused;		/* is DMA transfer paused? */ 
  uint8_t  mpu401_uart;
  uint8_t  midi_cmd;
  uint8_t  dma_cmd;		/* Information we need on the DMA transfer */
  uint8_t  dma_mode;		/* Information we need on the DMA transfer */
  uint8_t  dma_exit_ai;		/* exit DMA autoinit */
  uint8_t  new_dma_cmd;		/* Information we need on the DMA transfer */
  uint8_t  new_dma_mode;	/* Information we need on the DMA transfer */
  uint16_t dma_init_count;
  uint16_t new_dma_init_count;
  uint16_t dma_count;
  uint8_t  mixer_regs[256];
  uint8_t  mixer_index;
  uint8_t  E2Count;
  uint8_t  busy_hack;
/* All values are imperical! */
#define SB_DSP_CMD_BUF_SZ 8
  uint8_t  command[SB_DSP_CMD_BUF_SZ];
  int      command_idx;
#define DSP_QUEUE_SIZE 64
  struct rng_s dsp_queue;
#define DSP_FIFO_SIZE 64
  struct rng_s fifo_in;
  struct rng_s fifo_out;
#define DSP_OUT_FIFO_TRIGGER 32
#define DSP_IN_FIFO_TRIGGER 32
#define MIDI_FIFO_SIZE 32
  struct rng_s midi_fifo_in;
  struct rng_s midi_fifo_out;
  void *dspio;
};

extern int sb_get_dma_num(void);
extern int sb_get_hdma_num(void);
extern int sb_dma_active(void);
extern int sb_dma_16bit(void);
extern int sb_dma_samp_signed(void);
extern int sb_dma_samp_stereo(void);
extern int sb_dma_input(void);
extern int sb_dma_silence(void);
extern int sb_get_dma_sampling_rate(void);
extern int sb_get_dma_data(void *ptr, int is16bit);
extern void sb_put_dma_data(void *ptr, int is16bit);
extern void sb_handle_dma(void);
extern int sb_get_output_sample(void *ptr, int is16bit);
extern void sb_put_input_sample(void *ptr, int is16bit);
extern int sb_output_fifo_filled(void);
extern int sb_input_fifo_filled(void);
extern int sb_input_fifo_empty(void);
extern int sb_output_fifo_empty(void);
extern void sb_put_midi_data(unsigned char val);
extern int sb_midi_output_empty(void);
extern void sb_get_midi_data(Bit8u *val);

#endif
