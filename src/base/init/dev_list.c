/* 
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
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
#include "dosemu_debug.h"

#ifdef USE_SBEMU
#include "sound.h"
#endif

struct io_dev_struct {
  const char * name;
  void (* init_func)(void);
  void (* reset_func)(void);
  void (* term_func)(void);
};
#define MAX_IO_DEVICES 30

static struct io_dev_struct io_devices[MAX_IO_DEVICES] = {
  { "pit",     pit_init,     pit_reset,     NULL },
  { "cmos",    cmos_init,    cmos_reset,    NULL },
  { "serial",  serial_init,  NULL,          serial_close },
  { "pic",     pic_init,     pic_reset,     NULL },
#ifdef HAVE_KEYBOARD_V1
  { "keyb",    keyb_8042_init, keyb_8042_reset, NULL },
#endif
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

  for (ptr = io_devices; ptr < &io_devices[MAX_IO_DEVICES]; ptr++)
    if (ptr->init_func)
      ptr->init_func();

  for (ptr = io_devices; ptr->name; ptr++)
    if (ptr->reset_func)
      ptr->reset_func();
}

void iodev_reset(void)        /* called at reboot */
{
  struct io_dev_struct *ptr;

  for (ptr = io_devices; ptr < &io_devices[MAX_IO_DEVICES]; ptr++)
    if (ptr->reset_func)
      ptr->reset_func();
}

void iodev_term(void)
{
  struct io_dev_struct *ptr;

  for (ptr = io_devices; ptr < &io_devices[MAX_IO_DEVICES]; ptr++)
    if (ptr->term_func)
      ptr->term_func();
}

void iodev_register(char *name, 
	void (*init_func)(void), 
	void (*reset_func)(void), 
	void (*term_func)(void))
{
	struct io_dev_struct *ptr;
	for(ptr = io_devices; ptr < &io_devices[MAX_IO_DEVICES -1]; ptr++) {
		if (ptr->name) {
			if (strcmp(ptr->name, name) == 0) {
				g_printf("IODEV: %s already registred\n",
					name);
				return;
			}
			continue;
		}
		ptr->name = name;
		ptr->init_func = init_func;
		ptr->reset_func = reset_func;
		ptr->term_func = term_func;
		return;
	}
	g_printf("IODEV: Could not find free slot for %s\n",
		name);
	return;
}

void iodev_unregiseter(char *name)
{
	struct io_dev_struct *ptr;
	for(ptr = io_devices; ptr < &io_devices[MAX_IO_DEVICES -1]; ptr++) {
		if (!ptr->name || (strcmp(ptr->name, name) != 0)) {
			continue;
		}
		ptr->name = 0;
		ptr->init_func = 0;
		ptr->reset_func = 0;
		ptr->term_func = 0;
	}
}
