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
#include "emu.h"
#include "init.h"
#include "sound/sound.h"
#include "sound/sndpcm.h"
#include "sound/midi.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static const char *midomidid_name = "MIDI Output: midid plugin";
static pid_t midid_pid = -1;
static int pipe_in[2], pipe_out[2], pcm_stream;
static int timid_capt = 1, midid_dev = 0;

static void midomidid_async(void)
{
    char buf[16384];
    int n, selret;
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(pipe_in[0], &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    while ((selret = select(pipe_in[0] + 1, &rfds, NULL, NULL, &tv)) > 0) {
	n = RPT_SYSCALL(read(pipe_in[0], buf, sizeof(buf)));
	if (n > 0) {
	    pcm_write_samples(buf, n, 44100, PCM_FORMAT_S16_LE,
			      pcm_stream);
	} else {
	    break;
	}
	FD_ZERO(&rfds);
	FD_SET(pipe_in[0], &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
    }
}

static int midomidid_init(void)
{
#define GM 'g'
#define MT32 'm'
    char *midid_bin = "midid";
    char *midid_args_1 = "-d2 -C";
    char *midid_args_t2 = "-d%d";
    char midid_args_2[80];
    char *midid_args_xtra = "-m -o1";
//  char *midid_args_xtra = "-o1";
    char *midid_cmd, *ptr;
    sigset_t sigs;
    int i;
#define T_MAX_ARGS 255
    char *midid_args[T_MAX_ARGS];
    if (pipe(pipe_in) == -1) {
	error("pipe() failed: %s\n", strerror(errno));
	return 0;
    }
    if (pipe(pipe_out) == -1) {
	error("pipe() failed: %s\n", strerror(errno));
	return 0;
    }
    switch ((midid_pid = fork())) {
    case 0:
	close(pipe_in[0]);
	close(pipe_out[1]);
	dup2(pipe_out[0], STDIN_FILENO);
	dup2(pipe_in[1], STDOUT_FILENO);
	close(pipe_out[0]);
	close(pipe_in[1]);
#if 1
	/* redirect stderr to /dev/null */
	close(STDERR_FILENO);
	open("/dev/null", O_WRONLY);
#endif
	/* unblock SIGIO and SIGALRM as midid/timidity use it */
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGIO);
	sigaddset(&sigs, SIGALRM);
	signal(SIGIO, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	sigprocmask(SIG_UNBLOCK, &sigs, NULL);

	if (timid_capt)
	    strcpy(midid_args_2, midid_args_1);
	else
	    sprintf(midid_args_2, midid_args_t2, midid_dev);
	asprintf(&midid_cmd, "%s %s %s", midid_bin, midid_args_2,
		 midid_args_xtra);
	ptr = midid_cmd;
	for (i = 0; i < T_MAX_ARGS; i++) {
	    do
		midid_args[i] = strsep(&ptr, " ");
	    while (midid_args[i] && !midid_args[i][0]);
	    if (!midid_args[i])
		break;
	}
	execvp(midid_bin, midid_args);
	free(midid_cmd);
	exit(1);
	break;
    case -1:
	error("fork() failed: %s\n", strerror(errno));
	close(pipe_out[0]);
	close(pipe_in[1]);
	close(pipe_in[0]);
	close(pipe_out[1]);
	return 0;
    }
    close(pipe_out[0]);
    close(pipe_in[1]);
    if (timid_capt) {
	pcm_stream = pcm_allocate_stream(2, "MIDI");
	add_to_io_select(pipe_in[0], 1, midomidid_async);
    }
    return 1;
}

static void midomidid_done(void)
{
    int status;
    if (midid_pid == -1)
	return;
    if (timid_capt) {
	remove_from_io_select(pipe_in[0], 1);
    }
    close(pipe_out[1]);
    close(pipe_in[0]);
    waitpid(midid_pid, &status, 0);
}

static void midomidid_reset(void)
{
    if (midid_pid == -1)
	return;
#if 0
    kill(midid_pid, SIGUSR1);
#endif
}

static void midomidid_write(unsigned char val)
{
    write(pipe_out[1], &val, 1);
}

CONSTRUCTOR(static int midomidid_register(void))
{
    struct midi_out_plugin midomidid;
    midomidid.name = midomidid_name;
    midomidid.init = midomidid_init;
    midomidid.done = midomidid_done;
    midomidid.reset = midomidid_reset;
    midomidid.write = midomidid_write;
    midomidid.stop = NULL;
#if 0
    return midi_register_output_plugin(midomidid);
#else
    return 0;
#endif
}
