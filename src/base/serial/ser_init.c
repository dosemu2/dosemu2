/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

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
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>

#include "config.h"
#include "emu.h"
#include "port.h"
#include "mouse.h"
#include "bios.h"
#include "pic.h"
#include "serial.h"
#include "ser_defs.h"
#include "utilities.h"	/* due to getpwnam */
#include "iodev.h"

int no_local_video = 0;

/* See README.serial file for more information on the com[] structure 
 * The declarations for this is in ../include/serial.h
 */
#if 0
serial_t com[MAX_SER];
#endif

/*  Determines if the tty is already locked.  Stolen from uri-dip-3.3.7k
 *  Nice work Uri Blumenthal & Ian Lance Taylor!
 *  [nam = complete path to lock file, return = nonzero if locked]
 */
static int tty_already_locked(char *nam)
{
  int  i = 0, pid = 0;
  FILE *fd = (FILE *)0;

  /* Does the lock file on our device exist? */
  if ((fd = fopen(nam, "r")) == (FILE *)0)
    return 0; /* No, return perm to continue */

  /* Yes, the lock is there.  Now let's make sure at least */
  /* there's no active process that owns that lock.        */
  if(config.tty_lockbinary)
    i = read(fileno(fd), &pid, sizeof(pid)) == sizeof(pid);
  else 
    i = fscanf(fd, "%d", &pid);

  (void) fclose(fd);

  if (i != 1) /* Lock file format's wrong! Kill't */
    return 0;

  /* We got the pid, check if the process's alive */
  if (kill(pid, 0) == 0)      /* it found process */
    return 1;                 /* Yup, it's running... */

  /* Dead, we can proceed locking this device...  */
  return 0;
}


/*  Locks or unlocks a terminal line Stolen from uri-dip-3.3.7k
 *  Nice work Uri Blumenthal & Ian Lance Taylor!
 *  [path = device name, 
 *   mode: 1 = lock, 2 = reaquire lock, anythingelse = unlock,
 *   return = zero if success, greater than zero for failure]
 */
static int tty_lock(char *path, int mode)
{
  char saved_path[PATH_MAX];
  char dev_nam[20];
  struct passwd *pw;
  pid_t ime;
  int cwrote;

  memset(dev_nam, 0, sizeof(dev_nam));
  sprintf(saved_path, "%s/%s%s", config.tty_lockdir, config.tty_lockfile, 
         (strrchr(path, '/')+1));
  strcpy(dev_nam, path);
  
  if (mode == 1) {      /* lock */
    if (path == NULL) return(0);        /* standard input */
    {
      FILE *fd;
      if (tty_already_locked(saved_path) == 1) {
        s_printf("attempt to use already locked tty %s\n", saved_path);
        error("attempt to use already locked tty %s\n", saved_path);
        return (-1);
      }
      fd = fopen(saved_path, "w");
      if (fd == (FILE *)0) {
        s_printf("lock: (%s): %s\n", saved_path, strerror(errno));
        error("tty: lock: (%s): %s\n", saved_path, strerror(errno));
        return(-1);
      }

      ime = getpid();
      if(config.tty_lockbinary)
	cwrote = write (fileno(fd), &ime, sizeof(ime));
      else
	fprintf(fd, "%10d\n", (int)ime);

      (void)fclose(fd);
    }

    /* Make sure UUCP owns the lockfile.  Required by some packages. */
    if ((pw = getpwnam(OWNER_LOCKS)) == NULL) {
      error("tty: lock: UUCP user %s unknown!\n", OWNER_LOCKS);
      return(0);        /* keep the lock anyway */
    }
    
    (void) chown(saved_path, pw->pw_uid, pw->pw_gid);
    (void) chmod(saved_path, 0644);
  } 
  else if (mode == 2) { /* re-acquire a lock after a fork() */
    FILE *fd;

     fd = fopen(saved_path,"w");
     if (fd == (FILE *)0) {
     s_printf("tty_lock(%s) reaquire: %s\n", 
              saved_path, strerror(errno));
      error("tty_lock: reacquire (%s): %s\n",
              saved_path, strerror(errno));
      return(-1);
    }
    ime = getpid();
     
    if(config.tty_lockbinary)
      cwrote = write (fileno(fd), &ime, sizeof(ime));
    else
      fprintf(fd, "%10d\n", (int)ime);

    (void) fclose(fd);
    (void) chmod(saved_path, 0444);
    return(0);
  } 
  else {    /* unlock */
    FILE *fd;
    int retval;

    fd = fopen(saved_path,"w");
    if (fd == (FILE *)0) {
      s_printf("DOSEMU: tty_lock: can't reopen to delete: %s\n",
             strerror(errno));
      return (-1);
    }
      
    retval = unlink(saved_path);
    if (retval < 0) {
      s_printf("tty: unlock: (%s): %s\n", saved_path,
             strerror(errno));
      error("tty: unlock: (%s): %s\n", saved_path,
             strerror(errno));
      return(-1);
    }
  }
  return(0);
}

