/*
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/***********************************************************************
  timidity server driver
 ***********************************************************************/

#include "device.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
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

static int ctrl_sock, data_sock;
static struct sockaddr_in ctrl_adr, data_adr;
static struct hostent * serv;
static long long start_time;
static int timebase = 100;

SEQ_USE_EXTBUF();

void timid_start_timer()
{
static char cmd[] = "TIMEBASE\n";
char buf[255];
struct timeval time;
int n;
  send(ctrl_sock, cmd, strlen(cmd), 0);
  n = recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
  buf[n] = 0;
  sscanf(buf, "200 %i", &timebase);
  gettimeofday(&time, NULL);
  start_time = TIME2TICK(time);
  fprintf(stderr, "\tTimer: timebase = %i HZ, start = %lli\n", timebase, start_time);
  SEQ_START_TIMER();
  SEQ_DUMPBUF();
}

void timid_stop_timer()
{
  SEQ_STOP_TIMER();
  SEQ_DUMPBUF();
}

long timid_get_ticks()
{
struct timeval time;
  gettimeofday(&time, NULL);
  return (TIME2TICK(time) - start_time);
}

void timid_timestamp()
{
  SEQ_WAIT_TIME(timid_get_ticks());
}

void timid_sync_timidity()
{
char buf[255];
int n;
#define SEQ_META 0x7f
#define SEQ_SYNC 0x02
  _CHN_COMMON(0, SEQ_EXTENDED, SEQ_META, SEQ_SYNC, 0, 0);
  SEQ_DUMPBUF();
  n = recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
  buf[n] = 0;
  fprintf(stderr, "\tSync: %s\n", buf);
}

static int timid_preinit(void)
{
  serv = gethostbyname(config.timid_host);
  if (! serv)
    return FALSE;
  if ((ctrl_sock = socket(PF_INET, SOCK_STREAM, 0))==-1)
    return FALSE;
  if ((data_sock = socket(PF_INET, SOCK_STREAM, 0))==-1) {
    close(ctrl_sock);
    return FALSE;
  }
  
  ctrl_adr.sin_family = AF_INET;
  ctrl_adr.sin_port = htons(config.timid_port);
  memcpy(&ctrl_adr.sin_addr.s_addr, serv->h_addr, sizeof(ctrl_adr.sin_addr.s_addr));

  data_adr.sin_family = AF_INET;
  data_adr.sin_addr.s_addr = ctrl_adr.sin_addr.s_addr;

  if (connect(ctrl_sock, (struct sockaddr *)&ctrl_adr, sizeof(ctrl_adr)) != 0) {
    close(ctrl_sock);
    close(data_sock);
    return FALSE;
  }

  return TRUE;
}

