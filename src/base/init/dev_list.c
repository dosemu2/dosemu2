/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Description:
 *  Manages a list of the available I/O devices.  It will automatically
 *  call their initialization and termination routines.
 *  The current I/O device list includes:
 * VERB
 *     Fully emulated:      pit, pic, cmos, serial
 *     Partially emulated:  rtc, keyb, lpt
 *     Unemulated:          dma, hdisk, floppy, pos
 * /VERB
 * /REMARK
 *
 * Maintainers: Scott Buchholz
 * 
 * DANG_END_MODULE
 *
 */

#include "config.h"

#include "emu.h"
#include "iodev.h"
#include "int.h"
#include "port.h"
#include "pic.h"
#include "serial.h"
#include "keyb_server.h"     /* for keyb_8042_{init,reset} */
#include "lpt.h"
#include "disks.h"

#ifdef USE_SBEMU
#include "sound.h"
#endif

struct io_dev_struct {
  const char * name;
  void (* init_func_t)(void);
  void (* reset_func_t)(void);
  void (* term_func_t)(void);
};

static struct io_dev_struct io_devices[] = {
  { "pit",     pit_init,     pit_reset,     NULL },
  { "cmos",    cmos_init,    cmos_reset,    NULL },
  { "serial",  serial_init,  NULL,          serial_close },
  { "pic",     pic_init,     pic_reset,     NULL },
  { "keyb",    keyb_8042_init, keyb_8042_reset, NULL },
#if 0
  { "pos",     pos_init,     pos_reset,     NULL },
  { "lpt",     lpt_init,     lpt_reset,     lpt_term },
#endif
  { "dma",     dma_init,     dma_reset,     NULL },   
#if 0
  { "floppy",  floppy_init,  floppy_reset,  NULL },
  { "hdisk",   hdisk_init,   hdisk_reset,   NULL },
  { "disks",   disk_init,    disk_reset,    disk_term },
#endif
#ifdef USE_SBEMU
  { "sound",   sound_init,   sound_reset,   NULL }, 
#endif
  { NULL,      NULL,         NULL,          NULL }
};


void iodev_init(void)        /* called at startup */
{
  struct io_dev_struct *ptr;

  for (ptr = io_devices; ptr->name; ptr++)
    if (ptr->init_func_t)
      ptr->init_func_t();

  for (ptr = io_devices; ptr->name; ptr++)
    if (ptr->reset_func_t)
      ptr->reset_func_t();
}

void iodev_reset(void)        /* called at reboot */
{
  struct io_dev_struct *ptr;

  for (ptr = io_devices; ptr->name; ptr++)
    if (ptr->reset_func_t)
      ptr->reset_func_t();
}

void iodev_term(void)
{
  struct io_dev_struct *ptr;

  for (ptr = io_devices; ptr->name; ptr++)
    if (ptr->term_func_t)
      ptr->term_func_t();
}