static void async_serial_run(void)
{
  /* Currently the fd is not passed to function, so we just call serial_run() */
  s_printf("SERIAL: Async notification received\n");
  serial_run();
}

/* This function opens ONE serial port for DOSEMU.  Normally called only
 * by do_ser_init below.   [num = port, return = file descriptor]
 */
int ser_open(int num)
{
  s_printf("SER%d: Running ser_open, fd=%d\n",num, com[num].fd);
  
  if (com[num].fd != -1) return (com[num].fd);
  
  if ( com[num].virtual )
  {
    if (tty_lock(com[num].dev, 0) >= 0)
    {
      dbug_printf("Opening Virtual Port\n");
      com[num].dev_locked = FALSE;
      com[num].fd = 0;
      no_local_video = 1;
    }
  }
  else
    if ( tty_lock(com[num].dev, 1) >= 0) {		/* Lock port */
      /* We know that we have access to the serial port */
      com[num].dev_locked = TRUE;
    
      /* If the port is used for a mouse, then remove lockfile, because
       * the use of the mouse serial port can be switched between processes,
       * such as on Linux virtual consoles.
       */
      if (com[num].mouse)
        if (tty_lock(com[num].dev, 0) >= 0)   		/* Unlock port */
          com[num].dev_locked = FALSE;
    }
  else {
    /* The port is in use by another process!  Don't touch the port! */
    com[num].dev_locked = FALSE;
    com[num].fd = -2;
    return(-1);
  }
  
  if (com[num].dev[0] == 0) {
    s_printf("SER%d: Device file not yet defined!\n",num);
    return (-1);
  }
  
  com[num].fd = RPT_SYSCALL(open(com[num].dev, O_RDWR | O_NONBLOCK));
  if (com[num].fd < 0) {
    error("SERIAL: Unable to open device %s: %s\n",
      com[num].dev, strerror(errno));
    goto fail_unlock;
  }
  if (!isatty(com[num].fd)) {
    error("SERIAL: Serial port device %s is not a tty, closing\n",
      com[num].dev);
    goto fail_close;
  }
  RPT_SYSCALL(tcgetattr(com[num].fd, &com[num].oldset));
  add_to_io_select(com[num].fd, 1, async_serial_run);
  return com[num].fd;

fail_close:
  close(com[num].fd);
  /* fall through */
fail_unlock:
  if (tty_lock(com[num].dev, 0) >= 0)   		/* Unlock port */
    com[num].dev_locked = FALSE;

  com[num].fd = -2; // disable permanently
  return -1;
}

