/* DANG_BEGIN_MODULE
 *
 * REMARK
 * ser_init.c: Serial ports initialization for DOSEMU
 * Please read the README.serial file in this directory for more info!
 *
 * Lock file stuff was derived from Taylor UUCP with these copyrights:
 * Copyright (C) 1991, 1992 Ian Lance Taylor
 * Uri Blumenthal <uri@watson.ibm.com> (C) 1994
 * Paul Cadach, <paul@paul.east.alma-ata.su> (C) 1994
 *
 * Rest of serial code Copyright (C) 1995 by Mark Rejhon
 *
 * The code in this module is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This module is maintained by Mark Rejhon at these Email addresses:
 *      marky@magmacom.com
 *      ag115@freenet.carleton.ca
 * /REMARK
 *
 * maintainer:
 * Mark Rejhon <marky@ottawa.com>
 *
 * DANG_END_MODULE
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>

#include "emu.h"
#include "sig.h"
#include "port.h"
#include "bios.h"
#include "pic.h"
#include "serial.h"
#include "ser_defs.h"
#include "tty_io.h"
#include "sermouse.h"
#include "utilities.h"	/* due to getpwnam */
#include "iodev.h"

int no_local_video = 0;
serial_t com_cfg[MAX_SER];
com_t com[MAX_SER];
static u_char irq_source_num[255];	/* Index to map from IRQ no. to serial port */
struct ser_dmx {
  ioport_t port;
  Bit8u def_val;
  int use_cnt;
  char name[16];
};
#define DMX_MAX 4
static struct ser_dmx dmxs[DMX_MAX];
static int num_dmxs;

static void add_dmx(ioport_t port, int val)
{
  int i;
  Bit8u dval = val - 1;
  for (i = 0; i < num_dmxs; i++) {
    if (dmxs[i].port == port) {
      if (dmxs[i].def_val != dval) {
        error("SER: inconsistent config for demux on port %#x\n", port);
        return;
      }
      dmxs[i].use_cnt++;
      return;
    }
  }
  num_dmxs++;
  assert(num_dmxs <= DMX_MAX);
  dmxs[i].port = port;
  dmxs[i].def_val = dval;
  dmxs[i].use_cnt = 1;
  sprintf(dmxs[i].name, "ser_dmx_%i", i);
}

static Bit8u dmx_readb(ioport_t port)
{
  int num, i;
  Bit8u val;
  for (i = 0; i < num_dmxs; i++) {
    if (dmxs[i].port == port)
      break;
  }
  assert(i < num_dmxs);
  val = dmxs[i].def_val;
  for (num = 0; num < config.num_ser; num++) {
    if (com_cfg[num].dmx_port == port &&
	(com[num].int_condition & com_cfg[num].dmx_mask)) {
      if (com_cfg[num].dmx_val)
        val |= 1 << com_cfg[num].dmx_shift;
      else
        val &= ~(1 << com_cfg[num].dmx_shift);
    }
  }
  s_printf("SER: read demux at port %#x=%#x\n", dmxs[i].port, val);
  return val;
}

static void dmx_writeb(ioport_t port, Bit8u value)
{
  s_printf("SER: write to readonly port %#x, val=%#x\n", port, value);
}

static int init_dmxs(void)
{
  emu_iodev_t io_device;
  int i;

  for (i = 0; i < num_dmxs; i++) {
    io_device.read_portb  = dmx_readb;
    io_device.write_portb = dmx_writeb;
    io_device.read_portw  = NULL;
    io_device.write_portw = NULL;
    io_device.read_portd  = NULL;
    io_device.write_portd = NULL;
    io_device.start_addr  = dmxs[i].port;
    io_device.end_addr    = dmxs[i].port;
    io_device.irq         = EMU_NO_IRQ;
    io_device.fd          = -1;
    io_device.handler_name = dmxs[i].name;
    port_register_handler(io_device, 0);
    s_printf("SER: added demux at port %#x\n", dmxs[i].port);
  }
  return i;
}

