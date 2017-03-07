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

/*
 * Purpose: SB16 emulation.
 *
 * Author: Stas Sergeev.
 *
 * Some code is taken from an old sound.c by Joel N. Weber II,
 * Alistair MacDonald, Michael Karcher and others - thanks.
 * Thanks to Vlad Romascanu and VDMSound project for the E2 code and some info.
 */

#include "emu.h"
#include "utilities.h"
#include "bitops.h"
#include "port.h"
#include "sound/sound.h"
#include "sound.h"
#include "dspio.h"
#include "adlib.h"
#include "sb16.h"
#include <string.h>

static int sb_irq_tab[] = { 2, 5, 7, 10 };
static int sb_dma_tab[] = { 0, 1, 3 };
static int sb_hdma_tab[] = { 5, 6, 7 };

#define SB_HAS_DATA (rng_count(&sb.dsp_queue) ? SB_DATA_AVAIL : SB_DATA_UNAVAIL)

static struct sb_struct sb;


static int sb_get_dsp_irq_num(void)
{
    int idx = find_bit(sb.mixer_regs[0x80]);
    if (idx == -1 || idx > 3) {
	error("SB IRQ wrong (%#x)\n", sb.mixer_regs[0x80]);
	return -1;
    }
    return sb_irq_tab[idx];
}

int get_mpu401_irq_num(void)
{
    if (!dspio_is_mt32_mode())
	return config.mpu401_irq;
    return (sb.mpu401_uart ? config.mpu401_uart_irq_mt32 :
	    config.mpu401_irq_mt32);
}

int sb_get_dma_num(void)
{
    int idx = find_bit(sb.mixer_regs[0x81] & 0x0f);
    if (idx == -1 || idx == 2) {
	error("SB DMA wrong (%#x)\n", sb.mixer_regs[0x81]);
	return -1;
    }
    return idx;
}

int sb_get_hdma_num(void)
{
    int idx = find_bit(sb.mixer_regs[0x81] & 0xf0);
    if (idx == 4) {
	error("SB HDMA wrong (%#x)\n", sb.mixer_regs[0x81]);
	return -1;
    }
    return idx;
}

/* DMA mode decoding functions */
int sb_dma_active(void)
{
    return (sb.dma_cmd && !sb.paused);
}

int sb_dma_16bit(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (16bit)\n");
    switch (sb.dma_cmd) {
    case 0xb0 ... 0xbf:
	return 1;
    }
    return 0;
}

static int sb_dma_sb16mode(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (sb16)\n");
    switch (sb.dma_cmd) {
    case 0xb0 ... 0xcf:
	return 1;
    }
    return 0;
}

int sb_fifo_enabled(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (fifo)\n");
    if (sb_dma_sb16mode())
	return (sb.dma_cmd & 2) ? 1 : 0;
    /* is this correct? */
    return 1;
}

int sb_dma_samp_signed(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (signed)\n");
    if (sb_dma_sb16mode())
	return (sb.dma_mode & 16) ? 1 : 0;
    return 0;
}

int sb_dma_samp_stereo(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (stereo)\n");
    if (sb_dma_sb16mode())
	return (sb.dma_mode & 32) ? 1 : 0;
    return (sb.mixer_regs[0x0e] & 2) ? 1 : 0;
}

int sb_dma_input(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (input)\n");
    if (sb_dma_sb16mode())
	return (sb.dma_cmd & 8) ? 1 : 0;
    switch (sb.dma_cmd) {
    case 0x24:
    case 0x2c:
    case 0x98:
    case 0x99:
    case 0xe2:			// weird case!
	return 1;
    }
    return 0;
}

int sb_dma_silence(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (silence)\n");
    switch (sb.dma_cmd) {
    case 0x80:
	return 1;
    }
    return 0;
}

static int sb_dma_internal(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (internal)\n");
    switch (sb.dma_cmd) {
    case 0xe2:
	return 1;
    }
    return 0;
}

int sb_input_enabled(void)
{
    return !sb_dma_internal();
}

static int sb_dma_autoinit(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (ai)\n");
#if 0
    /* if speaker state is not consistent with direction, dont autoinit - correct? */
    if (!(sb_dma_input() ^ sb.speaker))
	return 0;
#endif
    if (sb.dma_exit_ai)
	return 0;
    if (sb_dma_sb16mode())
	return (sb.dma_cmd & 4) ? 1 : 0;
    switch (sb.dma_cmd) {
    case 0x1c:
    case 0x1f:
    case 0x2c:
    case 0x7d:
    case 0x7f:
    case 0x90:
    case 0x98:
	return 1;
    }
    return 0;
}

int sb_get_dma_sampling_rate(void)
{
    int sample_rate;
    if (sb_dma_internal())	// that's because we don't zero sb.rate at reset...
	return 0;	// 0 is checked in dspio.c... hacks hacks :((
    sample_rate = sb.rate;
    if (!sb_dma_sb16mode())
	sample_rate >>= sb_dma_samp_stereo();
    return sample_rate;
}

static int sb_dma_high_speed(void)
{
    if (!sb.dma_cmd)
	error("SB: used inactive DMA (hs)\n");
    switch (sb.dma_cmd) {
    case 0x90:
    case 0x91:
    case 0x98:
    case 0x99:
	return 1;
    }
    return 0;
}

/* some sb-midi decoding */
static int sb_midi_input(void)
{
    switch (sb.midi_cmd) {
    case 0x30 ... 0x37:
	return 1;
    }
    return 0;
}

static int sb_midi_uart(void)
{
    switch (sb.midi_cmd) {
    case 0x34 ... 0x37:
	return 1;
    }
    return 0;
}

static int sb_midi_timestamp(void)
{
    switch (sb.midi_cmd) {
    case 0x32:
    case 0x33:
    case 0x36:
    case 0x37:
	return 1;
    }
    return 0;
}

static int sb_midi_int(void)
{
    switch (sb.midi_cmd) {
    case 0x31:
    case 0x33:
    case 0x35:
    case 0x37:
	return 1;
    }
    return 0;
}

static void start_dma_clock(void)
{
    /* this was likely needed for KryptEgg game, see commit 5741ac14 */
    sb.busy = 2;

    dspio_start_dma(sb.dspio);
}

static void stop_dma_clock(void)
{
    dspio_stop_dma(sb.dspio);

    sb.busy = 1;
}

static void stop_dma(void)
{
    /* first reset dma_cmd, as dspio may query it from dspio_stop_dma() */
    sb.dma_cmd = 0;
    stop_dma_clock();
}

static void sb_dma_actualize(void)
{
    if (sb.new_dma_cmd) {
	S_printf("SB: Actualized command %#x\n", sb.new_dma_cmd);
	sb.dma_cmd = sb.new_dma_cmd;
	sb.dma_mode = sb.new_dma_mode;
	/* count is reloaded in sb_dma_start() */
	sb.new_dma_cmd = 0;
	sb.new_dma_mode = 0;
	sb.dma_exit_ai = 0;
    }
}

