/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/***********************************************************************
  null (flush) device -- mainly for testing
 ***********************************************************************/

#include "device.h"
#include <stdlib.h>

static bool null_detect(void)
{
	return(TRUE);
}

static bool null_init(void)
{
	return(TRUE);
}

static void null_doall(void)
{
}

static void null_doall2(int a,int b)
{
}

static void null_doall3(int a,int b,int c)
{
}

static bool null_setmode(Emumode new_mode)
{
  /* Our NULL driver has all emulations in the world :) */
  return TRUE;
}

void register_null(Device * dev)
{
	dev->name = "null";
	dev->version = 100;
	dev->detect = null_detect;
	dev->init = null_init;
	dev->done = null_doall;
	dev->pause = NULL;
	dev->resume = NULL;
	dev->flush = null_doall;
	dev->noteon = null_doall3;
	dev->noteoff = null_doall3;
	dev->control = null_doall3;
	dev->notepressure = null_doall3;
	dev->channelpressure = null_doall2;
	dev->bender = null_doall2;
	dev->program = null_doall2;
	dev->setmode = null_setmode;
}
