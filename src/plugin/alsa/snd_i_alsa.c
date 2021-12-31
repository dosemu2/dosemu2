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
 * Purpose: alsa sound input.
 *
 * Author: Stas Sergeev
 */

#include "emu.h"
#include "init.h"
#include "sound/sound.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <alsa/asoundlib.h>

#define alsain_name "alsa_in"
#define alsain_longname "Sounds input: alsa"
static int pcm_stream, pcm_running;
static const char *alsa_dev_default = "default";
static char *alsa_dev;
static const char *device_name_param = "dev_name";
static snd_pcm_t *capture_handle;
static unsigned int alsa_freq = 44100;
static struct pollfd *pfds;
static int num_pfds;
#define ALSAIN_CHANS 2
#define ALSAIN_FMT PCM_FORMAT_S16_LE
#define _FMT(x) SND_##x
#define FMT(x) _FMT(x)

static void alsain_async(void *arg)
{
    sndbuf_t buf[16384][SNDBUF_CHANS];
    int n, pollret;
    unsigned short revent;

    while ((pollret = poll(pfds, num_pfds, 0)) > 0) {
	snd_pcm_poll_descriptors_revents (capture_handle, pfds, num_pfds,
		    &revent);
	if (!(revent & POLLIN))
	    continue;
	n = snd_pcm_readi (capture_handle, buf,
		snd_pcm_bytes_to_frames (capture_handle, sizeof(buf)));
	if (n > 0) {
	    if (debug_level('S') > 5)
		S_printf("ALSA: read %i frames\n", n);
	    pcm_running = 1;
	    pcm_write_interleaved(buf, n, alsa_freq, ALSAIN_FMT,
			      ALSAIN_CHANS, pcm_stream);
	} else {
	    error("ALSA: read returned %i\n", n);
	    break;
	}
    }
    if (pollret < 0 && errno != EINTR)
	error("ALSA: poll returned %i, %s\n", pollret, strerror(errno));
}

static int alsain_open(void *arg)
{
    char *dname = pcm_parse_params(config.snd_plugin_params,
	    alsain_name, device_name_param);
    alsa_dev = dname ?: strdup(alsa_dev_default);

    pcm_stream = pcm_allocate_stream(ALSAIN_CHANS, "MIC INPUT",
	    (void*)MC_MIC);
    return 1;
}

static void alsain_close(void *arg)
{
    free(alsa_dev);
}

static void alsain_start(void *arg)
{
    int err, cnt, i;
    snd_pcm_hw_params_t *hw_params;
    if ((err = snd_pcm_open (&capture_handle, alsa_dev, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0) {
	error("cannot open audio device %s (%s)\n", 
		 alsa_dev,
		 snd_strerror (err));
	return;
    }

    if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
	error("cannot allocate hardware parameter structure (%s)\n",
		 snd_strerror (err));
	return;
    }

    if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
	error("cannot initialize hardware parameter structure (%s)\n",
		 snd_strerror (err));
	return;
    }

    if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
	error("cannot set access type (%s)\n",
		 snd_strerror (err));
	return;
    }

    if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, FMT(ALSAIN_FMT))) < 0) {
	error("cannot set sample format (%s)\n",
		 snd_strerror (err));
	return;
    }

    if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &alsa_freq, NULL)) < 0) {
	error("cannot set sample rate (%s)\n",
		 snd_strerror (err));
	return;
    }

    if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, ALSAIN_CHANS)) < 0) {
	error("cannot set channel count (%s)\n",
		 snd_strerror (err));
	return;
    }

    if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
	error("cannot set parameters (%s)\n",
		 snd_strerror (err));
	return;
    }

    snd_pcm_hw_params_free (hw_params);

    if ((err = snd_pcm_prepare (capture_handle)) < 0) {
	error("cannot prepare audio interface for use (%s)\n",
		 snd_strerror (err));
	return;
    }

    cnt = snd_pcm_poll_descriptors_count (capture_handle);
    pfds = malloc(sizeof(struct pollfd) * cnt);
    num_pfds = snd_pcm_poll_descriptors (capture_handle, pfds, cnt);
    for (i = 0; i < num_pfds; i++)
	add_to_io_select(pfds[i].fd, alsain_async, NULL);
    S_printf("ALSA: input started\n");
}

static void alsain_stop(void *arg)
{
    int i;
    if (pcm_running)
	pcm_flush(pcm_stream);
    pcm_running = 0;
    for (i = 0; i < num_pfds; i++)
      remove_from_io_select(pfds[i].fd);
    snd_pcm_close (capture_handle);
    free(pfds);
    S_printf("ALSA: input stopped\n");
}

static const struct pcm_recorder recorder
#ifdef __cplusplus
{
    alsain_name,
    alsain_longname,
    alsain_open,
    alsain_close,
    alsain_start,
    alsain_stop,
    (void *)MC_MIC,
};
#else
= {
    .name = alsain_name,
    .longname = alsain_longname,
    .open = alsain_open,
    .close = alsain_close,
    .start = alsain_start,
    .stop = alsain_stop,
    .id2 = (void *)MC_MIC,
};
#endif

CONSTRUCTOR(static void alsain_init(void))
{
    pcm_register_recorder(&recorder, NULL);
}