static void sb_dma_start(void)
{
    sb.dma_restart.val = DMA_RESTART_NONE;
    sb_dma_actualize();
    if (sb_dma_active()) {
	sb.dma_count = sb.dma_init_count;
	S_printf("SB: DMA transfer started, count=%i\n",
		 sb.dma_init_count);
	S_printf("SB: sample params: rate=%i, stereo=%i, signed=%i 16bit=%i\n",
		 sb_get_dma_sampling_rate(), sb_dma_samp_stereo(),
		 sb_dma_samp_signed(), sb_dma_16bit());
	start_dma_clock();
    }
}

static int sb_irq_active(int type)
{
    return (sb.mixer_regs[0x82] & type);
}

static void sb_request_irq(int type)
{
    if (type & SB_IRQ_DSP)
	pic_request(pic_irq_list[sb_get_dsp_irq_num()]);
    if (type & SB_IRQ_MPU401)
	pic_request(pic_irq_list[get_mpu401_irq_num()]);
}

static void sb_activate_irq(int type)
{
    S_printf("SB: Activating irq type %d\n", type);
    if (sb_irq_active(type)) {
	S_printf("SB: Warning: Interrupt already active!\n");
	return;
    }
    sb_request_irq(type);
    sb.mixer_regs[0x82] |= type;
}

static void sb_deactivate_irq(int type)
{
    uint32_t act_map;
    int mpu_irq = get_mpu401_irq_num();

    S_printf("SB: Deactivating irq type %d\n", type);
    if (!(sb.mixer_regs[0x82] & type)) {
	S_printf("SB: Warning: Interrupt not active!\n");
	return;
    }
    sb.mixer_regs[0x82] &= ~type;
    /* if dsp and mpu irqs are the same, untrigger only when
     * both are inactive */
    act_map = ((!!(sb.mixer_regs[0x82] & SB_IRQ_DSP)) <<
	    sb_get_dsp_irq_num()) |
	    ((!!(sb.mixer_regs[0x82] & SB_IRQ_MPU401)) << mpu_irq);
    if (type & SB_IRQ_DSP) {
	if (!(act_map & (1 << sb_get_dsp_irq_num())))
	    pic_untrigger(pic_irq_list[sb_get_dsp_irq_num()]);
    }
    if (type & SB_IRQ_MPU401) {
	if (!(act_map & (1 << mpu_irq)))
	    pic_untrigger(pic_irq_list[mpu_irq]);
    }
}

static void sb_run_irq(int type)
{
    if (!sb_irq_active(type))
	return;
    S_printf("SB: Run irq type %d\n", type);
    sb_request_irq(type);
}

static void sb_dma_activate(void)
{
    if (sb.dma_restart.val == DMA_RESTART_CHECK) {
	if (sb_irq_active(SB_IRQ_DSP))
	    sb_deactivate_irq(SB_IRQ_DSP);
	sb.dma_restart.val = DMA_RESTART_NONE;
    }
    sb.new_dma_cmd = sb.command[0];
    sb.new_dma_mode = sb.command[1];
    /* a weird logic to fix Speedy game: if DMA have never advanced
     * (because channel is masked), forget current autoinit and
     * actualize the new settings. To not introduce the race condition
     * for the programs that rely on pending settings, we prolong
     * the busy status till missing DMA DACK. */
    if (!sb_dma_active() || sb.dma_restart.allow) {
	sb.paused = 0;	// !sb_dma_active() may mean paused
	S_printf("SB: starting DMA transfer\n");
	sb_dma_start();
    } else {
	S_printf("SB: DMA command %#x pending, current=%#x\n",
		 sb.new_dma_cmd, sb.dma_cmd);
    }
}

void sb_handle_dma(void)
{
    sb.dma_count--;
    sb.dma_restart.allow = 0;
    if (sb.dma_count == 0xffff) {
	sb.dma_count = sb.dma_init_count;
	sb.dma_restart.allow = 1;
	/* testsb16 will lock up if IRQ is raised on E2 */
	if (!sb_dma_internal()) {
	    S_printf("SB: Done block, triggering IRQ\n");
	    sb_activate_irq(sb_dma_16bit()? SB_IRQ_16BIT : SB_IRQ_8BIT);
	}
	if (!sb_dma_autoinit()) {
	    S_printf("SB: DMA transfer completed\n");
	    if (!sb_dma_internal()) {
		sb.dma_restart.val = DMA_RESTART_CHECK;
		sb.dma_restart.is_16 = sb_dma_16bit();
	    }
	    stop_dma();
	} else if (sb_fifo_enabled()) {
	    /* remember autoinit state here coz it may change
	     * before the ACK code to check it */
	    sb.dma_restart.val = DMA_RESTART_AUTOINIT;
	    sb.dma_restart.is_16 = sb_dma_16bit();
	    /* auto-init & FIFO - stop till IRQ-ACK */
	    stop_dma_clock();
	} else {
	    /* auto-init & !FIFO - apply new parameters and continue */
	    S_printf("SB: FIFO not enabled, continuing transfer\n");
	    sb_dma_actualize();
	}
    }
    sb.busy = 1;
}

void sb_dma_nack(void)
{
    /* speedy reprograms DSP without exiting auto-init
     * but when the DMA channel is masked. We need to downgrade
     * the busy status from 2 to 1. */
    if (sb.busy == 2)
	sb.busy = 1;
}

void sb_handle_dma_timeout(void)
{
    stop_dma();
}

static void dsp_write_output(uint8_t value)
{
    rng_put(&sb.dsp_queue, &value);
    if (debug_level('S') >= 2) {
	S_printf("SB: Insert into output Queue [%u]... (0x%x)\n",
		 rng_count(&sb.dsp_queue), value);
    }
}

static void sb_dsp_reset(void)
{
    S_printf("SB: Resetting SB DSP\n");

    stop_dma();
    dspio_toggle_speaker(sb.dspio, 0);
    dspio_clear_fifos(sb.dspio);
    rng_clear(&sb.dsp_queue);
    sb.paused = 0;
    sb.midi_cmd = 0;
    sb.dma_mode = 0;
    sb.new_dma_cmd = 0;
    sb.new_dma_mode = 0;
    sb.dma_exit_ai = 0;
    sb.dma_restart.val = DMA_RESTART_NONE;
    sb.dma_restart.is_16 = 0;
    sb.dma_restart.allow = 0;
    sb.dma_init_count = 0;
    sb.dma_count = 0;
    sb.command_idx = 0;
    sb.E2Count = 0;
    sb.last_data = 0;
    sb.reset_val = 0xaa;
    sb.rate = 32000;	// NFS in sbpro mode needs this rate
/* the following must not be zeroed out */
#if 0
    sb.mixer_index = 0;
#endif
}

static void sb_dsp_soft_reset(unsigned char value)
{
    if (value & 1) {
	if (!sb.reset) {
	    sb_deactivate_irq(SB_IRQ_ALL);
	    sb.reset = 1;

	    if (sb_dma_active() && sb_dma_high_speed()) {
		/* for High-Speed mode reset means only exit High-Speed */
		S_printf("SB: Reset called, exiting High-Speed DMA mode\n");
		stop_dma();
	    } else if (sb_midi_uart()) {
		S_printf("SB: Reset called, exiting UART midi mode\n");
		sb.midi_cmd = 0;
	    } else {
		sb_dsp_reset();
		sb.busy = 2;
	    }
	}
    } else {
	if (sb.reset) {
	    sb.reset = 0;
	    dsp_write_output(sb.reset_val);
	    sb.busy = 1;
	}
    }
}

