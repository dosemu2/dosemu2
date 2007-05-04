/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "device.h"
#include "midid.h"
#include "io.h"
#include "emulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

Device *devices = NULL;		/* list of all drivers */

/***********************************************************************
  Devices handling
 ***********************************************************************/

void device_add(void (*register_func) (Device * dev))
/* Add device with register function dev to beginning of device list */
{
  Device *newdev;
  newdev = calloc(1, sizeof(Device));
  assert(newdev);
  /* Setting up some default values */
  newdev->name = "(noname)";
  newdev->version = 0;
  /* Fill in rest of the values */
  register_func(newdev);
/*  if (comments && newdev->detected)
    fprintf(stderr, "%s device detected.\n", newdev->name);*/
  newdev->next = devices;
  devices = newdev;
}

void device_register_all(void)
{
  device_add(register_null);
  device_add(register_midout);
  device_add(register_timid);
  device_add(register_oss);
}

Device *dev_find_first_detected(void)
/* Return the first detected device of the list;
   NULL if none found */
{
  /* Select first device */
  Device *dev = devices;
  while (dev) {
    if (dev->detected)
      return (dev);
    dev = dev->next;
  }
  return NULL;
}

Device *dev_find_first_active(void)
/* Return the first active device of the list;
   NULL if none found */
{
  /* Select first device */
  Device *dev = devices;
  while (dev) {
    if (dev->active)
      return (dev);
    dev = dev->next;
  }
  return NULL;
}

void device_printall(void)
/* Prints a list of available drivers */
{
  int i = 1;
  Device *dev = devices;
  while (dev) {
    printf("%i. %s (v%i.%02i) ", i, dev->name, dev->version / 100,
	   dev->version % 100);
    if (dev->detected)
      printf("[detected]");
    printf("\n");
    i++;
    dev = dev->next;
  }
}

void device_printactive(void)
/* Prints a list of active drivers */
{
  Device *dev = devices;
  while (dev) {
    if (dev->active) {
      fprintf(stderr, "Using %s (v%i.%02i) device\n", dev->name,
    	    dev->version / 100, dev->version % 100);
    }
    dev = dev->next;
  }
}

void device_detect_all(void)
/* Detects available drivers */
{
  int for_each = 1;
  Device *dev = devices;

  if (dev_find_first_active())
    for_each = 0;

  while (dev) {
    if (for_each || dev->active) {
      dev->detected = dev->detect();
      if (dev->active && !dev->detected) {
        fprintf(stderr, "Warning: detection failed for \"%s\"\n", dev->name);
	dev->active = 0;
      }
    }
    dev = dev->next;
  }
}

int device_init_all(void)
/* Initialises available drivers */
{
  Device *dev = devices;
  int num = 0;

  while (dev) {
    if (dev->active) {
      fprintf(stderr, "Initialising %s...\n", dev->name);
      dev->ready = dev->init();
      if (! dev->ready) {
        fprintf(stderr, "Warning: Init failed for \"%s\"\n", dev->name);
      } else {
        emulation_set(dev, config.mode);
        num++;
      }
      fprintf(stderr, "\n");
    }
    dev = dev->next;
  }
  return num;
}

void device_stop_all(void)
/* De-Initialises available drivers */
{
  Device *dev = devices;
  int num = 0;

  while (dev) {
    if (dev->ready) {
      fprintf(stderr, "Stopping %s...\n", dev->name);
      dev->done();
      dev->ready = 0;
      num++;
    }
    dev = dev->next;
  }
  if (num)
    fprintf(stderr, "Stopped %i devices\n\n", num);
}

void device_pause_all(void)
{
  Device *dev = devices;
  int num = 0;

  while (dev) {
    if (dev->ready && dev->pause) {
      fprintf(stderr, "Pausing %s...\n", dev->name);
      dev->pause();
      num++;
    }
    dev = dev->next;
  }
  if (num)
    fprintf(stderr, "Paused %i devices\n\n", num);
}

void device_resume_all(void)
{
  Device *dev = devices;
  int num = 0;

  while (dev) {
    if (dev->ready && dev->resume) {
      fprintf(stderr, "Resuming %s...\n", dev->name);
      dev->resume();
      num++;
    }
    dev = dev->next;
  }
  if (num)
    fprintf(stderr, "Resumed %i devices\n\n", num);
}

Device *device_activate(int number)
/* Select device number <number>; first one is 1.
   0 means first detected one */
{
  Device *dev = devices, *retdev = NULL;
  if (!number) {
    retdev = dev_find_first_active();
    if (! retdev) {
      retdev = dev_find_first_detected();
      if (retdev) {
        retdev->active = 1;
      }
    }
  }
  else {
    number--;
    while ((number--) && dev)
      dev = dev->next;
    retdev = dev;
    retdev->active = 1;
  }

  if (comments && number == 0) {
    device_printactive();
    fprintf(stderr, "\n");
  }
  return retdev;
}


bool for_each_dev(bool (*func)(Device *, va_list), ...)
{
  va_list args;
  int ret = 0;
  Device *dev = devices;

  while (dev) {
    if (dev->ready) {
      va_start(args, func);
      ret = func(dev, args);
      va_end(args);
      if (!ret) {
        fprintf(stderr, "Warning: %s failed the request\n", dev->name);
      }
    }
    dev = dev->next;
  }
  return ret;
}


bool run_flush(Device * dev, va_list args)
{				/* Flush all commands sent */
  if (dev->flush)
    dev->flush();
  return 1;
}

bool run_noteon(Device * dev, va_list args)
{				/* 0x90 */
  int chn, note, vol;
  chn = va_arg(args, int);
  note = va_arg(args, int);
  vol = va_arg(args, int);
  dev->noteon(chn, note, vol);
  return 1;
}

bool run_noteoff(Device * dev, va_list args)
{				/* 0x80 */
  int chn, note, vol;
  chn = va_arg(args, int);
  note = va_arg(args, int);
  vol = va_arg(args, int);
  dev->noteoff(chn, note, vol);
  return 1;
}

bool run_notepressure(Device * dev, va_list args)
{				/* 0xA0 */
  int chn, control, value;
  chn = va_arg(args, int);
  control = va_arg(args, int);
  value = va_arg(args, int);
  dev->notepressure(chn, control, value);
  return 1;
}

bool run_channelpressure(Device * dev, va_list args)
{				/* 0xD0 */
  int chn, vol;
  chn = va_arg(args, int);
  vol = va_arg(args, int);
  dev->channelpressure(chn, vol);
  return 1;
}

bool run_control(Device * dev, va_list args)
{				/* 0xB0 */
  int chn, control, value;
  chn = va_arg(args, int);
  control = va_arg(args, int);
  value = va_arg(args, int);
  dev->control(chn, control, value);
  return 1;
}

bool run_program(Device * dev, va_list args)
{				/* 0xC0 */
  int chn, pgm;
  chn = va_arg(args, int);
  pgm = va_arg(args, int);
  dev->program(chn, pgm);
  return 1;
}

bool run_bender(Device * dev, va_list args)
{				/* 0xE0 */
  int chn, pitch;
  chn = va_arg(args, int);
  pitch = va_arg(args, int);
  dev->bender(chn, pitch);
  return 1;
}

bool run_sysex(Device * dev, va_list args)
{				/* 0xF0 */
  unsigned char *buf;
  int len;
  buf = va_arg(args, unsigned char *);
  len = va_arg(args, int);
  if (dev->sysex)
    dev->sysex(buf, len);
  return 1;
}
