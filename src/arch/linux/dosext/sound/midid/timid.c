/*
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
#include <sys/soundcard.h>

#define BUF_LOW_SYNC	0.1
#define BUF_HIGH_SYNC	0.15

#define TIME2TICK(t) ((long long)t.tv_sec * timebase + \
    (((long long)t.tv_usec * timebase) / 1000000))

static int ctrl_sock_in, ctrl_sock_out, data_sock;
static pid_t tmdty_pid = -1;
static struct sockaddr_in ctrl_adr, data_adr;
static long long start_time;
static int timebase = 100, timer_started = 0;

void timid_seqbuf_dump(void);

SEQ_USE_EXTBUF();

static void timid_init_timer(void)
{
  static const char *cmd = "TIMEBASE\n";
  char buf[255];
  int n;

  write(ctrl_sock_out, cmd, strlen(cmd));
  n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
  buf[n] = 0;
  sscanf(buf, "200 %i", &timebase);
  fprintf(stderr, "\tTimer: timebase = %i HZ\n", timebase);
}

static void timid_start_timer(void)
{
  struct timeval time;
  gettimeofday(&time, NULL);
  start_time = TIME2TICK(time);
  SEQ_START_TIMER();
  SEQ_DUMPBUF();
  timer_started = 1;
}

static void timid_stop_timer(void)
{
  SEQ_STOP_TIMER();
  SEQ_DUMPBUF();
  timer_started = 0;
}

static long timid_get_ticks(void)
{
  struct timeval time;
  gettimeofday(&time, NULL);
  return (TIME2TICK(time) - start_time);
}

static void timid_timestamp(void)
{
  if (!timer_started)
    timid_start_timer();
  SEQ_WAIT_TIME(timid_get_ticks());
}

static void timid_sync_timidity(void)
{
  char buf[255];
  int n;
#define SEQ_META 0x7f
#define SEQ_SYNC 0x02
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
      fprintf(stderr, "Received %i data bytes\n", n);
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
    if (pipe(tmdty_pipe_in) == -1) {
      perror("pipe()");
      goto err_ds;
    }
    if (pipe(tmdty_pipe_out) == -1) {
      perror("pipe()");
      goto err_ds;
    }
    switch ((tmdty_pid = fork())) {
      case 0:
	close(tmdty_pipe_in[0]);
	close(tmdty_pipe_out[1]);
	dup2(tmdty_pipe_out[0], STDIN_FILENO);
	dup2(tmdty_pipe_in[1], STDOUT_FILENO);
#if 0
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

static bool timid_detect(void)
{
  char buf[255];
  int n, status, selret = 0, ret = FALSE;
  fd_set rfds;
  struct timeval tv;

  if (!timid_preinit())
    return FALSE;

  FD_ZERO(&rfds);
  FD_SET(ctrl_sock_in, &rfds);
  tv.tv_sec = 3;
  tv.tv_usec = 0;
  while ((selret = select(ctrl_sock_in + 1, &rfds, NULL, NULL, &tv)) > 0) {
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    if (!n) {
      break;
    }
    if (strstr(buf, "220 TiMidity++")) {
      ret = TRUE;
      break;
    }
    FD_ZERO(&rfds);
    FD_SET(ctrl_sock_in, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }
  if (selret < 0)
    perror("select()");

  close(data_sock);
  close(ctrl_sock_out);
  if (tmdty_pid != -1) {
    waitpid(tmdty_pid, &status, 0);
    tmdty_pid = -1;
  }
  return ret;
}

static bool timid_init(void)
{
  static const char *cmd1 = "CLOSE\n";
  static const char *cmd2 = "SETBUF %1.2f %1.2f\n";
  static const char *cmd3 = "OPEN %s\n";
  char buf[255];
  char * pbuf;
  int n, i, data_port, ret = FALSE, selret = 0;
  fd_set rfds;
  struct timeval tv;

  if (!timid_preinit())
    return FALSE;

  FD_ZERO(&rfds);
  FD_SET(ctrl_sock_in, &rfds);
  tv.tv_sec = 3;
  tv.tv_usec = 0;
  while ((selret = select(ctrl_sock_in + 1, &rfds, NULL, NULL, &tv)) > 0) {
    n = read(ctrl_sock_in, buf, sizeof(buf) - 1);
    buf[n] = 0;
    if (n)
      fprintf(stderr, "\tInit: %s", buf);
    else {
      fprintf(stderr, "\tInit failed!\n");
      break;
    }
    if (strstr(buf, "220 TiMidity++")) {
      ret = TRUE;
      break;
    }
    FD_ZERO(&rfds);
    FD_SET(ctrl_sock_in, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }
  fprintf(stderr, "\n");
  if (selret < 0)
    perror("select()");
  if (!ret)
    return FALSE;

  write(ctrl_sock_out, cmd1, strlen(cmd1));
  read(ctrl_sock_in, buf, sizeof(buf) - 1);
  sprintf(buf, cmd2, BUF_LOW_SYNC, BUF_HIGH_SYNC);
  write(ctrl_sock_out, buf, strlen(buf));
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
  timid_init_timer();
  return TRUE;
}

static void timid_done(void)
{
  static const char *cmd1 = "CLOSE\n";
  static const char *cmd2 = "QUIT\n";
  char buf[255];
  int n, status;

  timid_sync_timidity();
  timid_stop_timer();
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

static void timid_noteon(int chn, int note, int vol)
{
  timid_timestamp();
  SEQ_START_NOTE(0, chn, note, vol);
}

static void timid_noteoff(int chn, int note, int vol)
{
  timid_timestamp();
  SEQ_STOP_NOTE(0, chn, note, vol);
}

static void timid_control(int chn, int control, int value)
{
  timid_timestamp();
  SEQ_CONTROL(0, chn, control, value);
}

static void timid_notepressure(int chn, int note, int pressure)
{
  timid_timestamp();
  SEQ_KEY_PRESSURE(0, chn, note, pressure);
}

static void timid_channelpressure(int chn, int pressure)
{
  timid_timestamp();
  SEQ_CHN_PRESSURE(0, chn, pressure);
}

static void timid_bender(int chn, int pitch)
{
  timid_timestamp();
  SEQ_BENDER(0, chn, pitch);
}

static void timid_program(int chn, int pgm)
{
  timid_timestamp();
  SEQ_PGM_CHANGE(0, chn, pgm);
}

static void timid_sysex(unsigned char *buf, int len)
{
  int i;
  timid_timestamp();
  for (i = 0; i < len; i += 6)
    SEQ_SYSEX(0, buf + i, MIN(len - i, 6));
}

static void timid_flush(void)
{
  SEQ_DUMPBUF();
}

void timid_seqbuf_dump(void)
{
  if (_seqbufptr)
    send(data_sock, _seqbuf, _seqbufptr, 0);
  _seqbufptr = 0;
}

static bool timid_setmode(Emumode new_mode)
{
  if (new_mode == EMUMODE_GM)
    return TRUE;
  return FALSE;
}

void register_timid(Device * dev)
{
	dev->name = "timidity client";
	dev->version = 10;
	dev->detect = timid_detect;
	dev->init = timid_init;
	dev->done = timid_done;
	dev->setmode = timid_setmode;
	dev->flush = timid_flush;
	dev->noteon = timid_noteon;
	dev->noteoff = timid_noteoff;
	dev->control = timid_control;
	dev->notepressure = timid_notepressure;
	dev->channelpressure = timid_channelpressure;
	dev->bender = timid_bender;
	dev->program = timid_program;
	dev->sysex = timid_sysex;
}