static void ser_reset_dev(int num)
{
  com[num].dll = 0x30;			/* Baudrate divisor LSB: 2400bps */
  com[num].dlm = 0;			/* Baudrate divisor MSB: 2400bps */
  com[num].IER = 0;			/* Interrupt Enable Register */
  com[num].IIR.mask = 0;		/* Interrupt I.D. Register */
  com[num].LCR = UART_LCR_WLEN8;	/* Line Control Register: 5N1 */
  com[num].FCReg = 0; 			/* FIFO Control Register */
  com[num].rx_fifo_trigger = 1;		/* Receive FIFO trigger level */
  com[num].MCR = 0;			/* Modem Control Register */
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;   /* Txmit Hold Reg Empty */
  com[num].MSR = 0;			/* Modem Status Register */
  com[num].SCR = 0; 			/* Scratch Register */
  com[num].int_condition = TX_INTR;	/* FLAG: Pending xmit intr */
  com[num].IIR.flags = 0;		/* FLAG: FIFO disabled */
  com[num].ms_timer = 0;		/* Modem Status check timer */
  com[num].rx_timer = 0;		/* Receive read() polling timer */
  com[num].rx_timeout = 0;		/* FLAG: No Receive timeout */
  com[num].rx_fifo_size = 16;		/* Size of receive FIFO to emulate */
  com[num].tx_cnt = 0;
  uart_clear_fifo(num,UART_FCR_CLEAR_CMD);	/* Initialize FIFOs */
}

static void ser_setup_custom(int num)
{
  int i, cnt;
  switch (com_cfg[num].custom) {
  case SER_CUSTOM_NONE:
    break;
  case SER_CUSTOM_PCCOM:
    s_printf("SER%d: setting up PCCOM config\n", num);
    cnt = 0;
    for (i = 0; i < num; i++) {
      if (com_cfg[i].custom == SER_CUSTOM_PCCOM)
        cnt++;
    }
    com_cfg[num].dmx_port = (cnt < 4 ? 0x2bf : 0x1bf);
    com_cfg[num].dmx_mask = RX_INTR;
    com_cfg[num].dmx_shift = cnt;
    com_cfg[num].dmx_val = 0;

    if (!com_cfg[num].base_port)
      com_cfg[num].base_port = (cnt < 4 ? 0x2a0 : 0x1a0) + cnt * 8;
    com_cfg[num].end_port = com_cfg[num].base_port + 6;
    if (!com_cfg[num].irq)
      com_cfg[num].irq = (cnt < 4 ? 5 : 7);
    break;
  }
}

static Bit8u com_readb(ioport_t port) {
  int tmp;
  for (tmp = 0; tmp < config.num_ser; tmp++) {
    if (((u_short)(port & ~7)) == com_cfg[tmp].base_port) {
      return(do_serial_in(tmp, (int)port));
    }
  }
  return 0;
}

static void com_writeb(ioport_t port, Bit8u value) {
  int tmp;
  for (tmp = 0; tmp < config.num_ser; tmp++) {
    if (((u_short)(port & ~7)) == com_cfg[tmp].base_port) {
      do_serial_out(tmp, (int)port, (int)value);
    }
  }
}

/* The following function is the main initialization routine that
 * initializes the UART for ONE serial port.  This includes setting up
 * the environment, define default variables, the emulated UART's init
 * stat, and open/initialize the serial line.   [num = port]
 */
