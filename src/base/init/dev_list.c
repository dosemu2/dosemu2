/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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

#include <string.h>
#include <stdlib.h>
#include "config.h"

#include "emu.h"
#include "iodev.h"
#include "int.h"
#include "port.h"
#include "pic.h"
#include "serial.h"
#include "mouse.h"
#include "keyb_server.h"     /* for keyb_8042_{init,reset} */
#include "lpt.h"
#include "disks.h"
#include "dma.h"
#include "dosemu_debug.h"
#include "pktdrvr.h"
#include "ipx.h"

#ifdef USE_SBEMU
#include "sound.h"
#endif

#include "joystick.h"

struct io_dev_struct {
  const char * name;
  void (* init_func)(void);
  void (* reset_func)(void);
  void (* term_func)(void);
};
#define MAX_IO_DEVICES 30

#define MAX_DEVICES_OWNED 5
struct owned_devices_struct {
  char * dev_names[MAX_DEVICES_OWNED];
  int devs_owned;
} owned_devices[MAX_IO_DEVICES];

static int current_device = -1;

static struct io_dev_struct io_devices[MAX_IO_DEVICES] = {
  { "pit",     NULL,         pit_reset,     NULL },
  { "cmos",    cmos_init,    cmos_reset,    NULL },
  { "video",   video_post_init, NULL, video_close },
  { "internal_mouse",  dosemu_mouse_init,  dosemu_mouse_reset, dosemu_mouse_close },
  { "serial",  serial_init,  serial_reset,  serial_close },
  { "pic",     pic_init,     pic_reset,     NULL },
#if 0
  { "pos",     pos_init,     pos_reset,     NULL },
#endif
  { "lpt",     printer_init, NULL,	    NULL },
  { "dma",     dma_init,     dma_reset,     NULL },
#if 0
  { "floppy",  floppy_init,  floppy_reset,  NULL },
  { "hdisk",   hdisk_init,   hdisk_reset,   NULL },
#endif
  { "disks",   disk_init,    disk_reset,    NULL },
#ifdef USE_SBEMU
  { "sound",   sound_init,   sound_reset,   sound_done },
#endif
  { "joystick", joy_init,    joy_reset,     joy_term },
#ifdef IPX
  { "ipx",      ipx_init,    NULL,          NULL },
#endif
#ifdef USING_NET
  { "packet driver", pkt_init, pkt_reset,   NULL },
#endif
  { NULL,      NULL,         NULL,          NULL }
};


void iodev_init(void)        /* called at startup */
{
  int i;

  for (i = 0; i < MAX_IO_DEVICES; i++)
    if (io_devices[i].init_func) {
      current_device = i;
      io_devices[i].init_func();
    }

  current_device = -1;
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

static int find_device_owner(char *dev_name)
{
	int i, j;
	for(i = 0; i < MAX_IO_DEVICES - 1; i++) {
	    for(j = 0; j < owned_devices[i].devs_owned; j++)
		if (strcmp(dev_name, owned_devices[i].dev_names[j]) == 0)
		    return i;
	}
	return -1;
}

void iodev_add_device(char *dev_name)
{
	int dev_own;
	if (current_device == -1) {
	    error("add_device() is called not during the init stage!\n");
	    leavedos(10);
	}
	dev_own = find_device_owner(dev_name);
	if (dev_own != -1) {
	    error("Device conflict: Attempt to use %s for %s and %s\n",
		dev_name, io_devices[dev_own].name, io_devices[current_device].name);
	    config.exitearly = 1;
	}
	if (owned_devices[current_device].devs_owned >= MAX_DEVICES_OWNED) {
	    error("No free slot for device %s\n", dev_name);
	    config.exitearly = 1;
	}
	c_printf("registering %s for %s\n",dev_name,io_devices[current_device].name);
	owned_devices[current_device].dev_names[owned_devices[current_device].devs_owned++] = dev_name;
}