void ser_set_params(int num)
{
  int data = 0;
  /* The following adjust raw line settings needed for DOSEMU serial     */
  /* These defines are based on the Minicom 1.70 communications terminal */
#if 1
  com[num].newset.c_cflag |= (CLOCAL | CREAD);
  com[num].newset.c_cflag &= ~(HUPCL | CRTSCTS);
  com[num].newset.c_iflag |= (IGNBRK | IGNPAR);
  com[num].newset.c_iflag &= ~(BRKINT | PARMRK | INPCK | ISTRIP |
                               INLCR | IGNCR | INLCR | ICRNL | IXON | 
                               IXOFF | IUCLC | IXANY | IMAXBEL);
  com[num].newset.c_oflag &= ~(OPOST | OLCUC | ONLCR | OCRNL | ONOCR |
                               ONLRET | OFILL | OFDEL);
  com[num].newset.c_lflag &= ~(XCASE | ISIG | ICANON | IEXTEN | ECHO | 
                               ECHONL | ECHOE | ECHOK | ECHOPRT | ECHOCTL | 
                               ECHOKE | NOFLSH | TOSTOP);
#else
  /* These values should only be used as a last resort, or for testing */
  com[num].newset.c_iflag = IGNBRK | IGNPAR;
  com[num].newset.c_lflag = 0;
  com[num].newset.c_oflag = 0;
  com[num].newset.c_cflag |= CLOCAL | CREAD;
  com[num].newset.c_cflag &= ~(HUPCL | CRTSCTS);
#endif

#ifdef __linux__
  com[num].newset.c_line = 0;
#endif
  com[num].newset.c_cc[VMIN] = 1;
  com[num].newset.c_cc[VTIME] = 0;
  if (com[num].system_rtscts) com[num].newset.c_cflag |= CRTSCTS;
  tcsetattr(com[num].fd, TCSANOW, &com[num].newset);

  com[num].dll = 0x30;			/* Baudrate divisor LSB: 2400bps */
  com[num].dlm = 0;			/* Baudrate divisor MSB: 2400bps */
  com[num].tx_char_time = DIV_2400 * 10;/* 115200ths of second per char */
  com[num].TX = 0;			/* Transmit Holding Register */
  com[num].RX = 0;			/* Received Byte Register */
  com[num].IER = 0;			/* Interrupt Enable Register */
  com[num].IIR = UART_IIR_NO_INT;	/* Interrupt I.D. Register */
  com[num].LCR = UART_LCR_WLEN8;	/* Line Control Register: 5N1 */
  com[num].DLAB = 0;			/* DLAB for baudrate change */
  com[num].FCReg = 0; 			/* FIFO Control Register */
  com[num].rx_fifo_trigger = 1;		/* Receive FIFO trigger level */
  com[num].MCR = 0;			/* Modem Control Register */
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;   /* Txmit Hold Reg Empty */
  com[num].LSRqueued = 0;		/* Queued LSR bits */
  com[num].MSR = 0;			/* Modem Status Register */
  com[num].MSRqueued = 0;		/* Queued MSR bits */
  com[num].SCR = 0; 			/* Scratch Register */
  com[num].int_enab = 0;		/* FLAG: Interrupts disabled */
  com[num].int_pend = 0;		/* FLAG: No interrupts pending */
  com[num].int_condition = 0;		/* FLAG: No int conditions set */
  com[num].int_request = 0;		/* FLAG: No int requested */
  com[num].fifo_enable = 0;		/* FLAG: FIFO enabled */
  com[num].ms_timer = 0;		/* Modem Status check timer */
  com[num].rx_timer = 0;		/* Receive read() polling timer */
  com[num].tx_timer = 0;		/* Transmi countdown to next char */
  com[num].tx_trigger = 0;		/* FLAG: Dont start more xmit ints */
  com[num].rx_timeout = TIMEOUT_RX;	/* FLAG: Receive timeout */
  com[num].tx_overflow = 0;		/* FLAG: Outgoing buffer overflow */
  com[num].rx_fifo_size = 16;		/* Size of receive FIFO to emulate */
  uart_clear_fifo(num,UART_FCR_CLEAR_CMD);	/* Initialize FIFOs */

  if(s2_printf) s_printf("SER%d: do_ser_init: running ser_termios\n",num);
  ser_termios(num);			/* Set line settings now */
  modstat_engine(num);

  /* Pull down DTR and RTS.  This is the most natural for most comm */
  /* devices including mice so that DTR rises during mouse init.    */
  if (!com[num].pseudo) {
    data = TIOCM_DTR | TIOCM_RTS;
    if (ioctl(com[num].fd, TIOCMBIC, &data) && errno == EINVAL) {
      s_printf("SER%d: TIOCMBIC unsupported, setting pseudo flag\n", num);
      com[num].pseudo = 1;
    }
  }
}


/* This function closes ONE serial port for DOSEMU.  Normally called 
 * only by do_ser_init below.   [num = port, return = file error code]
 */
static int ser_close(int num)
{
  int i;
  s_printf("SER%d: Running ser_close\n",num);
  remove_from_io_select(com[num].fd, 1);
  uart_clear_fifo(num,UART_FCR_CLEAR_CMD);
  
  /* save current dosemu settings of the file and restore the old settings
   * before closing the file down. 
   */
  RPT_SYSCALL(tcgetattr(com[num].fd, &com[num].newset));
  RPT_SYSCALL(tcsetattr(com[num].fd, TCSADRAIN, &com[num].oldset));
  i = RPT_SYSCALL(close(com[num].fd));
  com[num].fd = -1;
  
  /* Clear the lockfile from DOSEMU */
  if (com[num].dev_locked) {
    if (tty_lock(com[num].dev, 0) >= 0) 
      com[num].dev_locked = FALSE;
  }
  return (i);
}


static Bit8u com_readb(ioport_t port) {
  int tmp;
  for (tmp = 0; tmp < config.num_ser; tmp++) {
    if (((u_short)(port & ~7)) == com[tmp].base_port) {
      return(do_serial_in(tmp, (int)port));
    }
  }
  return 0;
}

