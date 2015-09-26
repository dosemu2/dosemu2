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

/* status bits, Centronics */
#define CTS_STAT_NOIOERR	LPT_STAT_NOIOERR
#define CTS_STAT_ONLINE		LPT_STAT_ONLINE
#define CTS_STAT_NOPAPER	LPT_STAT_NOPAPER
#define CTS_STAT_NOT_ACKing	LPT_STAT_NOT_ACK
#define CTS_STAT_BUSY		LPT_STAT_NOT_BUSY

/* control bits, Centronics */
#define CTS_CTRL_NOT_SELECT	LPT_CTRL_SELECT
#define CTS_CTRL_NOT_INIT	LPT_CTRL_NOT_INIT
#define CTS_CTRL_NOT_AUTOLF	LPT_CTRL_AUTOLF
#define CTS_CTRL_NOT_STROBE	LPT_CTRL_STROBE

/* inversion masks to convert LPT<-->Centronics */
#define LPT_STAT_INV_MASK (CTS_STAT_BUSY)
#define LPT_CTRL_INV_MASK (CTS_CTRL_NOT_STROBE | CTS_CTRL_NOT_AUTOLF | \
	CTS_CTRL_NOT_SELECT)

#define DEFAULT_STAT (CTS_STAT_ONLINE | CTS_STAT_NOIOERR | \
    CTS_STAT_NOT_ACKing | LPT_STAT_NOT_IRQ)
#define DEFAULT_CTRL (CTS_CTRL_NOT_INIT | CTS_CTRL_NOT_AUTOLF | \
    CTS_CTRL_NOT_STROBE)

#define NUM_PRINTERS 9
static struct printer lpt[NUM_PRINTERS] =
{
  {NULL, NULL, 5, 0x378, .control = DEFAULT_CTRL, .status = DEFAULT_STAT},
  {NULL, NULL, 5, 0x278, .control = DEFAULT_CTRL, .status = DEFAULT_STAT},
  {NULL, NULL, 10, 0x3bc, .control = DEFAULT_CTRL, .status = DEFAULT_STAT}
};

ioport_t get_lpt_base(int lptnum)
{
  if (lptnum >= NUM_LPTS)
    return -1;
  return lpt[lptnum].base_port;
}

static int get_printer(ioport_t port)
{
  int i;
  for (i = 0; i < NUM_LPTS; i++)
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
    val = lpt[i].data; /* simple unidirectional port */
    if (debug_level('p') >= 5)
      p_printf("LPT%d: Reading data byte %#x\n", i+1, val);
    break;
  case 1: /* status port, r/o */
    val = lpt[i].status ^ LPT_STAT_INV_MASK;
    /* we should really set ACK after 5 us but here we just
       use the fact that the BIOS only checks this once */
    lpt[i].status |= CTS_STAT_NOT_ACKing | LPT_STAT_NOT_IRQ;
    lpt[i].status &= ~CTS_STAT_BUSY;
    if (debug_level('p') >= 5)
      p_printf("LPT%d: Reading status byte %#x\n", i+1, val);
    break;
  case 2:
    val = lpt[i].control ^ LPT_CTRL_INV_MASK;
    if (debug_level('p') >= 5)
      p_printf("LPT%d: Reading control byte %#x\n", i+1, val);
    break;
  default:
    val = 0xff;
    break;
  }
  return val;
}

static void printer_io_write(ioport_t port, Bit8u value)
{
  int i = get_printer(port);
  if (i == -1)
    return;
  switch (port - lpt[i].base_port) {
  case 0:
    if (debug_level('p') >= 5)
      p_printf("LPT%d: Writing data byte %#x\n", i+1, value);
    lpt[i].data = value;
    break;
  case 1: /* status port, r/o */
    break;
  case 2:
    if (debug_level('p') >= 5)
      p_printf("LPT%d: Writing control byte %#x\n", i+1, value);
    value ^= LPT_CTRL_INV_MASK;		// convert to Centronics
    if (((lpt[i].control & (CTS_CTRL_NOT_STROBE | CTS_CTRL_NOT_SELECT)) == 0)
        && (value & CTS_CTRL_NOT_STROBE)) {
      /* STROBE rising */
      if (debug_level('p') >= 9)
        p_printf("LPT%d: STROBE, sending %#x (%c)\n", i+1, lpt[i].data,
	    lpt[i].data);
      printer_write(i, lpt[i].data);
      lpt[i].status &= ~CTS_STAT_NOT_ACKing;
      lpt[i].status |= CTS_STAT_BUSY;
    }
    lpt[i].control = value;
    break;
  }
}

static int dev_printer_open(int prnum)
{
  int um = umask(026);
  lpt[prnum].dev_fd = open(lpt[prnum].dev, O_WRONLY);
  umask(um);
  if (lpt[prnum].dev_fd == -1) {
    error("LPT%i: error opening %s: %s\n", prnum+1, lpt[prnum].dev,
	strerror(errno));
    return -1;
  }
  p_printf("LPT: opened printer %d to %s\n", prnum, lpt[prnum].dev);
  return 0;
}

