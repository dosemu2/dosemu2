/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/***********************************************************************
  timidity server driver
 ***********************************************************************/

#define _GNU_SOURCE
#include "device.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define seqbuf_dump timid_seqbuf_dump
#define _seqbuf timid_seqbuf
#define _seqbufptr timid_seqbufptr
#define _seqbuflen timid_seqbuflen
#include <sys/soundcard.h>
#include "seqops.h"

static int ctrl_sock_in, ctrl_sock_out, data_sock;
static pid_t tmdty_pid = -1;
static struct sockaddr_in ctrl_adr, data_adr;

#define SEQ_META 0x7f
#define SEQ_END_OF_MIDI 0x2f
#define SEQ_SYNC 0x02

void timid_seqbuf_dump(void);

static void timid_sync_timidity(void)
{
  char buf[255];
  int n;
  _CHN_COMMON(0, SEQ_EXTENDED, SEQ_META, SEQ_SYNC, 0, 0);
  SEQ_DUMPBUF();
  n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
  buf[n] = 0;
  fprintf(stderr, "\tSync: %s\n", buf);
}

static void timid_io(int signum)
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
    n = read(data_sock, buf, sizeof(buf));
    if (n > 0) {
//      fprintf(stderr, "Received %i data bytes\n", n);
      write(STDOUT_FILENO, buf, n);
    } else {
      /* no EINTR in a sighandler */
      break;
    }
    FD_ZERO(&rfds);
    FD_SET(data_sock, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
  }
}

static int timid_preinit(void)
{
  struct hostent * serv;
  serv = gethostbyname(config.timid_host);
  if (! serv)
    return FALSE;
  if ((data_sock = socket(PF_INET, SOCK_STREAM, 0))==-1)
    return FALSE;
  data_adr.sin_family = AF_INET;
  memcpy(&ctrl_adr.sin_addr.s_addr, serv->h_addr, sizeof(ctrl_adr.sin_addr.s_addr));
  data_adr.sin_addr.s_addr = ctrl_adr.sin_addr.s_addr;

  if (config.timid_port) {
    int ctrl_sock;

    if ((ctrl_sock = socket(PF_INET, SOCK_STREAM, 0))==-1)
      goto err_ds;

    ctrl_adr.sin_family = AF_INET;
    ctrl_adr.sin_port = htons(config.timid_port);

    if (connect(ctrl_sock, (struct sockaddr *)&ctrl_adr, sizeof(ctrl_adr)) != 0) {
      close(ctrl_sock);
      goto err_ds;
    }

    ctrl_sock_in = ctrl_sock_out = ctrl_sock;
  } else {
    int tmdty_pipe_in[2], tmdty_pipe_out[2];
    const char *tmdty_capt = "-Or -o -";
    const char *tmdty_opt_hc = "-ir 0";
    char tmdty_sound_spec[200];
    char *tmdty_cmd;
#define T_MAX_ARGS 255
    char *tmdty_args[T_MAX_ARGS];
    char *ptr;
    int i;
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
	if (config.timid_freq) {
	  char opt_buf[100];
	  sprintf(opt_buf, " --sampling-freq=%d", config.timid_freq);
	  strcat(tmdty_sound_spec, opt_buf);
	}
	if (config.timid_mono)
	  strcat(tmdty_sound_spec, " --output-mono");
	if (config.timid_8bit)
	  strcat(tmdty_sound_spec, " --output-8bit");
	if (config.timid_uns)
	  strcat(tmdty_sound_spec, " --output-unsigned");
	if (config.timid_capture) {
	  strcat(tmdty_sound_spec, " ");
	  strcat(tmdty_sound_spec, tmdty_capt);
	}
	asprintf(&tmdty_cmd, "%s %s %s",
	    config.timid_bin, config.timid_args, tmdty_sound_spec);
	ptr = tmdty_cmd;
	for (i = 0; i < T_MAX_ARGS; i++) {
	  do tmdty_args[i] = strsep(&ptr, " ");
	  while (tmdty_args[i] && !tmdty_args[i][0]);
	  if (!tmdty_args[i]) break;
	}
	execvp(config.timid_bin, tmdty_args);
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
  }

  return TRUE;

err_ds:
  close(data_sock);
  return FALSE;
}

static bool timid_check_ready(char *buf, int size, int verb)
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
      fprintf(stderr, "\tInit: %s", buf);
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

