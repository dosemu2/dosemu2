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
 * Purpose: glue between dosemu and the DOSBOX/MAME OPL3 emulator.
 *
 * Author: Stas Sergeev.
 */

#include "emu.h"
#include "port.h"
#include "timers.h"
#include "utilities.h"
#include "sound/sound.h"
#include "sound.h"
#include "adlib.h"
#include "dbadlib.h"
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>

#define ADLIB_BASE 0x388
#define OPL3_INTERNAL_FREQ    14400000	// The OPL3 operates at 14.4MHz
#define OPL3_MAX_BUF 512
#define OPL3_MIN_BUF 128
#define ADLIB_CHANNELS SNDBUF_CHANS

#define ADLIB_THRESHOLD 20000000

static AdlibTimer opl3_timers[2];
#define OPL3_SAMPLE_BITS 16
#if (OPL3_SAMPLE_BITS==16)
typedef Bit16s OPL3SAMPLE;
#elif (OPL3_SAMPLE_BITS==8)
typedef Bit8s OPL3SAMPLE;
#endif
static int adlib_strm;
static int adlib_running;
static double adlib_time_last;
#if OPL3_SAMPLE_BITS==16
static const int opl3_format = PCM_FORMAT_S16_LE;
#else
static const int opl3_format = PCM_FORMAT_S8;
#endif
static const int opl3_rate = 44100;

static pthread_mutex_t synth_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t run_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_t syn_thr;
static sem_t syn_sem;
static void *synth_thread(void *arg);

Bit8u adlib_io_read_base(ioport_t port)
{
    Bit8u ret;
    pthread_mutex_lock(&synth_mtx);
    ret = dbadlib_PortRead(opl3_timers, port);
    pthread_mutex_unlock(&synth_mtx);
    if (debug_level('S') >= 9)
	S_printf("Adlib: Read %hhx from port %x\n", ret, port);
    return ret;
}

static Bit8u adlib_io_read(ioport_t port)
{
    return adlib_io_read_base(port - ADLIB_BASE);
}

static void opl3_update(void);

void adlib_io_write_base(ioport_t port, Bit8u value)
{
    adlib_time_last = GETusTIME(0);
    if (debug_level('S') >= 9)
	S_printf("Adlib: Write %hhx to port %x\n", value, port);
    if ( port&1 ) {
      opl3_update();
    }
    pthread_mutex_lock(&synth_mtx);
    dbadlib_PortWrite(opl3_timers, port, value);
    pthread_mutex_unlock(&synth_mtx);
}

static void adlib_io_write(ioport_t port, Bit8u value)
{
    adlib_io_write_base(port - ADLIB_BASE, value);
}

static void adlib_start(void)
{
    pcm_prepare_stream(adlib_strm);
    pthread_mutex_lock(&run_mtx);
    adlib_running = 1;
    pthread_mutex_unlock(&run_mtx);
}

static void opl3_update(void)
{
    int a_run;
    pthread_mutex_lock(&run_mtx);
    a_run = adlib_running;
    pthread_mutex_unlock(&run_mtx);
    if (!a_run)
	adlib_start();
    run_sb();
}

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

    dbadlib_init(opl3_timers, opl3_rate);

    sem_init(&syn_sem, 0, 0);
    pthread_create(&syn_thr, NULL, synth_thread, NULL);
    pthread_setname_np(syn_thr, "dosemu: adlib");
    adlib_strm = pcm_allocate_stream(ADLIB_CHANNELS, "Adlib", (void*)MC_MIDI);
}

void adlib_reset(void)
{
    adlib_time_last = 0;
}

void adlib_done(void)
{
    pthread_cancel(syn_thr);
    pthread_join(syn_thr, NULL);
    sem_destroy(&syn_sem);
}

static void adlib_process_samples(int nframes)
{
    sndbuf_t buf[OPL3_MAX_BUF][SNDBUF_CHANS];
    pthread_mutex_lock(&synth_mtx);
    dbadlib_generate(nframes, buf);
    pthread_mutex_unlock(&synth_mtx);
    pcm_write_interleaved(buf, nframes, opl3_rate, opl3_format,
	    ADLIB_CHANNELS, adlib_strm);
}

/* we know that timer updates do not affect synth, so disable that code */
#define UPDATE_TIMERS 0
static void adlib_run(void)
{
#if UPDATE_TIMERS
    int i;
#endif
    int nframes;
    double period, adlib_time_cur;
    long long now;
    int time_adj;

    adlib_time_cur = pcm_time_lock(adlib_strm);
    if (adlib_time_cur - adlib_time_last > ADLIB_THRESHOLD) {
	pcm_flush(adlib_strm);
	pcm_time_unlock(adlib_strm);
	pthread_mutex_lock(&run_mtx);
	adlib_running = 0;
	pthread_mutex_unlock(&run_mtx);
	return;
    }
    if (adlib_running) do {
	now = GETusTIME(0);
	time_adj = 0;
#if UPDATE_TIMERS
	/* find the closest timer */
	for (i = 0; i < 2; i++) {
	    if (opl3_timers[i].enabled && !opl3_timers[i].overflow &&
			!opl3_timers[i].masked && now > opl3_timers[i].start) {
		now = opl3_timers[i].start;
		time_adj = 1;
		if (debug_level('S') >= 9)
		    S_printf("Adlib: time adjusted to timer %i\n", i);
	    }
	}
#endif
	period = pcm_frame_period_us(opl3_rate);
	nframes = (now - adlib_time_cur) / period;
	if (nframes > OPL3_MAX_BUF)
	    nframes = OPL3_MAX_BUF;
	/* if time was adjusted, ignore MIN_BUF */
	if (nframes >= OPL3_MIN_BUF || (nframes && time_adj)) {
	    adlib_process_samples(nframes);
	    adlib_time_cur = pcm_get_stream_time(adlib_strm);
	    if (debug_level('S') >= 7)
		S_printf("SB: processed %i Adlib samples\n", nframes);
	}
#if UPDATE_TIMERS
	for (i = 0; i < 2; i++) {
	    pthread_mutex_lock(&synth_mtx);
	    AdlibTimer__Update(&opl3_timers[i], now);
	    pthread_mutex_unlock(&synth_mtx);
	}
#endif
    } while (time_adj);
    pcm_time_unlock(adlib_strm);
}

static void *synth_thread(void *arg)
{
    int a_run;
    while (1) {
	sem_wait(&syn_sem);
	pthread_mutex_lock(&run_mtx);
	a_run = adlib_running;
	pthread_mutex_unlock(&run_mtx);
	if (!a_run)
	    continue;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	adlib_run();
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    return NULL;
}

void adlib_timer(void)
{
    sem_post(&syn_sem);
}