static void sb_mixer_reset(void)
{
    memset(sb.mixer_regs, 0, 0x48);
    /* Restore values as per Creative specs */
    sb.mixer_regs[0x0a] = 0;	/* -48 dB */
    sb.mixer_regs[0x04] =
    sb.mixer_regs[0x22] =
    sb.mixer_regs[0x26] = 0xcc;	/* -12 dB */
    sb.mixer_regs[0x28] =
    sb.mixer_regs[0x2e] = 0;	/* -60 dB */

    sb.mixer_regs[0x30] =
    sb.mixer_regs[0x31] =
    sb.mixer_regs[0x32] =
    sb.mixer_regs[0x33] =
    sb.mixer_regs[0x34] =
    sb.mixer_regs[0x35] = 24 << 3;	/* -14 dB */

    sb.mixer_regs[0x36] =
    sb.mixer_regs[0x37] =
    sb.mixer_regs[0x38] =
    sb.mixer_regs[0x39] =
    sb.mixer_regs[0x3a] = 0;	/* -62 dB */

    sb.mixer_regs[0x3b] = 0;	/* -18 dB */

    sb.mixer_regs[0x3c] = 0x1f;
    sb.mixer_regs[0x3d] = 0x15;
    sb.mixer_regs[0x3e] = 0x0b;

    sb.mixer_regs[0x3f] =
    sb.mixer_regs[0x40] =
    sb.mixer_regs[0x41] =
    sb.mixer_regs[0x42] = 0;	/* 0 dB */

    sb.mixer_regs[0x44] =
    sb.mixer_regs[0x45] =
    sb.mixer_regs[0x46] =
    sb.mixer_regs[0x47] = 8 << 4;	/* 0 dB */
}

static int num_to_idx(int num, int arr[], int len)
{
    int i;
    for (i = 0; i < len; i++) {
	if (arr[i] == num)
	    return 1 << i;
    }
    return 0;
}

static void sb_mixer_init(void)
{
    int irq_idx, dma_idx, hdma_idx;

    S_printf("SB: Mixer init\n");

    memset(sb.mixer_regs, 0, sizeof(sb.mixer_regs));

    irq_idx = num_to_idx(config.sb_irq, sb_irq_tab, ARRAY_SIZE(sb_irq_tab));
    if (!irq_idx) {
	error("Sound Blaster cannot work on IRQ %i\n", config.sb_irq);
	config.exitearly = 1;
    }
    if (!num_to_idx(config.sb_dma, sb_dma_tab, ARRAY_SIZE(sb_dma_tab))) {
	error("Sound Blaster cannot work on DMA %i\n", config.sb_dma);
	config.exitearly = 1;
    }
    dma_idx = 1 << config.sb_dma;
    if (config.sb_hdma
	&& !num_to_idx(config.sb_hdma, sb_hdma_tab,
		       ARRAY_SIZE(sb_hdma_tab))) {
	error("Sound Blaster cannot work on HDMA %i\n", config.sb_hdma);
	config.exitearly = 1;
    }
    hdma_idx = config.sb_hdma ? 1 << config.sb_hdma : 0;
    sb.mixer_regs[0x80] = irq_idx;
    sb.mixer_regs[0x81] = dma_idx | hdma_idx;
    sb.mixer_regs[0x82] = SB16_ID82;
}

static void sb_reset(void)
{
    sb_dsp_reset();
    sb.busy = 1;
    sb_mixer_reset();
    adlib_reset();
}

/*
 * DANG_BEGIN_FUNCTION sb_dsp_write
 *
 * arguments:
 * value - The value being written to the DSP.
 *
 * DANG_END_FUNCTION
 */
