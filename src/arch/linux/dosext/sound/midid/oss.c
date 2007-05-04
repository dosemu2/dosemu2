/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
#include <unistd.h>

#define seqbuf_dump oss_seqbuf_dump
#define _seqbuf oss_seqbuf
#define _seqbufptr oss_seqbufptr
#define _seqbuflen oss_seqbuflen
#include <sys/soundcard.h>
#include "seqops.h"

#define SEQUENCER_DEV   "/dev/sequencer"  /* Used device */

static int seqfd;  /* Sequencer file handle */

void oss_seqbuf_dump(void);

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
  close(seqfd);
  seqfd = -1;
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
	dev->pause = NULL;
	dev->resume = NULL;
	dev->setmode = oss_setmode;
	USE_SEQ_OPS(dev);
}