static void pipe_callback(void *arg)
{
  char buf[1024];
  int num = (long)arg;
  int n = read(lpt[num].file.from_child, buf, sizeof(buf));
  if (n > 0) {
    buf[n] = 0;
    error("LPT%i: %s\n", num+1, buf);
  }
}

static int pipe_printer_open(int prnum)
{
  int err;
  err = popen2(lpt[prnum].prtcmd, &lpt[prnum].file);
  if (err) {
    error("system(\"%s\") in lpt.c failed, cannot print! "
	"Command returned error %s\n", lpt[prnum].prtcmd, strerror(errno));
    return err;
  }
  p_printf("LPT: doing printer command ..%s..\n", lpt[prnum].prtcmd);
  add_to_io_select(lpt[prnum].file.from_child, pipe_callback, (void *)(long)prnum);
  return err;
}

int printer_open(int prnum)
{
  int rc;

  if (!lpt[prnum].initialized)
    return -1;
  if (lpt[prnum].opened) {
    dosemu_error("opening printer %i twice\n", prnum);
    return 0;
  }

  rc = lpt[prnum].fops.open(prnum);
  if (!rc)
    lpt[prnum].opened = 1;
  else
    error("Error opening printer %i\n", prnum);
  return rc;
}

static int dev_printer_close(int prnum)
{
  return close(lpt[prnum].dev_fd);
}

static int pipe_printer_close(int prnum)
{
  remove_from_io_select(lpt[prnum].file.from_child);
  return pclose2(&lpt[prnum].file);
}

int printer_close(int prnum)
{
  if (lpt[prnum].opened && lpt[prnum].fops.close) {
    p_printf("LPT%i: closing printer\n", prnum+1);
    lpt[prnum].fops.close(prnum);
    lpt[prnum].remaining = 0;
  }
  lpt[prnum].opened = 0;
  return 0;
}

static int dev_printer_write(int prnum, Bit8u outchar)
{
  return write(lpt[prnum].dev_fd, &outchar, 1);
}

static int pipe_printer_write(int prnum, Bit8u outchar)
{
  return write(lpt[prnum].file.to_child, &outchar, 1);;
}

int printer_write(int prnum, Bit8u outchar)
{
  if (!lpt[prnum].initialized)
    return -1;
  if (!lpt[prnum].opened)
    printer_open(prnum);
  lpt[prnum].remaining = lpt[prnum].delay;
  if (debug_level('p') >= 9)
    p_printf("LPT%d: writing %#x (%c)\n", prnum+1, outchar, outchar);
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
  dev_printer_write,
  dev_printer_close,
};
static struct p_fops pipe_pfops =
{
  pipe_printer_open,
  pipe_printer_write,
  pipe_printer_close,
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

  for (i = 0; i < NUM_PRINTERS; i++) {
    lpt[i].initialized = 0;
    lpt[i].opened = 0;
    lpt[i].remaining = 0;	/* mark not accessed yet */
    if (!lpt[i].dev && !lpt[i].prtcmd)
      continue;
    p_printf("LPT%i: initializing printer %s\n", i+1,
	lpt[i].dev ? lpt[i].dev : lpt[i].prtcmd);
    if (lpt[i].dev)
      lpt[i].fops = dev_pfops;
    else if (lpt[i].prtcmd)
      lpt[i].fops = pipe_pfops;
    if (i >= min(config.num_lpt, NUM_LPTS)) lpt[i].base_port = 0;

    if (lpt[i].base_port != 0) {
      io_device.start_addr = lpt[i].base_port;
      io_device.end_addr   = lpt[i].base_port + 2;
      port_register_handler(io_device, 0);
    }
    lpt[i].initialized = 1;
  }
}

void
close_all_printers(void)
{
  int loop;

  for (loop = 0; loop < NUM_PRINTERS; loop++) {
    if (!lpt[loop].opened)
      continue;
    p_printf("LPT: closing printer %d (%s)\n", loop,
	     lpt[loop].dev ? lpt[loop].dev : lpt[loop].prtcmd);
    printer_close(loop);
  }
}

int
printer_tick(u_long secno)
{
  int i;

  for (i = 0; i < NUM_PRINTERS; i++) {
    if (lpt[i].remaining > 0) {
      if (debug_level('p') >= 9)
        p_printf("LPT%i: doing tick %d\n", i+1, lpt[i].remaining);
      if (lpt[i].remaining) {
        reset_idle(2);
	lpt[i].remaining--;
	if (!lpt[i].remaining)
	  printer_close(i);
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
    destptr->delay = pptr->delay;
  }
}

void printer_print_config(int prnum, void (*print)(const char *, ...))
{
  struct printer *pptr = &lpt[prnum];
  (*print)("LPT%d command \"%s\"  timeout %d  device \"%s\"  baseport 0x%03x\n",
	  prnum+1, (pptr->prtcmd ? pptr->prtcmd : ""), pptr->delay,
	   (pptr->dev ? pptr->dev : ""), pptr->base_port);
}

int lpt_get_max(void)
{
  return NUM_PRINTERS;
}

int lpt_is_configured(int num)
{
  return lpt[num].initialized;
}