static void sb_dsp_write(Bit8u value)
{
#define REQ_PARAMS(i) if (sb.command_idx <= i) return
#define PAR_LSB_MSB(i) (sb.command[i] | (sb.command[i+1] << 8))
#define PAR_MSB_LSB(i) ((sb.command[i] << 8) | sb.command[i+1])

    if (sb.command_idx < SB_DSP_CMD_BUF_SZ) {
	S_printf("SB: Accepted byte %#x, index=%i\n", value,
		 sb.command_idx);
	sb.command[sb.command_idx++] = value;
    } else {
	S_printf("SB: ERROR: command buffer overflowed!\n");
    }

    switch (sb.command[0]) {
	/* 0x04: ??? - SB16ASP */
    case 0x04:
	REQ_PARAMS(1);
	S_printf("SB: Unsupported DSP command 4\n");
	if ((sb.command[1] & 0xf1) == 0xf1)
	    sb.asp_init = 1;
	else
	    sb.asp_init = 0;
	break;

	/* 0x05: ??? - SB16ASP */
    case 0x05:
	REQ_PARAMS(2);
	S_printf("SB: Unsupported DSP command 5\n");
	break;

	/* 0x08: ??? - SB16ASP */
    case 0x08:
	REQ_PARAMS(1);
	S_printf("SB: Unsupported DSP command 8\n");
	if (sb.command[1] == 3)
	    dsp_write_output(0x18);
	break;

	/* 0x0e: ??? - SB16ASP */
    case 0x0e:
	REQ_PARAMS(2);
	S_printf("SB: Unsupported DSP command e\n");
	sb.asp_regs[sb.command[1]] = sb.command[2];
	break;

	/* 0x0f: ??? - SB16ASP */
    case 0x0f:
	REQ_PARAMS(1);
	S_printf("SB: Unsupported DSP command f\n");
	if (sb.asp_init)
	    sb.asp_regs[sb.command[1]] = ~sb.asp_regs[sb.command[1]];
	dsp_write_output(sb.asp_regs[sb.command[1]]);
	break;

    case 0x10:
	/* Direct 8-bit DAC - SB */
	REQ_PARAMS(1);
	S_printf("SB: Direct 8-bit DAC write (%u)\n", sb.command[1]);
	dspio_write_dac(sb.dspio, sb.command[1]);
	break;

	/* == OUTPUT COMMANDS == */
	/* DMA 8-bit DAC - SB */
    case 0x14:
	/* DMA 2-bit ADPCM DAC - SB */
    case 0x16:
	/* DMA 2-bit ADPCM DAC (Reference) - SB */
    case 0x17:
	/* DMA 8-bit ADC - SB */
    case 0x24:
	/* DMA 4-bit ADPCM DAC - SB */
    case 0x74:
	/* DMA 4-bit ADPCM DAC (Reference) - SB */
    case 0x75:
	/* DMA 2.6-bit ADPCM DAC - SB */
    case 0x76:
	/* DMA 2.6-bit ADPCM DAC (Reference) - SB */
    case 0x77:
	REQ_PARAMS(2);
	sb.dma_init_count = PAR_LSB_MSB(1);
	sb_dma_activate();
	break;

	/* DMA 8-bit DAC (Auto-Init) - SB2.0 */
    case 0x1C:
	/* DMA 8-bit ADC (Auto-Init) - SB2.0 */
    case 0x2C:
	/* DMA 8-bit DAC (High Speed, Auto-Init) - SB2.0-Pro2 */
    case 0x90:
	/* **CRISK** DMA-8 bit DAC (High Speed) */
    case 0x91:
	/* DMA 2-bit ADPCM DAC (Reference, Auto-Init) - SB2.0 */
    case 0x1F:
	/* DMA 4-bit ADPCM DAC (Auto-Init, Reference) - SB2.0 */
    case 0x7D:
	/* DMA 2.6-bit ADPCM DAC (Auto-Init, Reference) - SB2.0 */
    case 0x7F:
	/* DMA 8-bit ADC (High Speed, Auto-Init) - SB2.0-Pro2 */
    case 0x98:
	/* DMA 8-bit ADC (High Speed) - SB2.0-Pro2 */
    case 0x99:
	if (!sb.dma_init_count) {
	    REQ_PARAMS(2);
	    sb.dma_init_count = PAR_LSB_MSB(1);
	    S_printf("SB: DMA count is now set to %d\n",
		     sb.dma_init_count);
	} else {
	    S_printf("SB: Re-using DMA count, set to %d\n",
		     sb.dma_init_count);
	}
	sb_dma_activate();
	break;

	/* == INPUT COMMANDS == */
    case 0x20:
	/* Direct 8-bit ADC - SB */
	S_printf("SB: 8-bit ADC (Unimplemented)\n");
	dsp_write_output(0);
	/* I'll implement that, be patient */
	break;

    case 0x28:
	/* Direct 8-bit ADC (Burst) - SBPro2 */
	dsp_write_output(0);
	break;

	/* == MIDI == */
    case 0x30:			/* Enter polling midi input mode */
    case 0x31:			/* Enter interruptible midi input mode */
    case 0x32:			/* Midi Read Timestamp Poll - SB ? */
    case 0x33:			/* Midi Read Timestamp Interrupt - SB ? */
    case 0x34:			/* Enter midi polling UART mode */
    case 0x35:			/* Enter midi interruptible UART mode */
    case 0x36:			/* Enter midi polling UART mode, with timestamping */
    case 0x37:			/* Enter midi interruptible UART mode, with timestamping */
	if (sb_midi_input() && sb_midi_int() && !sb_midi_uart()
	    && sb.command[1] == sb.midi_cmd) {
	    S_printf("SB: Exiting SB-midi mode %#x\n", sb.midi_cmd);
	    sb.midi_cmd = 0;
	    return;
	}
	S_printf("SB: SB Midi command %#x accepted\n", sb.command[1]);
	sb.midi_cmd = sb.command[1];
	break;

    case 0x38:			/* Midi Write */
	REQ_PARAMS(1);
	S_printf("SB: Write 0x%x to SB Midi Port\n", sb.command[1]);
	dspio_write_midi(sb.dspio, sb.command[1]);
	break;

	/* == SAMPLE SPEED == */
    case 0x40:
	/* Set Time Constant */
	REQ_PARAMS(1);
	sb.rate = 1000000 / (256 - sb.command[1]);
	S_printf("SB: Time constant set to %u\n", sb.command[1]);
	break;

    case 0x41:
	/* Set Output Sample Rate - SB16 */
	REQ_PARAMS(2);
	/* MSB, LSB values - Karcher */
	sb.rate = PAR_MSB_LSB(1);
	S_printf("SB: Output sampling rate set to %u Hz\n", sb.rate);
	break;

    case 0x42:
	/* Set Input Sample Rate - SB16 */
	REQ_PARAMS(2);
	/* MSB, LSB values - Karcher */
	sb.rate = PAR_MSB_LSB(1);
	S_printf("SB: Input sampling rate set to %u Hz\n", sb.rate);
	break;

	/* == OUTPUT == */
#if 0
	/* not sb16 */
	/* Continue Auto-Init 8-bit DMA - SB16 */
    case 0x45:
	/* Continue Auto-Init 16-bit DMA - SB16 */
    case 0x47:
	if (sb.paused) {
	    S_printf("SB: Unpausing DMA, left=%i\n", sb.dma_count);
	    sb.paused = 0;
	    if (sb.dma_cmd)
		start_dma_clock();
	}
	break;
#endif
    case 0x48:
	/* Set DMA Block Size - SB2.0 */
	REQ_PARAMS(2);
	sb.dma_init_count = PAR_LSB_MSB(1);
	S_printf("SB: DMA count is set to %d\n", sb.dma_init_count);
	break;

    case 0x80:
	/* Silence DAC - SB */
	REQ_PARAMS(2);
	sb.dma_init_count = PAR_LSB_MSB(1);
	S_printf("SB: Silence count is set to %d\n",
		 sb.dma_init_count);
	sb_dma_activate();
	break;

#if 0
	/* Those are not available in SB16 */
	/* == STEREO MODE == */
    case 0xA0:
	/* Enable Mono Input - SBPro Only */
	S_printf("SB: Disable Stereo Input\n");
	break;

    case 0xA8:
	/* Enable Stereo Input - SBPro Only */
	S_printf("SB: Enable Stereo Input\n");
	break;
#endif

	/* == SB16 DMA == */
    case 0xB0 ... 0xCF:
	if (sb.command[0] & 1) {
	    S_printf("SB: Bad command\n");
	    break;
	}
	/* SB16 16-bit DMA */
	REQ_PARAMS(3);
	sb.dma_init_count = PAR_LSB_MSB(2);
	if (sb.command[0] & 8) {
	    S_printf("SB: Starting SB16 16-bit DMA input.\n");
	    dspio_toggle_speaker(sb.dspio, 0);
	} else {
	    S_printf("SB: Starting SB16 16-bit DMA output.\n");
	    dspio_toggle_speaker(sb.dspio, 1);
	}
	sb_dma_activate();
	break;

	/* == DMA == */
	/* Halt 8-bit DMA - SB */
    case 0xD0:
	if (!sb.paused) {
	    S_printf("SB: Pausing 8bit DMA, left=%i\n", sb.dma_count);
	    sb_deactivate_irq(SB_IRQ_8BIT);
	    sb.paused = 1;
	    if (sb.dma_cmd)
		stop_dma_clock();
	}
	break;
	/* Halt 16-bit DMA - SB16 */
    case 0xD5:
	if (!sb.paused) {
	    S_printf("SB: Pausing 16bit DMA, left=%i\n", sb.dma_count);
	    sb_deactivate_irq(SB_IRQ_16BIT);
	    sb.paused = 1;
	    if (sb.dma_cmd)
		stop_dma_clock();
	}
	break;

	/* == SPEAKER == */
    case 0xD1:
	/* Enable Speaker - SB */
	S_printf("SB: Enabling speaker\n");
	dspio_toggle_speaker(sb.dspio, 1);
	break;

    case 0xD3:
	/* Disable Speaker - SB */
	S_printf("SB: Disabling speaker\n");
	dspio_toggle_speaker(sb.dspio, 0);
	break;

	/* == DMA == */
	/* Continue 8-bit DMA - SB */
    case 0xD4:
	/* Continue 16-bit DMA - SB16 */
    case 0xD6:
	if (sb.paused) {
	    S_printf("SB: Unpausing 16bit DMA, left=%i\n", sb.dma_count);
	    sb.paused = 0;
	    if (sb.dma_cmd)
		start_dma_clock();
	}
	break;

	/* == SPEAKER == */
    case 0xD8:
	/* Speaker Status */
	dsp_write_output(dspio_get_speaker_state(sb.dspio));
	break;

	/* == DMA == */
	/* Exit Auto-Init 16-bit DMA - SB16 */
    case 0xD9:
	/* Exit Auto-Init 8-bit DMA - SB2.0 */
    case 0xDA:
	S_printf("SB: Exiting DMA autoinit\n");
	if (sb.dma_cmd)
	    sb.dma_exit_ai = 1;
	break;

	/* == DSP IDENTIFICATION == */
    case 0xE0:
	/* DSP Identification - SB2.0 */
	REQ_PARAMS(1);
	S_printf("SB: Identify SB2.0 DSP.\n");
	dsp_write_output(~sb.command[1]);
	break;

    case 0xE1:
	/* DSP Version - SB */
	S_printf("SB: Query Version\n");
	dsp_write_output(SB16_ID >> 8);
	dsp_write_output(SB16_ID & 0xFF);
	break;

    case 0xE2: {
	    /*
	     * Identification via DMA - completely undocumented.
	     * Based on the code of VDMSound project,
	     * http://www.sourceforge.net/projects/vdmsound/
	     */
	    const Bit8u E2_negmsk = 3;
	    const Bit8u E2_incmagic = 0x96;
	    int i;
	    Bit8u negmsk, inctmp, incmagic, incval;

	    REQ_PARAMS(1);
	    S_printf("SB: E2 identification starting\n");

	    negmsk = E2_negmsk;
	    inctmp = 1;
	    incval = 0;
	    for (i = 0; i < 8; i++) {
		negmsk <<= (i & 1) + 1;
		negmsk |= negmsk >> 4;
		negmsk &= 0x0f;
		if (test_bit(i, &sb.command[1]))
		    incval +=
			test_bit(sb.E2Count, &negmsk) ? -inctmp : inctmp;
		inctmp <<= 1;
	    }
	    incmagic = E2_incmagic;
	    if (sb.E2Count & 1)
		incmagic += 0x0f;
	    if (sb.E2Count & 2)
		incmagic = (incmagic << 4) | (incmagic >> 4);
	    incval += incmagic;
	    sb.E2Count++;
	    sb.E2Count &= 3;

	    sb.reset_val += incval;
	    sb_dma_activate();

	    break;
	}

    case 0xE3: {
	    char cs[] = "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";
	    rng_add(&sb.dsp_queue, sizeof(cs), cs);
	}
	break;

	/* == TEST == */
    case 0xE4:
	/* Write to Test - SB2.0 */
	REQ_PARAMS(1);
	sb.test = sb.command[1];
	S_printf("SB: write 0x%x to test register.\n", sb.test);
	break;

    case 0xE8:
	/* Read from Test - SB2.0 */
	S_printf("SB: Read 0x%x from test register.\n", sb.test);
	dsp_write_output(sb.test);
	break;

    case 0xF0:
	/* Sine Generator - SB */
	break;

	/* == IRQ == */
	/* 8-bit IRQ - SB */
    case 0xF2:
	S_printf("SB: Activating 8bit IRQ\n");
	sb_activate_irq(SB_IRQ_8BIT);
	break;

	/* 16-bit IRQ - SB16 */
    case 0xF3:
	S_printf("SB: Activating 16bit IRQ\n");
	sb_activate_irq(SB_IRQ_16BIT);
	break;

    case 0xf9:			/* from bochs */
	REQ_PARAMS(1);
	S_printf("SB: F9 test\n");
	switch (sb.command[1]) {
	case 0x0e:
	    dsp_write_output(0xff);
	    break;
	case 0x0f:
	    dsp_write_output(0x07);
	    break;
	case 0x37:
	    dsp_write_output(0x38);
	    break;
	default:
	    dsp_write_output(0);
	}
	break;

    case 0xfa: // Undoc'd SBPro II, from SoundFX
	// SB16 puts 0xfb in output buffer but no ready status
	S_printf("SB: Undocumented 0xfa\n");
	sb.last_data = 0xfb;
	break;

	/* 0xFB: DSP Status - SB16 */
	/* 0xFC: DSP Auxiliary Status - SB16 */
    case 0xfd: /* DSP Command Status - SB16, from SoundFX */
	S_printf("SB: Undocumented 0xfd\n");
	dsp_write_output(sb.command[0]);
	break;

    default:
	S_printf("SB: ERROR: Unsupported command\n");
    }

    sb.command_idx = 0;
}

