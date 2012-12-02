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
 * Purpose: glue between dosemu and the MAME OPL3 emulator.
 *
 * Author: Stas Sergeev.
 */

#include "emu.h"
#include "port.h"
#include "timers.h"
#include "utilities.h"
#include "sound/sound.h"
#include "sound/sndpcm.h"
#include "adlib.h"
#include <limits.h>

#define ADLIB_BASE 0x388
#define OPL3_INTERNAL_FREQ    14400000	// The OPL3 operates at 14.4MHz
#define OPL3_MAX_BUF 512
#define OPL3_MIN_BUF 128
#define ADLIB_CHANNELS 2

#define ADLIB_THRESHOLD 20000000
#define ADLIB_RUNNING() (adlib_time_cur > 0)
#define ADLIB_RUN() (adlib_time_cur = GETusTIME(0))
#define ADLIB_STOP() (adlib_time_cur = 0)

#ifdef HAS_YMF262
static void *opl3;
#endif
static long long opl3_timers[2];
static int adlib_strm;
static double adlib_time_cur, adlib_time_last;
#if OPL3_SAMPLE_BITS==16
static const int opl3_format = PCM_FORMAT_S16_LE;
#else
static const int opl3_format = PCM_FORMAT_S8;
#endif
#if 0
static const int opl3_rate = 44100;
#else
static const int opl3_rate = 22050;
#endif

Bit8u adlib_io_read_base(ioport_t port)
{
    Bit8u ret;
#ifdef HAS_YMF262
    ret = YMF262Read(opl3, port);
#else
    ret = 0xff;
#endif
    S_printf("Adlib: Read %hhx from port %x\n", ret, port);
    return ret;
}

static Bit8u adlib_io_read(ioport_t port)
{
    return adlib_io_read_base(port - ADLIB_BASE);
}

void adlib_io_write_base(ioport_t port, Bit8u value)
{
    adlib_time_last = GETusTIME(0);
    S_printf("Adlib: Write %hhx to port %x\n", value, port);
#ifdef HAS_YMF262
    YMF262Write(opl3, port, value);
#endif
}

static void adlib_io_write(ioport_t port, Bit8u value)
{
    adlib_io_write_base(port - ADLIB_BASE, value);
}

#ifdef HAS_YMF262
static void opl3_set_timer(void *param, int num, double interval_Sec)
{
    long long *timers = param;
    if (interval_Sec > 0)
	timers[num] += GETusTIME(0) + interval_Sec * 1000000;
    else
	timers[num] = 0;
    S_printf("Adlib: timer %i set to %ius\n", num,
	     (int) (interval_Sec * 1000000));
}

static void opl3_update(void *param, int min_interval_us)
{
    if (!ADLIB_RUNNING())
	ADLIB_RUN();
    run_new_sb();
}

static void opl3_irq(void *param, int irq)
{
    S_printf("SB: OPL3 IRQ (%i)\n", irq);
    /* this IRQ is not wired, nothing to do */
}
#endif

void opl3_init(void)
{
    emu_iodev_t io_device;

    S_printf("SB: OPL3 Initialization\n");

    /* This is the FM (Adlib + Advanced Adlib) */
    io_device.read_portb = adlib_io_read;
    io_device.write_portb = adlib_io_write;
    io_device.read_portw = NULL;
    io_device.write_portw = NULL;
    io_device.read_portd = NULL;
    io_device.write_portd = NULL;
    io_device.handler_name = "OPL3";
    io_device.start_addr = ADLIB_BASE;
    io_device.end_addr = ADLIB_BASE + 3;
    io_device.irq = EMU_NO_IRQ;
    io_device.fd = -1;
    if (port_register_handler(io_device, 0) != 0) {
	error("ADLIB: Cannot registering port handler\n");
    }
#ifdef HAS_YMF262
    opl3 = YMF262Init(OPL3_INTERNAL_FREQ, opl3_rate);
    YMF262SetTimerHandler(opl3, opl3_set_timer, opl3_timers);
    YMF262SetUpdateHandler(opl3, opl3_update, NULL);
    YMF262SetIRQHandler(opl3, opl3_irq, NULL);
#endif
}

void adlib_init(void)
{
    adlib_strm = pcm_allocate_stream(ADLIB_CHANNELS, "Adlib");
}

void adlib_reset(void)
{
    opl3_timers[0] = opl3_timers[1] = 0;
    adlib_time_cur = adlib_time_last = 0;
#ifdef HAS_YMF262
    YMF262ResetChip(opl3);
#endif
}

void adlib_done(void)
{
#ifdef HAS_YMF262
    YMF262Shutdown(opl3);
#endif
}

#ifdef HAS_YMF262
static void adlib_process_samples(int nframes)
{
    const int chan_map[] =
#if ADLIB_CHANNELS == 2
	{ 0, 1, 0, 1 };
#elif ADLIB_CHANNELS == 1
	{ 0, 0, 0, 0 };
#elif ADLIB_CHANNELS == 4
	{ 0, 1, 2, 3 };
#else
#error ADLIB_CHANNELS is wrong
	{ -1, -1, -1, -1 };
#endif
    int i, j, k;
    OPL3SAMPLE *chans[4], buf[4][OPL3_MAX_BUF],
	    buf3[OPL3_MAX_BUF][ADLIB_CHANNELS];

    for (i = 0; i < 4; i++)
	chans[i] = &buf[i][0];

    YMF262UpdateOne(opl3, chans, nframes);

    for (i = 0; i < nframes; i++) {
	for (j = 0; j < ADLIB_CHANNELS; j++) {
	    int buf2 = 0;
	    for (k = 0; k < ARRAY_SIZE(chan_map); k++) {
		if (chan_map[k] == j)
		    buf2 += buf[k][i];
	    }
	    buf3[i][j] = pcm_samp_cutoff(buf2, opl3_format);
	}
    }
    pcm_write_samples(buf3, nframes * ADLIB_CHANNELS *
	    pcm_format_size(opl3_format), opl3_rate, opl3_format, adlib_strm);
}
#endif

void adlib_timer(void)
{
#ifdef HAS_YMF262
    int i, nframes;
    double period;
    long long now = GETusTIME(0);

    if (adlib_time_cur - adlib_time_last > ADLIB_THRESHOLD) {
	ADLIB_STOP();
	pcm_flush(adlib_strm);
    }
    if (ADLIB_RUNNING()) {
	period = pcm_frame_period_us(opl3_rate);
	nframes = (now - adlib_time_cur) / period;
	if (nframes > OPL3_MAX_BUF)
	    nframes = OPL3_MAX_BUF;
	if (nframes >= OPL3_MIN_BUF) {
	    adlib_process_samples(nframes);
	    adlib_time_cur += nframes * period;
	    S_printf("SB: processed %i Adlib samples\n", nframes);
	}
    }

    for (i = 0; i < 2; i++) {
	if (opl3_timers[i] > 0 && now > opl3_timers[i]) {
	    S_printf("Adlib: timer %i expired\n", i);
	    opl3_timers[i] = now - opl3_timers[i];
	    YMF262TimerOver(opl3, i);
	}
    }
#endif
}
