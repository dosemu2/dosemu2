/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* for the Linux dos emulator versions 0.49 and newer
 *
 */
#define LPT_C 1

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "emu.h"
#include "bios.h"
#include "port.h"
#include "timers.h"
#include "lpt.h"
#include "utilities.h"
#include "dos2linux.h"

static int stub_printer_write(int, int);

struct printer lpt[NUM_PRINTERS] =
{
  {NULL, NULL, 5, 0x378, .status = LPT_NOTBUSY | LPT_ONLINE | LPT_IOERR | LPT_ACK},
  {NULL, NULL, 5, 0x278, .status = LPT_NOTBUSY | LPT_ONLINE | LPT_IOERR | LPT_ACK},
  {NULL, NULL, 10, 0x3bc, .status = LPT_NOTBUSY | LPT_ONLINE | LPT_IOERR | LPT_ACK}
};

static int get_printer(ioport_t port)
{
  int i;
  for (i = 0; i < 3; i++)
    if (lpt[i].base_port <= port && port <= lpt[i].base_port + 2)
      return i;
  return -1;
}

static Bit8u printer_io_read(ioport_t port)
{
  int i = get_printer(port);
  Bit8u val;

  if (i == -1)
    return 0xff;

  switch (port - lpt[i].base_port) {
  case 0:
    return lpt[i].data; /* simple unidirectional port */
  case 1: /* status port, r/o */
    val = lpt[i].status;
    /* we should really set ACK after 5 us but here we just
       use the fact that the BIOS only checks this once */
    lpt[i].status |= LPT_ACK;
    return val;
  case 2:
    return lpt[i].control;
  default:
    return 0xff;
  }
}

static void printer_io_write(ioport_t port, Bit8u value)
{
  int i = get_printer(port);
  if (i == -1)
    return;
  switch (port - lpt[i].base_port) {
  case 0:
    lpt[i].status = printer_write(i, value);
    lpt[i].data = value;
    break;
  case 1: /* status port, r/o */
    break;
  case 2:
    lpt[i].status &= ~LPT_ACK;
    lpt[i].control = value;
    break;
  default:
    break;
  }
}

static int dev_printer_open(int prnum)
{
  int um = umask(026);
  lpt[prnum].file = fopen(lpt[prnum].dev, "a");
  umask(um);
  return 0;
}

static int pipe_printer_open(int prnum)
{
  p_printf("LPT: doing printer command ..%s..\n", lpt[prnum].prtcmd);

  lpt[prnum].file = popen(lpt[prnum].prtcmd, "w");
  if (lpt[prnum].file == NULL)
    error("system(\"%s\") in lpt.c failed, cannot print!\
  Command returned error %s\n", lpt[prnum].prtcmd, strerror(errno));
  return 0;
}

int printer_open(int prnum)
{
  int rc;

  if (lpt[prnum].file != NULL)
    return 0;

  rc = lpt[prnum].fops.open(prnum);
  /* use line buffering so we don't need to have a long wait for output */
  setvbuf(lpt[prnum].file, NULL, _IOLBF, 0);
  p_printf("LPT: opened printer %d to %s, file %p\n", prnum,
	   lpt[prnum].dev ? lpt[prnum].dev : "<<NODEV>>",
           (void *) lpt[prnum].file);
  return rc;
}

static int dev_printer_close(int prnum)
{
  if (lpt[prnum].file != NULL)
    fclose(lpt[prnum].file);
  return 0;
}

static int pipe_printer_close(int prnum)
{
  if (lpt[prnum].file != NULL)
    pclose(lpt[prnum].file);
  lpt[prnum].file = NULL;
  return 0;
}

int printer_close(int prnum)
{
  if (lpt[prnum].fops.close) {
    p_printf("LPT: closing printer %d, %s\n", prnum,
	     lpt[prnum].dev ? lpt[prnum].dev : "<<NODEV>>");

    lpt[prnum].fops.close(prnum);
    lpt[prnum].file = NULL;
    lpt[prnum].remaining = -1;
  }
  lpt[prnum].fops.write = stub_printer_write;
  return 0;
}

