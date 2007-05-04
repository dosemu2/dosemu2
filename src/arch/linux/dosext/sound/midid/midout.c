/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/***********************************************************************
  .mid file writer
 ***********************************************************************/

#include "device.h"
#include <sys/time.h>	   /* for gettimeofday(2) */
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static struct timeval start_time;
static int time_base, last_timer_ticks;
static unsigned int track_num;
static FILE *fp;
static long track_size_pos;

static void midout_write_delta_time(void)
{
  struct timeval time;
  struct timezone tz;
  long elapsed_tv_sec, elapsed_tv_usec;
  int timer_ticks;
  unsigned int delta_time;
  unsigned char c[4];
  int idx;
  bool started_printing = FALSE;

/* Initialize the timer if this is the first call to this function */
  if (last_timer_ticks == -1) {
    if (gettimeofday(&start_time, &tz) != 0) {
      error("Could not get time of day\n");
    }
    last_timer_ticks = 0;
  }

  gettimeofday(&time, &tz);

  elapsed_tv_sec = time.tv_sec - start_time.tv_sec;
  elapsed_tv_usec = time.tv_usec - start_time.tv_usec;
  if (elapsed_tv_usec < 0) {
    elapsed_tv_usec += 1000000;
    elapsed_tv_sec -= 1;
  }

/* Calculate the number of ticks since the last message. */
  timer_ticks = elapsed_tv_sec * time_base + (time_base * elapsed_tv_usec) / 1000000;

  delta_time = (unsigned int) (timer_ticks - last_timer_ticks);
  last_timer_ticks = timer_ticks;

/* We have to divide the number of ticks into 7-bit segments, and only write
   the non-zero segments starting with the most significant (except for the
   least significant segment, which we always write).  The most significant bit
   is set to 1 in all but the least significant segment.
 */
  c[0] = (unsigned char)((delta_time >> 21) & 0x7f);
  c[1] = (unsigned char)((delta_time >> 14) & 0x7f);
  c[2] = (unsigned char)((delta_time >>  7) & 0x7f);
  c[3] = (unsigned char)((delta_time      ) & 0x7f);

  for (idx = 0; idx < 3; idx++) {
    if (started_printing || (c[idx] != '\0')) {
      started_printing = TRUE;
      fputc(c[idx] | '\x80', fp);
    }
  }
  fputc(c[3], fp);
}

static bool midout_detect(void)
{
  bool ret = FALSE;
  if (strcmp(config.midifile, "-") == 0)
    ret = TRUE;
  else {
    fp = fopen(config.midifile, "w");
    if (fp) {
      ret = TRUE;
      fclose(fp);
      unlink(config.midifile);
    }
  }
  return ret;
}

static void midout_start_track(void)
{
  unsigned char t0, t1, t2;
  int quarter_note_us;

/* Write out track header.
   The track will have a large length (0x7fffffff) because we don't know at
   this time how big it will really be.
 */
  fprintf(fp, "%s", "MTrk");
  track_size_pos = ftell(fp);
  fprintf(fp, "%c%c%c%c", '\x7f', '\xff', '\xff', '\xff'); /* #chunks */

/* Set the tempo (converting to microseconds per quarter note) */
  quarter_note_us = (int) (60.0 * 1000000.0 / config.tempo);
  t0 = (unsigned char)((quarter_note_us >> 16) & 0xff);
  t1 = (unsigned char)((quarter_note_us >>  8) & 0xff);
  t2 = (unsigned char)((quarter_note_us      ) & 0xff);
  fprintf(fp, "%c%c%c%c%c%c%c", '\0','\xff','\x51','\3', t0, t1, t2);

/* This got copied from bochs-1.3.  I don't know if it's really
   necessary.
 */

/* Set the time sig to 4/4 */
  fprintf(fp, "%c%c%c%c%c%c%c%c", '\0','\xff','\x58','\4','\4','\x2','\x18','\x08');
}

static void midout_finish_track(void)
{
/* Send (with delta-time of 0) "0xff 0x2f 0x0" to finish the track. */
  fprintf(fp, "%c%c%c%c", '\0', '\xff', '\x2f', '\0');
}

