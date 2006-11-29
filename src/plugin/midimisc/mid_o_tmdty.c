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

#define TMDTY_HOST "localhost"
#define TMDTY_FREQ 44100
#define TMDTY_MONO 0
#define TMDTY_8BIT 0
#define TMDTY_UNS 0
#define TMDTY_CAPT 1
#define TMDTY_BIN "timidity"
#define TMDTY_ARGS "-EFreverb=0 -EFchorus=0 -EFresamp=1 -EFvlpf=0 -EFns=0"

static const char *midotmdty_name = "MIDI Output: midid plugin";

static int ctrl_sock_in, ctrl_sock_out, data_sock, pcm_stream;
static pid_t tmdty_pid = -1;
static struct sockaddr_in ctrl_adr, data_adr;

static void midotmdty_io(void)
{
    char buf[16384];
    int n, selret;
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(data_sock, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    while ((selret = select(data_sock + 1, &rfds, NULL, NULL, &tv)) > 0) {
	n = RPT_SYSCALL(read(data_sock, buf, sizeof(buf)));
	if (n > 0) {
	    pcm_write_samples(buf, n, 44100, PCM_FORMAT_S16_LE,
			      pcm_stream);
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
    struct hostent *serv;
    sigset_t sigs;
    int tmdty_pipe_in[2], tmdty_pipe_out[2];
    const char *tmdty_capt = "-Or -o -";
    const char *tmdty_opt_hc = "-ir 0";
    char tmdty_sound_spec[200];
    char *tmdty_cmd;
#define T_MAX_ARGS 255
    char *tmdty_args[T_MAX_ARGS];
    char *ptr;
    int i;

    serv = gethostbyname(TMDTY_HOST);
    if (!serv)
	return FALSE;
    if ((data_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	return FALSE;
    data_adr.sin_family = AF_INET;
    memcpy(&ctrl_adr.sin_addr.s_addr, serv->h_addr,
	   sizeof(ctrl_adr.sin_addr.s_addr));
    data_adr.sin_addr.s_addr = ctrl_adr.sin_addr.s_addr;

    if (pipe(tmdty_pipe_in) == -1 || pipe(tmdty_pipe_out) == -1) {
	perror("pipe()");
	goto err_ds;
    }
    switch ((tmdty_pid = fork())) {
    case 0:
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
	if (TMDTY_MONO)
	    strcat(tmdty_sound_spec, " --output-mono");
	if (TMDTY_8BIT)
	    strcat(tmdty_sound_spec, " --output-8bit");
	if (TMDTY_UNS)
	    strcat(tmdty_sound_spec, " --output-unsigned");
	if (TMDTY_CAPT) {
	    strcat(tmdty_sound_spec, " ");
	    strcat(tmdty_sound_spec, tmdty_capt);
	}
	asprintf(&tmdty_cmd, "%s %s %s",
		 TMDTY_BIN, TMDTY_ARGS, tmdty_sound_spec);
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
	exit(1);
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

    return TRUE;

  err_ds:
    close(data_sock);
    return FALSE;
}

static int midotmdty_check_ready(char *buf, int size, int verb)
{
    fd_set rfds;
    struct timeval tv;
    int selret, n;

    FD_ZERO(&rfds);
    FD_SET(ctrl_sock_in, &rfds);
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    while ((selret = select(ctrl_sock_in + 1, &rfds, NULL, NULL, &tv)) > 0) {
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
	int vmin, vmid, vmaj, ver;
	if (!ptr) {
	    ret = FALSE;
	} else {
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
	close(data_sock);
	close(ctrl_sock_out);
	if (tmdty_pid != -1) {
	    waitpid(tmdty_pid, &status, 0);
	    tmdty_pid = -1;
	}
    }

    return ret;
}

static int midotmdty_init(void)
{
    const char *cmd1 = "OPEN %s\n";
    char buf[255];
    char *pbuf;
    int n, i, data_port;

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
    data_port = atoi(pbuf + 1);
    if (!data_port) {
	error("Can't determine the data port number!\n");
	close(data_sock);
	close(ctrl_sock_out);
	return FALSE;
    }
    S_printf("\tUsing port %d for data\n", data_port);
    i = 1;
    setsockopt(data_sock, SOL_TCP, TCP_NODELAY, &i, sizeof(i));
    data_adr.sin_port = htons(data_port);
    if (connect(data_sock, (struct sockaddr *) &data_adr, sizeof(data_adr))
	!= 0) {
	error("Can't open data connection!\n");
	close(data_sock);
	close(ctrl_sock_out);
	return FALSE;
    }
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    S_printf("\tConnect: %s\n", buf);

    if (TMDTY_CAPT) {
	add_to_io_select(data_sock, 1, midotmdty_io);
	pcm_stream = pcm_allocate_stream(2, "MIDI");
    }

    return TRUE;
}

static void midotmdty_done(void)
{
    const char *cmd1 = "CLOSE\n";
    const char *cmd2 = "QUIT\n";
    char buf[255];
    int n, status;

    if (TMDTY_CAPT)
	remove_from_io_select(data_sock, 1);

    write(ctrl_sock_out, cmd1, strlen(cmd1));
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    S_printf("\tClose: %s\n", buf);
    if (!strstr(buf, "already closed")) {
	shutdown(data_sock, 2);
	close(data_sock);
    }
    write(ctrl_sock_out, cmd2, strlen(cmd2));
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    S_printf("\tQuit: %s\n", buf);

    close(ctrl_sock_out);
    if (tmdty_pid != -1) {
	waitpid(tmdty_pid, &status, 0);
	tmdty_pid = -1;
    }
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
    if (tmdty_pid == -1)
	return;
    send(data_sock, &val, 1, MSG_DONTWAIT);
}

static void midotmdty_stop(void)
{
    S_printf("\tStop\n");
    midotmdty_write(0xfc);
}

CONSTRUCTOR(static int midotmdty_register(void))
{
    struct midi_out_plugin midotmdty;
    midotmdty.name = midotmdty_name;
    midotmdty.init = midotmdty_init;
    midotmdty.done = midotmdty_done;
    midotmdty.reset = midotmdty_reset;
    midotmdty.write = midotmdty_write;
    midotmdty.stop = midotmdty_stop;
#if 1
    return midi_register_output_plugin(midotmdty);
#else
    return 0;
#endif
}
