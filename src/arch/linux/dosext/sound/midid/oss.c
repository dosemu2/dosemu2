/*
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/***********************************************************************
  OSS sequencer driver
 ***********************************************************************/

#include "device.h"
#include <sys/types.h>     /* for open(2) */
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#define seqbuf_dump oss_seqbuf_dump
#include <sys/soundcard.h>

#define SEQUENCER_DEV   "/dev/sequencer"  /* Used device */

#define TIME2TICK(t) ((long long)t.tv_sec * timebase + \
    (((long long)t.tv_usec * timebase) / 1000000))

static long long start_time;
static int timebase = 100, timer_started = 0;
static int seqfd;  /* Sequencer file handle */

void oss_seqbuf_dump(void);

SEQ_USE_EXTBUF();

static void oss_start_timer(void)
{
  struct timeval time;
  gettimeofday(&time, NULL);
  start_time = TIME2TICK(time);
  SEQ_START_TIMER();
  SEQ_DUMPBUF();
  timer_started = 1;
}

static void oss_stop_timer(void)
{
  SEQ_STOP_TIMER();
  SEQ_DUMPBUF();
  timer_started = 0;
}

static long oss_get_ticks(void)
{
  struct timeval time;
  gettimeofday(&time, NULL);
  return (TIME2TICK(time) - start_time);
}

static void oss_timestamp(void)
{
  if (!timer_started)
    oss_start_timer();
  SEQ_WAIT_TIME(oss_get_ticks());
}

static bool oss_detect(void)
{
  if ((seqfd = open(SEQUENCER_DEV, O_WRONLY)) == -1)
    return 0;
  close(seqfd);
  return 1;
}

static bool oss_init(void)
{
  if ((seqfd = open(SEQUENCER_DEV, O_WRONLY)) == -1)
    return 0;
  return 1;
}

static void oss_done(void)
{
  oss_stop_timer();
}

static void oss_noteon(int chn, int note, int vol)
{
  oss_timestamp();
  SEQ_START_NOTE(0, chn, note, vol);
}

static void oss_noteoff(int chn, int note, int vol)
{
  oss_timestamp();
  SEQ_STOP_NOTE(0, chn, note, vol);
}

static void oss_control(int chn, int control, int value)
{
  oss_timestamp();
  SEQ_CONTROL(0, chn, control, value);
}

static void oss_notepressure(int chn, int note, int pressure)
{
  oss_timestamp();
  SEQ_KEY_PRESSURE(0, chn, note, pressure);
}

static void oss_channelpressure(int chn, int pressure)
{
  oss_timestamp();
  SEQ_CHN_PRESSURE(0, chn, pressure);
}

static void oss_bender(int chn, int pitch)
{
  oss_timestamp();
  SEQ_BENDER(0, chn, pitch);
}

static void oss_program(int chn, int pgm)
{
  oss_timestamp();
  SEQ_PGM_CHANGE(0, chn, pgm);
}

static void oss_sysex(unsigned char *buf, int len)
{
  int i;
  oss_timestamp();
  for (i = 0; i < len; i += 6)
    SEQ_SYSEX(0, buf + i, MIN(len - i, 6));
}

static void oss_flush(void)
{
  SEQ_DUMPBUF();
}

void oss_seqbuf_dump(void)
{
  if (_seqbufptr) {
    if (write(seqfd, _seqbuf, _seqbufptr) == -1) {
      perror("write " SEQUENCER_DEV);
      exit(-1);
    }
    _seqbufptr = 0;
  }
}

static bool oss_setmode(Emumode new_mode)
{
  if (new_mode == EMUMODE_GM)
    return TRUE;
  return FALSE;
}

void register_oss(Device * dev)
{
	dev->name = "OSS Sequencer";
	dev->version = 20;
	dev->detect = oss_detect;
	dev->init = oss_init;
	dev->done = oss_done;
	dev->setmode = oss_setmode;
	dev->flush = oss_flush;
	dev->noteon = oss_noteon;
	dev->noteoff = oss_noteoff;
	dev->control = oss_control;
	dev->notepressure = oss_notepressure;
	dev->channelpressure = oss_channelpressure;
	dev->bender = oss_bender;
	dev->program = oss_program;
	dev->sysex = oss_sysex;
}
