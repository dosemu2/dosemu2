#ifndef SEQOPS_H__
#define SEQOPS_H__

SEQ_DEFINEBUF(128);

static void seq_noteon(int chn, int note, int vol)
{
  SEQ_START_NOTE(0, chn, note, vol);
}

static void seq_noteoff(int chn, int note, int vol)
{
  SEQ_STOP_NOTE(0, chn, note, vol);
}

static void seq_control(int chn, int control, int value)
{
  SEQ_CONTROL(0, chn, control, value);
}

static void seq_notepressure(int chn, int note, int pressure)
{
  SEQ_KEY_PRESSURE(0, chn, note, pressure);
}

static void seq_channelpressure(int chn, int pressure)
{
  SEQ_CHN_PRESSURE(0, chn, pressure);
}

static void seq_bender(int chn, int pitch)
{
  SEQ_BENDER(0, chn, pitch);
}

static void seq_program(int chn, int pgm)
{
  SEQ_PGM_CHANGE(0, chn, pgm);
}

static void seq_sysex(unsigned char *buf, int len)
{
  int i;
  for (i = 0; i < len; i += 6)
    SEQ_SYSEX(0, buf + i, MIN(len - i, 6));
}

static void seq_flush(void)
{
  SEQ_DUMPBUF();
}

#define USE_SEQ_OPS(d) \
	d->flush = seq_flush; \
	d->noteon = seq_noteon; \
	d->noteoff = seq_noteoff; \
	d->control = seq_control; \
	d->notepressure = seq_notepressure; \
	d->channelpressure = seq_channelpressure; \
	d->bender = seq_bender; \
	d->program = seq_program; \
	d->sysex = seq_sysex

#endif