static void do_ser_init(int num)
{
  emu_iodev_t io_device;

  /* The following section sets up default com port, interrupt, base
  ** port address, and device path if they are undefined. The defaults are:
  **
  **   COM1:   irq = 4    base_port = 0x3F8    device = /dev/ttyS0
  **   COM2:   irq = 3    base_port = 0x2F8    device = /dev/ttyS1
  **   COM3:   irq = 4    base_port = 0x3E8    device = /dev/ttyS2
  **   COM4:   irq = 3    base_port = 0x2E8    device = /dev/ttyS3
  */

  static struct {
    int irq;
    ioport_t base_port;
    char *dev;
    char *handler_name;
  } default_com[MAX_SER] = {
    { 4, 0x3F8, "/dev/ttyS0", "COM1" },
    { 3, 0x2F8, "/dev/ttyS1", "COM2" },
    { 4, 0x3E8, "/dev/ttyS2", "COM3" },
    { 3, 0x2E8, "/dev/ttyS3", "COM4" },

    { 3, 0x4220, "/dev/ttyS4", "COM5" },
    { 3, 0x4228, "/dev/ttyS5", "COM6" },
    { 3, 0x5220, "/dev/ttyS6", "COM7" },
    { 3, 0x5228, "/dev/ttyS7", "COM8" },

    { 4, 0x6220, "/dev/ttyS8", "COM9" },
    { 4, 0x6228, "/dev/ttyS9", "COM10" },
    { 4, 0x7220, "/dev/ttyS10", "COM11" },
    { 4, 0x7228, "/dev/ttyS11", "COM12" },

    { 4, 0x8220, "/dev/ttyS12", "COM13" },
    { 4, 0x8228, "/dev/ttyS13", "COM14" },
    { 4, 0x9220, "/dev/ttyS14", "COM15" },
    { 4, 0x9228, "/dev/ttyS15", "COM16" },
  };

  ser_setup_custom(num);

  if (com_cfg[num].real_comport == 0) {		/* Is comport number undef? */
    error("SER%d: No COMx port number given\n", num);
    config.exitearly = 1;
    return;
  }

  if (com_cfg[num].irq == 0) {		/* Is interrupt undefined? */
    /* Define it depending on using standard irqs */
    com_cfg[num].irq = default_com[com_cfg[num].real_comport-1].irq;
  }
  com[num].interrupt = pic_irq_list[com_cfg[num].irq];

  if (com_cfg[num].base_port == 0) {		/* Is base port undefined? */
    /* Define it depending on using standard addrs */
    com_cfg[num].base_port = default_com[com_cfg[num].real_comport-1].base_port;
  }
  if (com_cfg[num].end_port == 0)
    com_cfg[num].end_port = com_cfg[num].base_port + 7;

#ifdef USE_MODEMU
  if (com_cfg[num].vmodem)
    com_cfg[num].dev = modemu_init(num);
#endif

  if ((!com_cfg[num].dev || !com_cfg[num].dev[0]) && !com_cfg[num].mouse) {	/* Is the device file undef? */
    /* Define it using std devs */
    com_cfg[num].dev = default_com[com_cfg[num].real_comport-1].dev;
  }
  if (com_cfg[num].dev && com_cfg[num].dev[0])
    iodev_add_device(com_cfg[num].dev);

  /* FOSSIL emulation is inactive at startup. */
  com[num].fossil_active = FALSE;

  /* convert irq number to pic_ilevel number and set up interrupt
   * if irq is invalid, no interrupt will be assigned
   */
  if(!irq_source_num[com_cfg[num].irq]) {
    s_printf("SER%d: enabling interrupt %d\n", num, com[num].interrupt);
    pic_seti(com[num].interrupt, pic_serial_run, 0, NULL);
  }
  irq_source_num[com_cfg[num].irq]++;

  /*** The following is where the real initialization begins ***/

  /* Tell the port manager that we exist and that we're alive */
  io_device.read_portb  = com_readb;
  io_device.write_portb = com_writeb;
  io_device.read_portw  = NULL;
  io_device.write_portw = NULL;
  io_device.read_portd  = NULL;
  io_device.write_portd = NULL;
  io_device.start_addr  = com_cfg[num].base_port;
  io_device.end_addr    = com_cfg[num].end_port;
  io_device.irq         = (irq_source_num[com_cfg[num].irq] == 1 ?
                           com_cfg[num].irq : EMU_NO_IRQ);
  io_device.fd		= -1;
  io_device.handler_name = default_com[num].handler_name;
  port_register_handler(io_device, 0);

  /* Information about serial port added to debug file */
  s_printf("SER%d: COM%d, intlevel=%d, base=0x%x, device=%s\n",
        num, com_cfg[num].real_comport, com[num].interrupt,
        com_cfg[num].base_port, com_cfg[num].dev);
#if 0
  /* first call to serial timer update func to initialize the timer */
  /* value, before the com[num] structure is initialized */
  serial_timer_update();
#endif
  /* Set file descriptor as unused, then attempt to open serial port */
  com[num].fd = -1;

  if (com_cfg[num].dmx_port)
    add_dmx(com_cfg[num].dmx_port, com_cfg[num].dmx_val);
}