bool timid_detect(void)
{
char buf[255];
int n, selret = 0, ret = FALSE;
fd_set rfds;
struct timeval tv;

  if (!timid_preinit())
    return FALSE;

  FD_ZERO(&rfds);
  FD_SET(ctrl_sock, &rfds);
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  while ((selret = select(ctrl_sock + 1, &rfds, NULL, NULL, &tv)) > 0) {
    n = recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
    buf[n] = 0;
    if (!n) {
      break;
    }
    if (strstr(buf, "220 TiMidity++")) {
      ret = TRUE;
      break;
    }
    FD_ZERO(&rfds);
    FD_SET(ctrl_sock, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }
  if (selret < 0)
    perror("select()");

  shutdown(ctrl_sock, 2);
  close(ctrl_sock);
  close(data_sock);
  return ret;
}

bool timid_init(void)
{
static char cmd1[] = "CLOSE\n";
static char cmd2[] = "SETBUF %1.2f %1.2f\n";
static char cmd3[] = "OPEN %s\n";
char buf[255];
char * pbuf;
int n, i, data_port, ret = FALSE, selret = 0;
fd_set rfds;
struct timeval tv;

  if (!timid_preinit())
    return FALSE;

  FD_ZERO(&rfds);
  FD_SET(ctrl_sock, &rfds);
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  while ((selret = select(ctrl_sock + 1, &rfds, NULL, NULL, &tv)) > 0) {
    n = recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
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
    FD_SET(ctrl_sock, &rfds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }
  fprintf(stderr, "\n");
  if (selret < 0)
    perror("select()");
  if (!ret)
    return FALSE;

  send(ctrl_sock, cmd1, strlen(cmd1), 0);
  recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
  sprintf(buf, cmd2, BUF_LOW_SYNC, BUF_HIGH_SYNC);
  send(ctrl_sock, buf, strlen(buf), 0);
  recv(ctrl_sock, buf, sizeof(buf) - 1, 0);

  i = 1;
  if(*(char *)&i == 1)
    sprintf(buf, cmd3, "lsb");
  else
    sprintf(buf, cmd3, "msb");
  fprintf(stderr, "\t%s", buf);
  send(ctrl_sock, buf, strlen(buf), 0);
  n = recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
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
    shutdown(ctrl_sock, 2);
    close(ctrl_sock);
    close(data_sock);
    return FALSE;
  }
  fprintf(stderr, "\tUsing port %d for data\n", data_port);
  setsockopt(data_sock, SOL_TCP, TCP_NODELAY, &i, sizeof(i));
  data_adr.sin_port = htons(data_port);
  if (connect(data_sock, (struct sockaddr *)&data_adr, sizeof(data_adr)) != 0) {
    error("Can't open data connection!\n");
    shutdown(ctrl_sock, 2);
    close(ctrl_sock);
    close(data_sock);
    return FALSE;
  }
  n = recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
  buf[n] = 0;
  fprintf(stderr, "\tConnect: %s\n", buf);
  timid_start_timer();
  return TRUE;
}

void timid_done(void)
{
static char cmd1[] = "CLOSE\n";
static char cmd2[] = "QUIT\n";
char buf[255];
int n;
  timid_sync_timidity();
  timid_stop_timer();
  send(ctrl_sock, cmd1, strlen(cmd1), 0);
  n = recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
  buf[n] = 0;
  fprintf(stderr, "\tClose: %s\n", buf);
  if (! strstr(buf, "already closed")) {
    shutdown(data_sock, 2);
    close(data_sock);
  }
  send(ctrl_sock, cmd2, strlen(cmd2), 0);
  n = recv(ctrl_sock, buf, sizeof(buf) - 1, 0);
  buf[n] = 0;
  fprintf(stderr, "\tQuit: %s\n", buf);

  shutdown(ctrl_sock, 2);
  close(ctrl_sock);
}

void timid_noteon(int chn, int note, int vol)
{
  timid_timestamp();
  SEQ_START_NOTE(0, chn, note, vol);
}

void timid_noteoff(int chn, int note, int vol)
{
  timid_timestamp();
  SEQ_STOP_NOTE(0, chn, note, vol);
}

void timid_control(int chn, int control, int value)
{
  timid_timestamp();
  SEQ_CONTROL(0, chn, control, value);
}

void timid_notepressure(int chn, int note, int pressure)
{
  timid_timestamp();
  SEQ_KEY_PRESSURE(0, chn, note, pressure);
}

void timid_channelpressure(int chn, int pressure)
{
  timid_timestamp();
  SEQ_CHN_PRESSURE(0, chn, pressure);
}

void timid_bender(int chn, int pitch)
{
  timid_timestamp();
  SEQ_BENDER(0, chn, pitch);
}

void timid_program(int chn, int pgm)
{
  timid_timestamp();
  SEQ_PGM_CHANGE(0, chn, pgm);
}

void timid_sysex(char buf[], int len)
{
  int i;
  timid_timestamp();
  for (i = 0; i < len; i += 6)
    SEQ_SYSEX(0, buf + i, MIN(len - i, 6));
}

void timid_flush(void)
{
  SEQ_DUMPBUF();
}

void timid_seqbuf_dump(void)
{
  if (_seqbufptr)
    send(data_sock, _seqbuf, _seqbufptr, 0);
  _seqbufptr = 0;
}

bool timid_setmode(Emumode new_mode)
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