static bool timid_detect(void)
{
  char buf[255];
  int status, ret;

  if (!timid_preinit())
    return FALSE;

  ret = timid_check_ready(buf, sizeof(buf), 0);

  close(data_sock);
  close(ctrl_sock_out);
  if (tmdty_pid != -1) {
    waitpid(tmdty_pid, &status, 0);
    tmdty_pid = -1;
  }

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
      if (ver < 10002)
	ret = FALSE;
    }
    if (!ret)
      fprintf(stderr, "[ Note: TiMidity++ found but is too old, please update. ]\n");
  }
  return ret;
}

static bool timid_init(void)
{
  const char *cmd1 = "CLOSE\n";
  const char *cmd3 = "OPEN %s\n";
  char buf[255];
  char * pbuf;
  int n, i, data_port, ret;

  if (!timid_preinit())
    return FALSE;

  ret = timid_check_ready(buf, sizeof(buf), 1);
  if (!ret)
    fprintf(stderr, "\tInit failed!\n");
  fprintf(stderr, "\n");
  if (!ret)
    return FALSE;

  write(ctrl_sock_out, cmd1, strlen(cmd1));
  read(ctrl_sock_in, buf, sizeof(buf) - 1);

  i = 1;
  if(*(char *)&i == 1)
    sprintf(buf, cmd3, "lsb");
  else
    sprintf(buf, cmd3, "msb");
  fprintf(stderr, "\t%s", buf);
  write(ctrl_sock_out, buf, strlen(buf));
  n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
  buf[n] = 0;
  fprintf(stderr, "\tOpen: %s\n", buf);
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
  fprintf(stderr, "\tUsing port %d for data\n", data_port);
  i = 1;
  setsockopt(data_sock, SOL_TCP, TCP_NODELAY, &i, sizeof(i));
  data_adr.sin_port = htons(data_port);
  if (connect(data_sock, (struct sockaddr *)&data_adr, sizeof(data_adr)) != 0) {
    error("Can't open data connection!\n");
    close(data_sock);
    close(ctrl_sock_out);
    return FALSE;
  }
  if (config.timid_capture) {
    signal(SIGIO, timid_io);
    fcntl(data_sock, F_SETFL, fcntl(data_sock, F_GETFL) | O_ASYNC);
    fcntl(data_sock, F_SETOWN, getpid());
  }
  n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
  buf[n] = 0;
  fprintf(stderr, "\tConnect: %s\n", buf);
  return TRUE;
}

static void timid_done(void)
{
  const char *cmd1 = "CLOSE\n";
  const char *cmd2 = "QUIT\n";
  char buf[255];
  int n, status;

  timid_sync_timidity();
  write(ctrl_sock_out, cmd1, strlen(cmd1));
  n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
  buf[n] = 0;
  if (config.timid_capture) {
    fcntl(data_sock, F_SETFL, fcntl(data_sock, F_GETFL) & ~O_ASYNC);
    signal(SIGIO, SIG_DFL);
  }
  fprintf(stderr, "\tClose: %s\n", buf);
  if (! strstr(buf, "already closed")) {
    shutdown(data_sock, 2);
    close(data_sock);
  }
  write(ctrl_sock_out, cmd2, strlen(cmd2));
  n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
  buf[n] = 0;
  fprintf(stderr, "\tQuit: %s\n", buf);

  close(ctrl_sock_out);
  if (tmdty_pid != -1) {
    waitpid(tmdty_pid, &status, 0);
    tmdty_pid = -1;
  }
}

static void timid_pause(void)
{
  _CHN_COMMON(0, SEQ_EXTENDED, SEQ_END_OF_MIDI, 0, 0, 0);
  SEQ_DUMPBUF();
  fprintf(stderr, "\tPaused\n");
}

void seqbuf_dump(void)
{
  if (_seqbufptr)
    send(data_sock, _seqbuf, _seqbufptr, 0);
  _seqbufptr = 0;
}

static bool timid_setmode(Emumode new_mode)
{
  return (new_mode == EMUMODE_GM);
}

void register_timid(Device * dev)
{
	dev->name = "timidity client";
	dev->version = 10;
	dev->detect = timid_detect;
	dev->init = timid_init;
	dev->done = timid_done;
	dev->pause = timid_pause;
	dev->resume = NULL;
	dev->setmode = timid_setmode;
	USE_SEQ_OPS(dev);
}