static int line_enabled(void)
{
    return ((sb.mixer_regs[0x3c] | sb.mixer_regs[0x3d] |
	    sb.mixer_regs[0x3e]) & 0x18);
}

static int mic_enabled(void)
{
    return ((sb.mixer_regs[0x3c] | sb.mixer_regs[0x3d] |
	    sb.mixer_regs[0x3e]) & 0x1);
}

static void sb_mixer_write(Bit8u value)
{
    Bit8u delta = sb.mixer_regs[sb.mixer_index] ^ value;
    S_printf("SB: write mixer reg %#x val=%#x\n", sb.mixer_index, value);
    sb.mixer_regs[sb.mixer_index] = value;
    switch (sb.mixer_index) {
    case 0:
	sb_mixer_reset();
	break;

    case 0x04:
	sb.mixer_regs[0x32] = (value & 0xf0) | 8;
	sb.mixer_regs[0x33] = (value << 4) | 8;
	break;

    case 0x0A:
	sb.mixer_regs[0x3A] = (value << 5) | 0x18;
	break;

    case 0x22:
	sb.mixer_regs[0x30] = (value & 0xf0) | 8;
	sb.mixer_regs[0x31] = (value << 4) | 8;
	break;

    case 0x26:
	sb.mixer_regs[0x34] = (value & 0xf0) | 8;
	sb.mixer_regs[0x35] = (value << 4) | 8;
	break;

    case 0x28:
	sb.mixer_regs[0x36] = (value & 0xf0) | 8;
	sb.mixer_regs[0x37] = (value << 4) | 8;
	break;

    case 0x2E:
	sb.mixer_regs[0x38] = (value & 0xf0) | 8;
	sb.mixer_regs[0x39] = (value << 4) | 8;
	break;

    case 0x38:
    case 0x39:
	if (line_enabled() && (sb.mixer_regs[0x38] || sb.mixer_regs[0x39]))
	    dspio_input_enable(sb.dspio, MC_LINE);
	else
	    dspio_input_disable(sb.dspio, MC_LINE);
	break;

    case 0x3a:
	if (mic_enabled() && sb.mixer_regs[0x3a])
	    dspio_input_enable(sb.dspio, MC_MIC);
	else
	    dspio_input_disable(sb.dspio, MC_MIC);
	break;

    case 0x3c:
    case 0x3d:
    case 0x3e:
	if (delta & 0x18) {
	    if (line_enabled() && (sb.mixer_regs[0x38] || sb.mixer_regs[0x39]))
		dspio_input_enable(sb.dspio, MC_LINE);
	    else
		dspio_input_disable(sb.dspio, MC_LINE);
	}

	if (delta & 0x1) {
	    if (mic_enabled() && sb.mixer_regs[0x3a])
		dspio_input_enable(sb.dspio, MC_MIC);
	    else
		dspio_input_disable(sb.dspio, MC_MIC);
	}
	break;
    }
}