static bool midout_init(void)
{
  unsigned char t0, t1;

  if (strcmp(config.midifile, "-") == 0)
    fp = stdout;
  else
    fp = fopen(config.midifile, "w");
  if (!fp)
    return FALSE;
/* Set last_timer_ticks to -1 to force the timer to initialize on the first
   command.
 */
  last_timer_ticks = -1;

/* Initialize track number. */
  track_num = 0;

/* Calculate the time base from the tempo and tick rate */
  time_base = (config.tempo / 60.0) * config.ticks_per_quarter_note;

/* Write out MID file header.
   The file will have a single track, with the configured number of
    ticks per quarter note.
 */
  fprintf(fp, "%s", "MThd");
  fprintf(fp, "%c%c%c%c", '\0', '\0', '\0', '\006'); /* header size */
  fprintf(fp, "%c%c", '\0', '\0'); /* single track format */
  fprintf(fp, "%c%c", '\0', '\1'); /* #tracks = 1 */
  t0 = (unsigned char)((config.ticks_per_quarter_note >> 8) & 0xff);
  t1 = (unsigned char)((config.ticks_per_quarter_note     ) & 0xff);
  fprintf(fp, "%c%c", t0, t1); /* #ticks / quarter note */

  midout_start_track();

  return(TRUE);
}

static void midout_done(void)
{
  long file_size;
  int track_bytes;
  unsigned char s0, s1, s2, s3;

  midout_finish_track();

  file_size = ftell(fp);
  track_bytes = (int)(file_size - track_size_pos - 4);
  s0 = (unsigned char)((track_bytes >> 24) & 0xff);
  s1 = (unsigned char)((track_bytes >> 16) & 0xff);
  s2 = (unsigned char)((track_bytes >>  8) & 0xff);
  s3 = (unsigned char)((track_bytes      ) & 0xff);

  if (fseek(fp, track_size_pos, SEEK_SET) == 0)
    fprintf(fp, "%c%c%c%c", s0, s1, s2, s3);
  fclose(fp);
}

static void midout_flush(void)
{
  fflush(fp);
}

static void midout_noteon(int chn, int note, int vel)
{
  unsigned char cm, no, ve;

  midout_write_delta_time();

  cm = (unsigned char)((chn & 0x0f) | 0x90);
  no = (unsigned char)(note & 0x7f);
  ve = (unsigned char)(vel & 0x7f);

  fprintf(fp, "%c%c%c", cm, no, ve);
}

static void midout_noteoff(int chn, int note, int vel)
{
  unsigned char cm, no, ve;

  midout_write_delta_time();

  cm = (unsigned char)((chn & 0x0f) | 0x80);
  no = (unsigned char)(note & 0x7f);
  ve = (unsigned char)(vel & 0x7f);

  fprintf(fp, "%c%c%c", cm, no, ve);
}

static void midout_control(int chn, int control, int value)
{
  unsigned char cm, co, va;

  midout_write_delta_time();

  cm = (unsigned char)((chn & 0x0f) | 0xb0);
  co = (unsigned char)(control & 0x7f);
  va = (unsigned char)(value & 0x7f);

  fprintf(fp, "%c%c%c", cm, co, va);
}

static void midout_notepressure(int chn, int control, int value)
{
  unsigned char cm, co, va;

  midout_write_delta_time();

  cm = (unsigned char)((chn & 0x0f) | 0xa0);
  co = (unsigned char)(control & 0x7f);
  va = (unsigned char)(value & 0x7f);

  fprintf(fp, "%c%c%c", cm, co, va);
}

static void midout_channelpressure(int chn, int vel)
{
  unsigned char cm, ve;

  midout_write_delta_time();

  cm = (unsigned char)((chn & 0x0f) | 0xd0);
  ve = (unsigned char)(vel & 0x7f);

  fprintf(fp, "%c%c", cm, ve);
}

static void midout_bender(int chn, int pitch)
{
  unsigned char cm, pi0, pi1;

  midout_write_delta_time();

  cm = (unsigned char)((chn & 0x0f) | 0xe0);
  pi0 = (unsigned char)(pitch & 0x7f);
  pi1 = (unsigned char)((pitch >> 7) & 0x7f);

  fprintf(fp, "%c%c%c", cm, pi0, pi1);
}

static void midout_program(int chn, int pgm)
{
  unsigned char cm, pg;

  midout_write_delta_time();

  cm = (unsigned char)((chn & 0x0f) | 0xc0);
  pg = (unsigned char)(pgm & 0x7f);

  fprintf(fp, "%c%c", cm, pg);
}

static bool midout_setmode(Emumode new_mode)
{
  if (new_mode == EMUMODE_GM)
    return(TRUE);
  return(FALSE);
}

void register_midout(Device * dev)
{
	dev->name = "MID file";
	dev->version = 10;
	dev->detect = midout_detect;
	dev->init = midout_init;
	dev->done = midout_done;
	dev->pause = NULL;
	dev->resume = NULL;
	dev->flush = midout_flush;
	dev->noteon = midout_noteon;
	dev->noteoff = midout_noteoff;
	dev->control = midout_control;
	dev->notepressure = midout_notepressure;
	dev->channelpressure = midout_channelpressure;
	dev->bender = midout_bender;
	dev->program = midout_program;
	dev->setmode = midout_setmode;
}