void serial_reset(void)
{
  int num;
  for (num = 0; num < config.num_ser; num++)
    ser_reset_dev(num);
}

/* DANG_BEGIN_FUNCTION serial_run
 *
 * This is the main housekeeping function, which should be called about
 * 20 to 100 times per second.  The more frequent, the better, up to
 * a certain point.   However, it should be self-compensating if it
 * executes 10 times or even 1000 times per second.   Serial performance
 * increases with frequency of execution of serial_run.
 *
 * Serial mouse performance becomes more smooth if the time between
 * calls to serial_run are smaller.
 *
 * DANG_END_FUNCTION
 */
static void serial_run(void)
{
  int i;
#if 0
  /* Update the internal serial timers */
  serial_timer_update();
#endif
  /* Do the necessary interrupt checksing in a logically efficient manner.
   * All the engines have built-in code to prevent loading the
   * system if they are called 100x's per second.
   */
  for (i = 0; i < config.num_ser; i++) {
    if (!com[i].opened)
      continue;
    serial_update(i);
  }
}

/* DANG_BEGIN_FUNCTION serial_init
 *
 * This is the master serial initialization function that is called
 * upon startup of DOSEMU to initialize ALL the emulated UARTs for
 * all configured serial ports.  The UART is initialized via the
 * initialize_uart function, which opens the serial ports and defines
 * variables for the specific UART.
 *
 * If the port is a mouse, the port is only initialized when i
 *
 * DANG_END_FUNCTION
 */
void serial_init(void)
{
  int i;
  warn("SERIAL $Id$\n");
  s_printf("SER: Running serial_init, %d serial ports\n", config.num_ser);

  /* Do UART init here - Need to set up registers and init the lines. */
  for (i = 0; i < config.num_ser; i++) {
    com[i].num = i;
    com[i].cfg = &com_cfg[i];
    com[i].fd = -1;
    com[i].opened = FALSE;
    com[i].dev_locked = FALSE;
    com[i].drv = com_cfg[i].mouse ? &serm_drv : &tty_drv;

    /* Serial port init is skipped if the port is used for a mouse, and
     * dosemu is running in Xwindows, or not at the console.  This is due
     * to the fact the mouse is in use by Xwindows (internal driver is used)
     * Direct access to the mouse by dosemu is useful mainly at the console.
     */
    do_ser_init(i);
  }

  init_dmxs();

  sigalrm_register_handler(serial_run);
}

/* Like serial_init, this is the master function that is called externally,
 * but at the end, when the user quits DOSEMU.  It deinitializes all the
 * configured serial ports.
 */
void serial_close(void)
{
  int i;
  s_printf("SER: Running serial_close\n");
  for (i = 0; i < config.num_ser; i++) {
    if (!com[i].opened)
      continue;
#ifdef USE_MODEMU
    if (com_cfg[i].vmodem)
      modemu_done(i);
#endif
    ser_close(i);
  }
}


#define SER_FN0(rt, f) \
rt f(int num) \
{ \
  return com[num].drv->f(&com[num]); \
}
#define SER_FN1(rt, f, p, c) \
rt f(int num, p c) \
{ \
  return com[num].drv->f(&com[num], c); \
}
#define SER_FN2(rt, f, p1, c1, p2, c2) \
rt f(int num, p1 c1, p2 c2) \
{ \
  return com[num].drv->f(&com[num], c1, c2); \
}

SER_FN0(void, rx_buffer_dump)
SER_FN0(void, tx_buffer_dump)
SER_FN0(int, serial_get_tx_queued)
SER_FN0(void, ser_termios)
SER_FN1(int, serial_brkctl, int, brkflg)
SER_FN2(ssize_t, serial_write, char*, buf, size_t, len)
SER_FN1(int, serial_dtr, int, flag)
SER_FN1(int, serial_rts, int, flag)
SER_FN0(int, ser_open)
SER_FN0(int, ser_close)
SER_FN0(int, uart_fill)
SER_FN0(int, serial_get_msr)