static Bit8u sb_mixer_read(void)
{
    Bit8u val;
    S_printf("SB: Reading Mixer register %#x\n", sb.mixer_index);
    switch (sb.mixer_index) {
    case 0x04:
	val = (sb.mixer_regs[0x32] & 0xf0) | (sb.mixer_regs[0x33] >> 4);
	break;

    case 0x0A:
	val = (sb.mixer_regs[0x3A] >> 5);
	break;

    case 0x22:
	val = (sb.mixer_regs[0x30] & 0xf0) | (sb.mixer_regs[0x31] >> 4);
	break;

    case 0x26:
	val = (sb.mixer_regs[0x34] & 0xf0) | (sb.mixer_regs[0x35] >> 4);
	break;

    case 0x28:
	val = (sb.mixer_regs[0x36] & 0xf0) | (sb.mixer_regs[0x37] >> 4);
	break;

    case 0x2E:
	val = (sb.mixer_regs[0x38] & 0xf0) | (sb.mixer_regs[0x39] >> 4);
	break;

    default:
	val = sb.mixer_regs[sb.mixer_index];
	break;
    }
    return val;
}

static double vol5h(int reg)
{
    return dspio_calc_vol(sb.mixer_regs[reg] >> 3, 2, -62);
}

static double vol2h(int reg)
{
    return dspio_calc_vol(sb.mixer_regs[reg] >> 6, 6, -18);
}

static double vol3l(int reg)
{
    return dspio_calc_vol(sb.mixer_regs[reg] & 7, 6, -42);
}

static double gain2h(int reg)
{
    return dspio_calc_vol(sb.mixer_regs[reg] >> 6, 6, 0);
}

#define ENAB(r, b) \
    if (!(sb.mixer_regs[r] & (1 << (b)))) \
	return MR_DISABLED