static int stub_printer_write(int prnum, int outchar)
{
  printer_open(prnum);

  /* from now on, use real write */
  lpt[prnum].fops.write = lpt[prnum].fops.realwrite;

  return printer_write(prnum, outchar);
}

static int file_printer_write(int prnum, int outchar)
{
  lpt[prnum].remaining = lpt[prnum].delay;

  fputc(outchar, lpt[prnum].file);
  return (LPT_NOTBUSY | LPT_IOERR | LPT_ONLINE);
}

int printer_write(int prnum, int outchar)
{
  return lpt[prnum].fops.write(prnum, outchar);
}

/* DANG_BEGIN_FUNCTION printer_init
 *
 * description:
 *  Initialize printer control structures
 *
 * DANG_END_FUNCTIONS
 */
static struct p_fops dev_pfops =
{
  dev_printer_open,
  stub_printer_write,
  dev_printer_close,
  file_printer_write
};
static struct p_fops pipe_pfops =
{
  pipe_printer_open,
  stub_printer_write,
  pipe_printer_close,
  file_printer_write
};

void
printer_init(void)
{
  int i;
  emu_iodev_t io_device;

  io_device.read_portb   = printer_io_read;
  io_device.write_portb  = printer_io_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.read_portd   = NULL;
  io_device.write_portd  = NULL;
  io_device.handler_name = "Parallel printer";
  io_device.irq          = EMU_NO_IRQ;
  io_device.fd           = -1;

  for (i = 0; i < 3; i++) {
    p_printf("LPT: initializing printer %s\n", lpt[i].dev ? lpt[i].dev : "<<NODEV>>");
    lpt[i].file = NULL;
    lpt[i].remaining = -1;	/* mark not accessed yet */
    if (lpt[i].dev)
      lpt[i].fops = dev_pfops;
    else if (lpt[i].prtcmd)
      lpt[i].fops = pipe_pfops;
    if (i >= config.num_lpt) lpt[i].base_port = 0;

    if (lpt[i].base_port != 0 && lpt[i].fops.open) {
      io_device.start_addr = lpt[i].base_port;
      io_device.end_addr   = lpt[i].base_port + 2;
      port_register_handler(io_device, 0);
    }
  }
}

void
close_all_printers(void)
{
  int loop;

  for (loop = 0; loop < NUM_PRINTERS; loop++) {
    p_printf("LPT: closing printer %d (%s)\n", loop,
	     lpt[loop].dev ? lpt[loop].dev : "<<NODEV>>");
    printer_close(loop);
  }
}

int
printer_tick(u_long secno)
{
  int i;

  for (i = 0; i < NUM_PRINTERS; i++) {
    if (lpt[i].remaining >= 0) {
      p_printf("LPT: doing real tick for %d\n", i);
      if (lpt[i].remaining) {
        reset_idle(2);
	lpt[i].remaining--;
	if (!lpt[i].remaining)
	  printer_close(i);
	else if (lpt[i].file != NULL)
	  fflush(lpt[i].file);
      }
    }
  }
  return 0;
}

void printer_config(int prnum, struct printer *pptr)
{
  struct printer *destptr;

  if (prnum < NUM_PRINTERS) {
    destptr = &lpt[prnum];
    destptr->prtcmd = pptr->prtcmd;
    destptr->dev = pptr->dev;
    destptr->file = pptr->file;
    destptr->remaining = pptr->remaining;
    destptr->delay = pptr->delay;
  }
}

void printer_print_config(int prnum, void (*print)(char *, ...))
{
  struct printer *pptr = &lpt[prnum];
  (*print)("LPT%d command \"%s\"  timeout %d  device \"%s\"  baseport 0x%03x\n",
	  prnum+1, (pptr->prtcmd ? pptr->prtcmd : ""), pptr->delay,
	   (pptr->dev ? pptr->dev : ""), pptr->base_port); 
}

#undef LPT_C
