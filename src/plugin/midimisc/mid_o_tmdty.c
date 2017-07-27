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
#include "utilities.h"
#include "sig.h"
#include "sound/sound.h"
#include "sound/midi.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#define TMDTY_HOST "localhost"
#define TMDTY_FREQ 44100
#define TMDTY_8BIT 0
#define TMDTY_UNS 0
#define TMDTY_CAPT 1
#define TMDTY_BIN "timidity"
#define TMDTY_ARGS "-EFreverb=0 -EFchorus=0 -EFresamp=1 -EFvlpf=0 -EFns=0"

#define TMDTY_CHANS 2

#define midotmdty_name "timidity"
#define midotmdty_longname "MIDI Output: TiMidity++ plugin"

static int ctrl_sock_in, ctrl_sock_out, data_sock;
static pid_t tmdty_pid = -1;
static int pcm_stream, pcm_running;

static void midotmdty_reset(void);

static void midotmdty_io(void *arg)
{
    sndbuf_t buf[16384][SNDBUF_CHANS];
    int n, selret, fmt;
    fd_set rfds;
    struct timeval tv;
    fmt = pcm_get_format(TMDTY_8BIT ? 0 : 1, TMDTY_UNS ? 0 : 1);
    FD_ZERO(&rfds);
    FD_SET(data_sock, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    while ((selret = select(data_sock + 1, &rfds, NULL, NULL, &tv)) > 0) {
	n = RPT_SYSCALL(read(data_sock, buf, sizeof(buf)));
	if (n > 0) {
	    int frames = n / (pcm_format_size(fmt) * TMDTY_CHANS);
	    pcm_running = 1;
	    pcm_write_interleaved(buf, frames, TMDTY_FREQ, fmt,
			      TMDTY_CHANS, pcm_stream);
	} else {
	    break;
	}
	FD_ZERO(&rfds);
	FD_SET(data_sock, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
    }
}

static int midotmdty_preinit(void)
{
    sigset_t sigs;
    int tmdty_pipe_in[2], tmdty_pipe_out[2];
    const char *tmdty_capt = "-Or -o -";
    const char *tmdty_opt_hc = "-ir 0";
    char tmdty_sound_spec[200];
    char *tmdty_cmd;
#define T_MAX_ARGS 255
    char *tmdty_args[T_MAX_ARGS];
    char *ptr;
    int i, ret;

    /* the socketpair is used as a bidirectional pipe: older versions
       (current as of 2008 :( ) of timidity write to stdin and we can
       catch that to avoid waiting 3 seconds at select at DOSEMU startup */
    if (pipe(tmdty_pipe_in) == -1 ||
	socketpair(AF_LOCAL, SOCK_STREAM, 0, tmdty_pipe_out) == -1) {
	perror("pipe() or socketpair()");
	goto err_ds;
    }
    switch ((tmdty_pid = fork())) {
    case 0:
	setsid();
	close(tmdty_pipe_in[0]);
	close(tmdty_pipe_out[1]);
	dup2(tmdty_pipe_out[0], STDIN_FILENO);
	dup2(tmdty_pipe_in[1], STDOUT_FILENO);
	close(tmdty_pipe_out[0]);
	close(tmdty_pipe_in[1]);
#if 1
	/* redirect stderr to /dev/null */
	close(STDERR_FILENO);
	open("/dev/null", O_WRONLY);
#endif
	strcpy(tmdty_sound_spec, tmdty_opt_hc);
	if (TMDTY_FREQ) {
	    char opt_buf[100];
	    sprintf(opt_buf, " --sampling-freq=%d", TMDTY_FREQ);
	    strcat(tmdty_sound_spec, opt_buf);
	}
	if (TMDTY_8BIT)
	    strcat(tmdty_sound_spec, " --output-8bit");
	if (TMDTY_UNS)
	    strcat(tmdty_sound_spec, " --output-unsigned");
	if (TMDTY_CAPT) {
	    strcat(tmdty_sound_spec, " ");
	    strcat(tmdty_sound_spec, tmdty_capt);
	}
	ret = asprintf(&tmdty_cmd, "%s %s %s",
		 TMDTY_BIN, TMDTY_ARGS, tmdty_sound_spec);
	assert(ret != -1);
	ptr = tmdty_cmd;
	for (i = 0; i < T_MAX_ARGS; i++) {
	    do
		tmdty_args[i] = strsep(&ptr, " ");
	    while (tmdty_args[i] && !tmdty_args[i][0]);
	    if (!tmdty_args[i])
		break;
	}

	/* unblock SIGIO and SIGALRM as timidity may use it */
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGIO);
	sigaddset(&sigs, SIGALRM);
	signal(SIGIO, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	sigprocmask(SIG_UNBLOCK, &sigs, NULL);

	execvp(TMDTY_BIN, tmdty_args);
	free(tmdty_cmd);
	_exit(1);
	break;
    case -1:
	perror("fork()");
	close(tmdty_pipe_out[0]);
	close(tmdty_pipe_in[1]);
	close(tmdty_pipe_in[0]);
	close(tmdty_pipe_out[1]);
	goto err_ds;
    }
    close(tmdty_pipe_out[0]);
    close(tmdty_pipe_in[1]);
    ctrl_sock_in = tmdty_pipe_in[0];
    ctrl_sock_out = tmdty_pipe_out[1];
    /* no handler, default handler does waitpid() */
    sigchld_register_handler(tmdty_pid, NULL);

    return TRUE;

  err_ds:
    return FALSE;
}

static int midotmdty_check_ready(char *buf, int size, int verb)
{
    fd_set rfds;
    struct timeval tv;
    int selret, n, ctrl_sock_max;

    FD_ZERO(&rfds);
    FD_SET(ctrl_sock_in, &rfds);
    FD_SET(ctrl_sock_out, &rfds);
    ctrl_sock_max = max(ctrl_sock_in, ctrl_sock_out);
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    while ((selret = select(ctrl_sock_max + 1, &rfds, NULL, NULL, &tv)) > 0) {
	if (!FD_ISSET(ctrl_sock_in, &rfds))
	    return FALSE;
	n = read(ctrl_sock_in, buf, size - 1);
	buf[n] = 0;
	if (!n)
	    break;
	if (verb)
	    S_printf("\tInit: %s", buf);
	if (strstr(buf, "220 TiMidity++"))
	    return TRUE;
	FD_ZERO(&rfds);
	FD_SET(ctrl_sock_in, &rfds);
	tv.tv_sec = 1;
	tv.tv_usec = 0;
    }
    if (selret < 0)
	perror("select()");
    return FALSE;
}

static int midotmdty_detect(void)
{
    char buf[255];
    int status, ret;

    if (!midotmdty_preinit())
	return FALSE;

    ret = midotmdty_check_ready(buf, sizeof(buf), 0);

    if (ret) {
	const char *ver_str = "Server Version ";
	char *ptr = strstr(buf, ver_str);
	if (!ptr) {
	    ret = FALSE;
	} else {
	    int vmin, vmid, vmaj, ver;
	    ptr += strlen(ver_str);
	    sscanf(ptr, "%d.%d.%d", &vmaj, &vmid, &vmin);
	    ver = vmaj * 10000 + vmid * 100 + vmin;
	    if (ver < 10004)
		ret = FALSE;
	}
	if (!ret)
	    S_printf
		("[ Note: TiMidity++ found but is too old, please update. ]\n");
    }

    if (!ret) {
	sigchld_enable_handler(tmdty_pid, 0);
	close(ctrl_sock_out);
	waitpid(tmdty_pid, &status, 0);
    }

    return ret;
}

static int midotmdty_init(void *arg)
{
    const char *cmd1 = "OPEN %s\n";
    char buf[255];
    char *pbuf;
    int n, i, data_port, err;
    struct addrinfo hints, *addrinfo, *ai;

    if (!midotmdty_detect())
	return FALSE;

    i = 1;
    if (*(char *) &i == 1)
	sprintf(buf, cmd1, "lsb");
    else
	sprintf(buf, cmd1, "msb");
    S_printf("\t%s", buf);
    write(ctrl_sock_out, buf, strlen(buf));
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    S_printf("\tOpen: %s\n", buf);
    pbuf = strstr(buf, " is ready");
    if (!pbuf)
	return FALSE;
    *pbuf = 0;
    pbuf = strrchr(buf, ' ');
    if (!pbuf)
	return FALSE;
    pbuf++;
    data_port = atoi(pbuf);
    if (!data_port) {
	error("Can't determine the data port number!\n");
	close(ctrl_sock_out);
	return FALSE;
    }
    S_printf("\tUsing port %d for data\n", data_port);
    if ((data_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
	close(ctrl_sock_out);
	return FALSE;
    }
    i = 1;
    setsockopt(data_sock, SOL_TCP, TCP_NODELAY, &i, sizeof(i));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;
    err = getaddrinfo(TMDTY_HOST, pbuf, &hints, &addrinfo);
    if (err) {
	error("Can't determine the data host addr!\n");
	close(data_sock);
	close(ctrl_sock_out);
	return FALSE;
    }
    for (ai = addrinfo; ai; ai = ai->ai_next) {
	err = connect(data_sock, ai->ai_addr, ai->ai_addrlen);
	if (!err)
	    break;
    }
    freeaddrinfo(addrinfo);
    if (err) {
	error("Can't open data connection!\n");
	close(data_sock);
	close(ctrl_sock_out);
	return FALSE;
    }
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    S_printf("\tConnect: %s\n", buf);

    if (TMDTY_CAPT) {
	add_to_io_select(data_sock, midotmdty_io, NULL);
	pcm_stream = pcm_allocate_stream(TMDTY_CHANS, "MIDI", 0);
    }

    midotmdty_reset();

    return TRUE;
}

static void midotmdty_done(void *arg)
{
    const char *cmd1 = "CLOSE\n";
    const char *cmd2 = "QUIT\n";
    char buf[255];
    int n, status;

    if (TMDTY_CAPT)
	remove_from_io_select(data_sock);

    write(ctrl_sock_out, cmd1, strlen(cmd1));
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    S_printf("\tClose: %s\n", buf);
    if (!strstr(buf, "already closed")) {
	shutdown(data_sock, 2);
	close(data_sock);
    }
    sigchld_enable_handler(tmdty_pid, 0);
    write(ctrl_sock_out, cmd2, strlen(cmd2));
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    S_printf("\tQuit: %s\n", buf);

    close(ctrl_sock_out);
    waitpid(tmdty_pid, &status, 0);
}

static void midotmdty_reset(void)
{
    const char *cmd1 = "RESET\n";
    const char *cmd2 = "PROTOCOL midi\n";
    char buf[255];
    int n;

    write(ctrl_sock_out, cmd1, strlen(cmd1));
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    S_printf("\tReset: %s\n", buf);

    write(ctrl_sock_out, cmd2, strlen(cmd2));
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    S_printf("\tSetup: %s\n", buf);
}

static void midotmdty_write(Bit8u val)
{
    send(data_sock, &val, 1, MSG_DONTWAIT);
}

static void midotmdty_stop(void *arg)
{
    S_printf("\tStop\n");
    midotmdty_write(0xfc);
    if (pcm_running)
	pcm_flush(pcm_stream);
    pcm_running = 0;
}

static int midotmdty_cfg(void *arg)
{
    return pcm_parse_cfg(config.midi_driver, midotmdty_name);
}

static const struct midi_out_plugin midotmdty = {
    .name = midotmdty_name,
    .longname = midotmdty_longname,
    .open = midotmdty_init,
    .close = midotmdty_done,
    .write = midotmdty_write,
    .stop = midotmdty_stop,
    .get_cfg = midotmdty_cfg,
    .stype = ST_GM,
    .weight = MIDI_W_PCM,
};

CONSTRUCTOR(static void midotmdty_register(void))
{
    midi_register_output_plugin(&midotmdty);
}