enum MixRet sb_mixer_get_input_volume(enum MixChan ch, enum MixSubChan sc,
	double *r_vol)
{
    double vol = 1;
    switch (ch) {
    case MC_MIDI:
	switch (sc) {
	case MSC_L:
	    ENAB(0x3d, 6);
	    vol *= vol5h(0x34);
	    break;
	case MSC_R:
	    ENAB(0x3e, 5);
	    vol *= vol5h(0x35);
	    break;
	case MSC_LR:
	    ENAB(0x3e, 6);
	    vol *= vol5h(0x34);
	    break;
	case MSC_RL:
	    ENAB(0x3d, 5);
	    vol *= vol5h(0x35);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    case MC_CD:
	switch (sc) {
	case MSC_L:
	    ENAB(0x3d, 2);
	    vol *= vol5h(0x36);
	    break;
	case MSC_R:
	    ENAB(0x3e, 1);
	    vol *= vol5h(0x37);
	    break;
	case MSC_LR:
	    ENAB(0x3e, 2);
	    vol *= vol5h(0x36);
	    break;
	case MSC_RL:
	    ENAB(0x3d, 1);
	    vol *= vol5h(0x37);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    case MC_LINE:
	switch (sc) {
	case MSC_L:
	    ENAB(0x3d, 4);
	    vol *= vol5h(0x38);
	    break;
	case MSC_R:
	    ENAB(0x3e, 3);
	    vol *= vol5h(0x39);
	    break;
	case MSC_LR:
	    ENAB(0x3e, 4);
	    vol *= vol5h(0x38);
	    break;
	case MSC_RL:
	    ENAB(0x3d, 3);
	    vol *= vol5h(0x39);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    case MC_MIC:
	switch (sc) {
	case MSC_MONO_L:
	    ENAB(0x3d, 0);
	    vol *= vol3l(0x3a);
	    break;
	case MSC_MONO_R:
	    ENAB(0x3e, 0);
	    vol *= vol3l(0x3a);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    default:
	return MR_UNSUP;
    }

    switch (sc) {
    case MSC_L:
    case MSC_RL:
    case MSC_MONO_L:
	vol *= gain2h(0x3f);
	break;
    case MSC_R:
    case MSC_LR:
    case MSC_MONO_R:
	vol *= gain2h(0x40);
	break;
    }

    *r_vol = vol;
    return MR_OK;
}

enum MixRet sb_mixer_get_output_volume(enum MixChan ch, enum MixSubChan sc,
	double *r_vol)
{
    double vol = 1;
    switch (ch) {
    case MC_MIDI:
	switch (sc) {
	case MSC_L:
	    vol *= vol5h(0x34);
	    break;
	case MSC_R:
	    vol *= vol5h(0x35);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    case MC_CD:
	switch (sc) {
	case MSC_L:
	    ENAB(0x3c, 2);
	    vol *= vol5h(0x36);
	    break;
	case MSC_R:
	    ENAB(0x3c, 1);
	    vol *= vol5h(0x37);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    case MC_LINE:
	switch (sc) {
	case MSC_L:
	    ENAB(0x3c, 4);
	    vol *= vol5h(0x38);
	    break;
	case MSC_R:
	    ENAB(0x3c, 3);
	    vol *= vol5h(0x39);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    case MC_MIC:
	switch (sc) {
	case MSC_MONO_L:
	case MSC_MONO_R:
	    ENAB(0x3c, 0);
	    vol *= vol3l(0x3a);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    case MC_VOICE:
	switch (sc) {
	case MSC_L:
	    vol *= vol5h(0x32);
	    break;
	case MSC_R:
	    vol *= vol5h(0x33);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    case MC_PCSP:
	switch (sc) {
	case MSC_MONO_L:
	case MSC_MONO_R:
	    vol *= vol2h(0x3b);
	    break;
	default:
	    return MR_UNSUP;
	}
	break;
    default:
	return MR_UNSUP;
    }

    switch (sc) {
    case MSC_L:
    case MSC_RL:
    case MSC_MONO_L:
	vol *= vol5h(0x30);		// master
	vol *= gain2h(0x41);
	break;
    case MSC_R:
    case MSC_LR:
    case MSC_MONO_R:
	vol *= vol5h(0x31);		// master
	vol *= gain2h(0x42);
	break;
    }

    *r_vol = vol;
    return MR_OK;
}


#define EN(r, b) (sb.mixer_regs[r] & (1 << (b)))

int sb_is_input_connected(enum MixChan ch)
{
    switch (ch) {
    case MC_MIDI:
	return (EN(0x3d, 6) || EN(0x3e, 5) || EN(0x3e, 6) || EN(0x3d, 5));
    case MC_CD:
	return (EN(0x3d, 2) || EN(0x3e, 1) || EN(0x3e, 2) || EN(0x3d, 1));
    case MC_LINE:
	return (EN(0x3d, 4) || EN(0x3e, 3) || EN(0x3e, 4) || EN(0x3d, 3));
    case MC_MIC:
	return (EN(0x3d, 0) || EN(0x3e, 0));
    default:
	return 0;
    }
}

int sb_is_output_connected(enum MixChan ch)
{
    switch (ch) {
    case MC_CD:
	return (EN(0x3c, 2) || EN(0x3c, 1));
    case MC_LINE:
	return (EN(0x3c, 4) || EN(0x3c, 3));
    case MC_MIC:
	return EN(0x3c, 0);
    case MC_VOICE:
    case MC_PCSP:
    case MC_MIDI:
	return 1;
    default:
	return 0;
    }
}

int sb_mixer_get_chan_num(enum MixChan ch)
{
    switch (ch) {
    case MC_MIC:
    case MC_PCSP:
	return 1;
    default:
	return 2;
    }
}

/*
 * DANG_BEGIN_FUNCTION sb_io_write
 *
 * arguments:
 * port - The I/O port being written to.
 * value - The value being output.
 *
 * DANG_END_FUNCTION
 */

static void sb_io_write(ioport_t port, Bit8u value)
{
    ioport_t addr;
    addr = port - config.sb_base;

    if (debug_level('S') >= 2) {
	S_printf("SB: Write to port 0x%04x, value 0x%02x\n", (Bit16u) port,
		 value);
    }

    switch (addr) {
	/* == FM MUSIC == */
    case 0x00 ... 0x03:
	adlib_io_write_base(addr, value);
	break;

    case 0x08:
    case 0x09:
	adlib_io_write_base(addr - 8, value);
	break;

	/* == MIXER == */
    case 0x04:			/* Mixer Register Address Port */
	sb.mixer_index = value;
	break;

    case 0x05:			/* Mixer Data Register */
	sb_mixer_write(value);
	break;

    case 0x06:			/* DSP reset */
	sb_dsp_soft_reset(value);
	break;

#if 0
    case 0x0A:			/* Write to DSP queue probably - undocumented */
	S_printf("SB: undocumented write to A\n");
	rng_clear(&sb.dsp_queue);
	rng_put(&sb.dsp_queue, &value);
	break;
#endif

	/* == DSP == */
    case 0x0C:			/* dsp write register */
	if (sb_midi_uart()) {
	    dspio_write_midi(sb.dspio, value);
	    break;
	}
	if (sb_dma_active() && sb_dma_high_speed()) {
	    S_printf
		("SB: Commands are not permitted in High-Speed DMA mode!\n");
	    /* break; */
	}
	sb_dsp_write(value);
	break;

	/* 0x0D: Timer Interrupt Clear - SB16 */
	/* 0x10 - 0x13: CD-ROM - SBPro ***IGNORED*** */

    default:
	S_printf("SB: Write to port %#x unimplemented\n", addr);
    }
}

/*
 * DANG_BEGIN_FUNCTION sb_io_read
 *
 * arguments:
 * port - The I/O port being read from.
 *
 * DANG_END_FUNCTION
 */

static Bit8u sb_io_read(ioport_t port)
{
    ioport_t addr;
    Bit8u result = 0;

    addr = port - config.sb_base;

    switch (addr) {

	/* == FM Music == */
    case 0x00 ... 0x03:
	result = adlib_io_read_base(addr);
	break;
    case 0x08:
    case 0x09:
	result = adlib_io_read_base(addr - 8);
	break;

    case 0x04:			/* Mixer index */
	result = sb.mixer_index;
	break;

    case 0x05:			/* Mixer Data Register */
	result = sb_mixer_read();
	break;

    case 0x06:			/* Reset ? */
	S_printf("SB: read from Reset address\n");
	result = 0;		/* Some programs read this whilst resetting */
	break;

    case 0x0A:			/* DSP Read Data - SB */
	if (rng_count(&sb.dsp_queue))
	    rng_get(&sb.dsp_queue, &sb.last_data);
	result = sb.last_data;
	S_printf("SB: Read 0x%x from SB DSP\n", result);
	if (sb_midi_int()) {
	    if (!rng_count(&sb.dsp_queue))
		sb_deactivate_irq(SB_IRQ_MIDI);
	    sb_run_irq(SB_IRQ_MIDI);
	}
	break;

    case 0x0C:			/* DSP Write Buffer Status */
	result = 0x7f;
	if (sb.busy)
	    result |= 0x80;
	if (sb.busy == 1)
	    sb.busy = 0;
	S_printf("SB: Read 0x%x from DSP Write Buffer Status Register (%i)\n",
		 result, sb.busy);
	break;

    case 0x0D:			/* DSP MIDI Interrupt Clear - SB16 ? */
	S_printf
	    ("SB: read 16-bit MIDI interrupt status. Not Implemented.\n");
	result = 0xFF;
	break;

    case 0x0E:
	/* DSP Data Available Status - SB */
	/* DSP 8-bit IRQ Ack - SB */
	result = SB_HAS_DATA;
	S_printf("SB: 8-bit IRQ Ack (%i)\n", sb.dma_count);
	if (sb_irq_active(SB_IRQ_8BIT))
	    sb_deactivate_irq(SB_IRQ_8BIT);
	if (sb.paused && sb.dma_cmd) {
	    S_printf("SB: drop DMA command %x\n", sb.dma_cmd);
	    sb.dma_cmd = 0;
	    sb.dma_restart.val = DMA_RESTART_NONE;
	}
	if (sb.dma_restart.val && !sb.dma_restart.is_16)
	    sb_dma_start();
	break;
    case 0x0F:			/* 0x0F: DSP 16-bit IRQ - SB16 */
	result = SB_HAS_DATA;
	S_printf("SB: 16-bit IRQ Ack: (%i)\n", sb.dma_count);
	if (sb_irq_active(SB_IRQ_16BIT))
	    sb_deactivate_irq(SB_IRQ_16BIT);
	if (sb.paused && sb.dma_cmd) {
	    S_printf("SB: drop DMA command %x\n", sb.dma_cmd);
	    sb.dma_cmd = 0;
	    sb.dma_restart.val = DMA_RESTART_NONE;
	}
	if (sb.dma_restart.val && sb.dma_restart.is_16)
	    sb_dma_start();
	break;

	/* == CD-ROM - UNIMPLEMENTED == */
    case 0x10:			/* CD-ROM Data Register - SBPro */
	S_printf("SB: read from CD-ROM Data register.\n");
	result = 0;
	break;

    case 0x11:			/* CD-ROM Status Port - SBPro */
	S_printf("SB: read from CD-ROM status port.\n");
	result = 0xFE;
	break;

    default:
	S_printf("SB: %#x is an unhandled read port.\n", port);
	result = 0xFF;
    }

    if (debug_level('S') >= 3) {
	S_printf("SB: port read 0x%x returns 0x%x\n", port, result);
    }

    return result;
}

int sb_get_dma_data(void *ptr, int is16bit)
{
    if (sb_dma_internal()) {
	S_printf("SB: E2 value %#x transferred\n", sb.reset_val);
	if (is16bit)
	    *(Bit16u *) ptr = sb.reset_val;
	else
	    *(Bit8u *) ptr = sb.reset_val;
	return 1;
    }
    return 0;
}

static Bit8u mpu401_io_read(ioport_t port)
{
    ioport_t addr;
    Bit8u r = 0xff;

    addr = port - config.mpu401_base;

    switch (addr) {
    case 0:
	/* Read data port */
	if (dspio_get_midi_in_fillup(sb.dspio))
	    r = dspio_get_midi_in_byte(sb.dspio);
	else
	    S_printf("MPU401: ERROR: No data to read\n");
	S_printf("MPU401: Read data port = 0x%02x, %i bytes still in queue\n",
	     r, dspio_get_midi_in_fillup(sb.dspio));
	if (!dspio_get_midi_in_fillup(sb.dspio))
	    sb_deactivate_irq(SB_IRQ_MPU401);
	sb_run_irq(SB_IRQ_MPU401);
	break;
    case 1:
	/* Read status port */
	/* 0x40=OUTPUT_AVAIL; 0x80=INPUT_AVAIL */
	r = 0xff & (~0x40);	/* Output is always possible */
	if (dspio_get_midi_in_fillup(sb.dspio))
	    r &= (~0x80);
	S_printf("MPU401: Read status port = 0x%02x\n", r);
	break;
    }
    return r;
}

static void mpu401_io_write(ioport_t port, Bit8u value)
{
    uint32_t addr;
    addr = port - config.mpu401_base;

    switch (addr) {
    case 0:
	/* Write data port */
	if (debug_level('S') > 5)
		S_printf("MPU401: Write 0x%02x to data port\n", value);
	dspio_write_midi(sb.dspio, value);
	if (!sb.mpu401_uart && debug_level('S') > 5)
		S_printf("MPU401: intelligent mode write unhandled\n");
	break;
    case 1:
	/* Write command port */
	S_printf("MPU401: Write 0x%02x to command port\n", value);
	dspio_clear_midi_in_fifo(sb.dspio);
	/* the following doc:
	 * http://www.piclist.com/techref/io/serial/midi/mpu.html
	 * says 3f does not need ACK. But dosbox sources say that
	 * it does. Someone please try on a real HW? */
	dspio_put_midi_in_byte(sb.dspio, 0xfe);	/* A command is sent: MPU_ACK it next time */
	switch (value) {
	case 0x3f:		// 0x3F = UART mode
	    sb.mpu401_uart = 1;
	    break;
	case 0xff:		// 0xFF = reset MPU
	    sb.mpu401_uart = 0;
	    dspio_stop_midi(sb.dspio);
	    break;
	case 0x80:		// Clock ??
	    break;
	case 0xac:		// Query version
	    dspio_put_midi_in_byte(sb.dspio, 0x15);
	    break;
	case 0xad:		// Query revision
	    dspio_put_midi_in_byte(sb.dspio, 0x1);
	    break;
	}
	sb_activate_irq(SB_IRQ_MPU401);
	break;
    }
}

static void process_sb_midi_input(void)
{
    Bit8u tmp;
    if (sb_midi_timestamp()) {
	Bit32u time = dspio_get_midi_in_time(sb.dspio);
	dsp_write_output(time);
	dsp_write_output(time >> 8);
	dsp_write_output(time >> 16);
    }
    tmp = dspio_get_midi_in_byte(sb.dspio);
    dsp_write_output(tmp);
}

void sb_handle_midi_data(void)
{
#define MPU401_IN_FIFO_TRIGGER 1
    if (sb_midi_input()) {
	while (dspio_get_midi_in_fillup(sb.dspio))
	    process_sb_midi_input();
	if (sb_midi_int())
	    sb_activate_irq(SB_IRQ_MIDI);
    } else if (sb.mpu401_uart) {
	if (dspio_get_midi_in_fillup(sb.dspio) == MPU401_IN_FIFO_TRIGGER)
	    sb_activate_irq(SB_IRQ_MPU401);
    }
}

void run_sb(void)
{
    if (!config.sound)
	return;
    dspio_timer(sb.dspio);
}

static void mpu401_init(void)
{
    emu_iodev_t io_device;

    S_printf("MPU401: MPU-401 Initialisation\n");

    if (config.mpu401_irq == -1) {
	config.mpu401_irq = config.sb_irq;
	config.mpu401_uart_irq_mt32 = config.mpu401_irq_mt32;
	S_printf("SB: mpu401 irq set to %i\n", config.mpu401_irq);
    } else {
	config.mpu401_uart_irq_mt32 = config.mpu401_irq;
    }

    /* This is the MPU-401 */
    io_device.read_portb = mpu401_io_read;
    io_device.write_portb = mpu401_io_write;
    io_device.read_portw = NULL;
    io_device.write_portw = NULL;
    io_device.read_portd = NULL;
    io_device.write_portd = NULL;
    io_device.handler_name = "Midi Emulation";
    io_device.start_addr = config.mpu401_base;
    io_device.end_addr = config.mpu401_base + 0x001;
    if (config.mpu401_irq == config.sb_irq) {
	S_printf("SB: same irq for DSP and MPU401\n");
	io_device.irq = EMU_NO_IRQ;
    } else {
	io_device.irq = config.mpu401_irq;
    }
    io_device.fd = -1;
    if (port_register_handler(io_device, 0) != 0)
	error("MPU-401: Cannot registering port handler\n");

    S_printf("MPU401: MPU-401 Initialisation - Base 0x%03x \n",
	     config.mpu401_base);
}

static void mpu401_done(void)
{
}

static void sb_dsp_init(void)
{
    rng_init(&sb.dsp_queue, DSP_QUEUE_SIZE, 1);

    sb.reset_val = 0xaa;
}

static void sb_dsp_done(void)
{
    rng_destroy(&sb.dsp_queue);
}

/*
 * Sound Initialisation
 * ====================
 */
static void sb_init(void)
{
    emu_iodev_t io_device;

    S_printf("SB: SB Initialisation\n");

    /* SB Emulation */
    io_device.read_portb = sb_io_read;
    io_device.write_portb = sb_io_write;
    io_device.read_portw = NULL;
    io_device.write_portw = NULL;
    io_device.read_portd = NULL;
    io_device.write_portd = NULL;
    io_device.handler_name = "SB Emulation";
    io_device.start_addr = config.sb_base;
    io_device.end_addr = config.sb_base + 0x013;
    io_device.irq = config.sb_irq;
    io_device.fd = -1;
    if (port_register_handler(io_device, 0) != 0) {
	error("SB: Cannot registering DSP port handler\n");
    }

    sb_dsp_init();
    sb_mixer_init();
    opl3_init();
    mpu401_init();

    S_printf("SB: Initialisation - Base 0x%03x\n", config.sb_base);
}

static void sb_done(void)
{
    sb_dsp_done();
    adlib_done();
    mpu401_done();
}

void sound_init(void)
{
    if (!config.sound)
	return;
    sb.dspio = dspio_init();
    if (!sb.dspio) {
	error("dspio faild\n");
	leavedos(93);
    }
    sb_init();
}

void sound_reset(void)
{
    if (config.sound) {
	dspio_reset(sb.dspio);
	sb_reset();
    }
}

void sound_done(void)
{
    if (config.sound) {
	sb_done();
	dspio_done(sb.dspio);
    }
}
