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
 * Purpose: munt midi synth
 *
 * Author: Stas Sergeev
 *
 */

#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <limits.h>
#include <mt32emu/c_interface/c_interface.h>
#include "emu.h"
#include "init.h"
#include "timers.h"
#include "sound/midi.h"
#include "sound/sound.h"

#define midomunt_name "munt"
#define midomunt_longname "MIDI Output: munt device"

static mt32emu_context ctx;
static int pcm_stream;
static int output_running, pcm_running;
static double mf_time_base;
#define MUNT_CHANNELS 2
#define MUNT_MAX_BUF 512
#define MUNT_MIN_BUF 128
static const int munt_format = PCM_FORMAT_S16_LE;
static int munt_srate;

static pthread_t syn_thr;
static sem_t syn_sem;
static pthread_mutex_t syn_mtx = PTHREAD_MUTEX_INITIALIZER;
static void *synth_thread(void *arg);

static int midomunt_init(void *arg)
{
    mt32emu_return_code ret;
    char p[PATH_MAX];

    ctx = mt32emu_create_context((mt32emu_report_handler_i){ NULL }, NULL);
    strcpy(p, config.munt_roms_dir);
    strcat(p, "/MT32_CONTROL.ROM");
    ret = mt32emu_add_rom_file(ctx, p);
    if (ret != MT32EMU_RC_ADDED_CONTROL_ROM) {
	error("MUNT: Can't find %s\n", p);
	goto err;
    }
    strcpy(p, config.munt_roms_dir);
    strcat(p, "/MT32_PCM.ROM");
    ret = mt32emu_add_rom_file(ctx, p);
    if (ret != MT32EMU_RC_ADDED_PCM_ROM) {
	error("MUNT: Can't find %s\n", p);
	goto err;
    }

    sem_init(&syn_sem, 0, 0);
    pthread_create(&syn_thr, NULL, synth_thread, NULL);
    pthread_setname_np(syn_thr, "dosemu: munt");

    pcm_stream = pcm_allocate_stream(MUNT_CHANNELS, "MIDI-MT32",
	    (void*)MC_MIDI);

    return 1;

err:
    mt32emu_free_context(ctx);
    return 0;
}

static void midomunt_done(void *arg)
{
    pthread_cancel(syn_thr);
    pthread_join(syn_thr, NULL);
    sem_destroy(&syn_sem);
    mt32emu_free_context(ctx);
}

static void midomunt_start(void)
{
    mt32emu_return_code ret;

    pthread_mutex_lock(&syn_mtx);
    ret = mt32emu_open_synth(ctx);
    if (ret != MT32EMU_RC_OK) {
	pthread_mutex_unlock(&syn_mtx);
	error("MUNT: open_synth() failed\n");
	return;
    }
    munt_srate = mt32emu_get_actual_stereo_output_samplerate(ctx);
    S_printf("MIDI: starting munt, srate=%i\n", munt_srate);
    mf_time_base = GETusTIME(0);
    pcm_prepare_stream(pcm_stream);
    output_running = 1;
    pthread_mutex_unlock(&syn_mtx);
}

static void midomunt_write(unsigned char val)
{
    unsigned long long now;
    int tstamp;

    if (!output_running)
	midomunt_start();

    now = GETusTIME(0);
    /* timstamp is measured in samples */
    tstamp = (now - mf_time_base) * munt_srate / 1000000;
    mt32emu_parse_stream_at(ctx, &val, 1, tstamp);
}

static void midomunt_stop(void *arg)
{
    if (!output_running)
	return;
    pthread_mutex_lock(&syn_mtx);
    mt32emu_close_synth(ctx);
    if (pcm_running)
	pcm_flush(pcm_stream);
    pcm_running = 0;
    output_running = 0;
    pthread_mutex_unlock(&syn_mtx);
}

static void mf_process_samples(int nframes)
{
    sndbuf_t buf[MUNT_MAX_BUF][MUNT_CHANNELS];

    mt32emu_render_bit16s(ctx, (sndbuf_t *)buf, nframes);
    pcm_running = 1;
    pcm_write_interleaved(buf, nframes, munt_srate, munt_format,
	    MUNT_CHANNELS, pcm_stream);
}

static void process_samples(long long now, int min_buf)
{
    int nframes, retry;
    double period, mf_time_cur;
    mf_time_cur = pcm_time_lock(pcm_stream);
    do {
	retry = 0;
	period = pcm_frame_period_us(munt_srate);
	nframes = (now - mf_time_cur) / period;
	if (nframes > MUNT_MAX_BUF) {
	    nframes = MUNT_MAX_BUF;
	    retry = 1;
	}
	if (nframes >= min_buf) {
	    mf_process_samples(nframes);
	    mf_time_cur = pcm_get_stream_time(pcm_stream);
	    if (debug_level('S') >= 5)
		S_printf("MIDI: processed %i samples with munt\n", nframes);
	}
    } while (retry);
    pcm_time_unlock(pcm_stream);
}

static void *synth_thread(void *arg)
{
    while (1) {
	sem_wait(&syn_sem);
	pthread_mutex_lock(&syn_mtx);
	if (!output_running) {
		pthread_mutex_unlock(&syn_mtx);
		continue;
	}
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	process_samples(GETusTIME(0), MUNT_MIN_BUF);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_mutex_unlock(&syn_mtx);
    }
    return NULL;
}

static void midomunt_run(void)
{
    if (!output_running)
	return;
    sem_post(&syn_sem);
}

static int midomunt_cfg(void *arg)
{
    return pcm_parse_cfg(config.midi_driver, midomunt_name);
}

static const struct midi_out_plugin midomunt = {
    .name = midomunt_name,
    .longname = midomunt_longname,
    .open = midomunt_init,
    .close = midomunt_done,
    .write = midomunt_write,
    .stop = midomunt_stop,
    .run = midomunt_run,
    .get_cfg = midomunt_cfg,
    .stype = ST_MT32,
    .weight = MIDI_W_PCM | MIDI_W_PREFERRED,
};

CONSTRUCTOR(static void midomunt_register(void))
{
    midi_register_output_plugin(&midomunt);
}
