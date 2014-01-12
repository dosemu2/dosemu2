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
 * Purpose: sound input demo plugin (gets data from pipe).
 *
 * Author: Stas Sergeev
 */

#include "emu.h"
#include "init.h"
#include "sound/sound.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "config/plugin_config.h"

#define ENABLED EXPERIMENTAL

static int pipe_in = -1;
#if ENABLED
static int pcm_stream, pcm_running;
static const char *PIPE_NAME = "/tmp/desnd_pipe";
#endif
#define PIPE_FREQ 44100
#define PIPE_CHANS 2
#define PIPE_FMT PCM_FORMAT_S16_LE

#if ENABLED
static void pipe_async(void *arg)
{
    sndbuf_t buf[16384][SNDBUF_CHANS];
    int n, selret;
    fd_set rfds;
    struct timeval tv;

    FD_ZERO(&rfds);
    FD_SET(pipe_in, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    while ((selret = select(pipe_in + 1, &rfds, NULL, NULL, &tv)) > 0) {
	n = RPT_SYSCALL(read(pipe_in, buf, sizeof(buf)));
	if (n > 0) {
	    int frames = n / (pcm_format_size(PIPE_FMT) * PIPE_CHANS);
	    pcm_running = 1;
	    pcm_write_interleaved(buf, frames, PIPE_FREQ, PIPE_FMT,
			      PIPE_CHANS, pcm_stream);
	} else {
	    break;
	}
	FD_ZERO(&rfds);
	FD_SET(pipe_in, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
    }
}
#endif

void sndpipe_plugin_init(void)
{
#if ENABLED
    mkfifo(PIPE_NAME, 0666);
    pipe_in = open(PIPE_NAME, O_RDONLY | O_NONBLOCK);
    if (pipe_in == -1)
	return;
    pcm_stream = pcm_allocate_stream(2, "PCM IN", PCM_ID_R);
    add_to_io_select(pipe_in, pipe_async, NULL);
#endif
}

void sndpipe_plugin_close(void)
{
    if (pipe_in == -1)
	return;
    if (pcm_running)
	pcm_flush(pcm_stream);
    remove_from_io_select(pipe_in);
    close(pipe_in);
}