static void com_writeb(ioport_t port, Bit8u value) {
  int tmp;
  for (tmp = 0; tmp < config.num_ser; tmp++) {
    if (((u_short)(port & ~7)) == com[tmp].base_port) {
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
  int i;
  
  /* The following section sets up default com port, interrupt, base
  ** port address, and device path if they are undefined. The defaults are:
  **
  **   COM1:   irq = 4    base_port = 0x3F8    device = /dev/ttyS0
  **   COM2:   irq = 3    base_port = 0x2F8    device = /dev/ttyS1
  **   COM3:   irq = 4    base_port = 0x3E8    device = /dev/ttyS2
  **   COM4:   irq = 3    base_port = 0x2E8    device = /dev/ttyS3
  **
  ** If COMx is unspecified, the next unused COMx port number is assigned.
  */
  if (com[num].real_comport == 0) {		/* Is comport number undef? */
    for (i = 1; i < 16; i++) if (com_port_used[i] != 1) break;
    com[num].real_comport = i;
    com_port_used[i] = 1;
    s_printf("SER%d: No COMx port number given, defaulting to COM%d\n", num, i);
  }

  if (com[num].interrupt <= 0) {		/* Is interrupt undefined? */
    switch (com[num].real_comport) {		/* Define it depending on */
    case 4:  com[num].interrupt = 0x3; break;	/*  using standard irqs */
    case 3:  com[num].interrupt = 0x4; break;
    case 2:  com[num].interrupt = 0x3; break;
    default: com[num].interrupt = 0x4; break;
    }
  }
  
  if (com[num].base_port <= 0) {		/* Is base port undefined? */
    switch (com[num].real_comport) {		/* Define it depending on */ 
    case 4:  com[num].base_port = 0x2E8; break;	/*  using standard addrs */
    case 3:  com[num].base_port = 0x3E8; break;
    case 2:  com[num].base_port = 0x2F8; break;
    default: com[num].base_port = 0x3F8; break;
    }
  }

  if (com[num].dev[0] == 0) {			/* Is the device file undef? */
    switch (com[num].real_comport) {		/* Define it using std devs */
    case 4:  strcpy(com[num].dev, "/dev/ttyS3"); break;
    case 3:  strcpy(com[num].dev, "/dev/ttyS2"); break;
    case 2:  strcpy(com[num].dev, "/dev/ttyS1"); break;
    default: strcpy(com[num].dev, "/dev/ttyS0"); break;
    }
  }
  iodev_add_device(com[num].dev);

  /* FOSSIL emulation is inactive at startup. */
  com[num].fossil_active = FALSE;

  /* Flag whether to emulate RTS/CTS for DOS or let Linux do the job
   * If this is nonzero, Linux will handle RTS/CTS flow control directly.
   * DANG_FIXTHIS This needs more work before it is implemented into /etc/dosemu.conf as an 'rtscts' option.
   */
  com[num].system_rtscts = 0;
 
  /* convert irq number to pic_ilevel number and set up interrupt
   * if irq is invalid, no interrupt will be assigned 
   */
  if(com[num].interrupt < 16) {
    com[num].interrupt = pic_irq_list[com[num].interrupt];
    s_printf("SER%d: enabling interrupt %d\n", num, com[num].interrupt);
    pic_seti(com[num].interrupt, pic_serial_run, 0, NULL);
    pic_unmaski(com[num].interrupt);
  }
  irq_source_num[com[num].interrupt] = num;	/* map interrupt to port */

  /*** The following is where the real initialization begins ***/

  /* Tell the port manager that we exist and that we're alive */
  io_device.read_portw  = NULL;
  io_device.write_portw = NULL;
  io_device.read_portd  = NULL;
  io_device.write_portd = NULL;
  io_device.start_addr  = com[num].base_port;
  io_device.end_addr    = com[num].base_port+7;
  io_device.irq         = com[num].interrupt;
  io_device.fd		= -1;
  switch (num) {
    case 0 :
      io_device.read_portb   = com_readb;
      io_device.write_portb  = com_writeb;
      io_device.handler_name = "COM1";
      break;
    case 1 :
      io_device.read_portb   = com_readb;
      io_device.write_portb  = com_writeb;
      io_device.handler_name = "COM2";
      break;
    case 2 :
      io_device.read_portb   = com_readb;
      io_device.write_portb  = com_writeb;
      io_device.handler_name = "COM3";
      break;
    case 3 :
      io_device.read_portb   = com_readb;
      io_device.write_portb  = com_writeb;
      io_device.handler_name = "COM4";
      break;
  }
  port_register_handler(io_device, 0);

  /* Information about serial port added to debug file */
  s_printf("SER%d: COM%d, intlevel=%d, base=0x%x, device=%s\n", 
        num, com[num].real_comport, com[num].interrupt, 
        com[num].base_port, com[num].dev);

  /* Write serial port information into BIOS data area 0040:0000
   * This is for DOS and many programs to recognize ports automatically
   */
  if ((com[num].real_comport >= 1) && (com[num].real_comport <= 4)) {
#if 1
    *((u_short *) (0x400) + (com[num].real_comport-1)) = com[num].base_port;
#else
    WRITE_WORD(0x400 + (com[num].real_comport-1)*2, com[num].base_port);
#endif

/* Debugging to determine whether memory location was written properly */
    s_printf("SER%d: BIOS memory location 0x%x has value of 0x%x\n", num,
	(int)((u_short *) (0x400) + (com[num].real_comport-1)) 
	,READ_WORD(0x400 + 2*(com[num].real_comport-1)));
  }

  /* first call to serial timer update func to initialize the timer */
  /* value, before the com[num] structure is initialized */
  serial_timer_update();

  /* Set file descriptor as unused, then attempt to open serial port */
  com[num].fd = -1;  
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
  warn("SERIAL $Header$\n");
  s_printf("SER: Running serial_init, %d serial ports\n", config.num_ser);

  /* Clean the BIOS data area at 0040:0000 for serial ports */
#if 1
  *(u_short *) 0x400 = 0;
  *(u_short *) 0x402 = 0;
  *(u_short *) 0x404 = 0;
  *(u_short *) 0x406 = 0;
#else
  WRITE_WORD(0x400, 0);
  WRITE_WORD(0x402, 0);
  WRITE_WORD(0x404, 0);
  WRITE_WORD(0x406, 0);
#endif

  /* Do UART init here - Need to set up registers and init the lines. */
  for (i = 0; i < config.num_ser; i++) {
    com[i].fd = -1;
    com[i].dev_locked = FALSE;
    
    /* Serial port init is skipped if the port is used for a mouse, and 
     * dosemu is running in Xwindows, or not at the console.  This is due
     * to the fact the mouse is in use by Xwindows (internal driver is used)
     * Direct access to the mouse by dosemu is useful mainly at the console.
     */
    if (com[i].mouse && !config.console)
      s_printf("SER%d: Not touching mouse outside of the console!\n",i);
    else
      do_ser_init(i);
  }
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
    if (com[i].fd < 0)
      continue;
    if (com[i].mouse && !config.console)
      s_printf("SER%d: Not touching mouse outside of the console!\n",i);
    else {
      RPT_SYSCALL(tcsetattr(com[i].fd, TCSADRAIN, &com[i].oldset));
      ser_close(i);
    }
  }
}

/* The following de-initializes the mouse on the serial port that the mouse
 * has been enabled on.  For mouse sharing purposes, this is the function
 * that is called when the user switches out of the VC running DOSEMU.
 * (Also, this silly function name needs to be changed soon.)
 */
void child_close_mouse(void)
{
  u_char i, rtrn;
  if (config.console) {
    s_printf("MOUSE: CLOSE function starting. num_ser=%d\n", config.num_ser);
    for (i = 0; i < config.num_ser; i++) {
      s_printf("MOUSE: CLOSE port=%d, dev=%s, fd=%d, valid=%d\n", 
                i, com[i].dev, com[i].fd, com[i].mouse);
      if ((com[i].mouse == TRUE) && (com[i].fd > 0)) {
        s_printf("MOUSE: CLOSE port=%d: Running ser_close.\n", i);
        rtrn = ser_close(i);
        if (rtrn) s_printf("MOUSE SERIAL ERROR - %s\n", strerror(errno));
      }
      else {
        s_printf("MOUSE: CLOSE port=%d: Not running ser_close.\n", i);
      }
    }
    s_printf("MOUSE: CLOSE function ended.\n");
  }
}

/* The following initializes the mouse on the serial port that the mouse
 * has been enabled on.  For mouse sharing purposes, this is the function
 * that is called when the user switches back into the VC running DOSEMU.
 * (Also, this silly function name needs to be changed soon.)
 */
void child_open_mouse(void)
{
  u_char i;
  if (config.console) {
    s_printf("MOUSE: OPEN function starting.\n");
    for (i = 0; i < config.num_ser; i++) {
      s_printf("MOUSE: OPEN port=%d, type=%d, dev=%s, valid=%d\n",
                i, config.mouse.type, com[i].dev, com[i].mouse);
      if (com[i].mouse == TRUE) {
        s_printf("MOUSE: OPEN port=%d: Running ser-open.\n", i);
        com[i].fd = -1;
        ser_open(i);
        tcgetattr(com[i].fd, &com[i].newset);
      }
    }
  }
}

