/*
 * DANG_BEGIN_MODULE
 *
 * Description:
 *  Manages a list of the available I/O devices.  It will automatically
 *  call their initialization and termination routines.
 *  The current I/O device list includes:
 *     Fully emulated:      pit, pic, cmos, serial
 *     Partially emulated:  rtc, keyb, lpt
 *     Unemulated:          dma, hdisk, floppy, pos
 *
 * Maintainers: Scott Buchholz
 * 
 * DANG_END_MODULE
 *
 * $Date: $
 * $Source: $
 * $Revision: $
 * $State: $
 *
 * $Log: $
 */

#include "config.h"

#include "emu.h"
#include "iodev.h"
#include "int.h"
#include "port.h"
#include "pic.h"
#ifdef NEW_KBD_CODE
#include "keyb_server.h"     /* for keyb_8042_{init,reset} */
#else
#include "keyboard.h"
#endif
#include "lpt.h"
#include "serial.h"
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
#ifdef NEW_KBD_CODE
  { "keyb",    keyb_8042_init, keyb_8042_reset, NULL },
#else
  { "keyb",    keyb_init,    keyb_reset,    NULL },
#endif
#if 0
  { "rtc",     NULL,         rtc_reset,     NULL },
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
