/*
 * serial.c: Near-complete software emulation of a 16550A UART.
 * Please send bug reports and bugfixes to mdrejhon@magi.com
 * 
 *   Information on a 16550 is based on information from HELPPC.EXE 2.1 and 
 *   results from National Semiconductor's COMTEST.EXE diagnostics program.
 *   This code aims to emulate a 16550 as accurately as possible, using just
 *   reasonably POSIX.2 compilant code.  In some cases, this code does a 
 *   better job than OS/2 serial emulation (in non-direct mode) done by its 
 *   COM.SYS driver!  This module is fairly huge, but nearly 50% are comments!
 * 
 *   This module is maintained by Mark Rejhon at these Email addresses:
 *        mdrejhon@magi.com
 *        ag115@freenet.carleton.ca
 *
 * COPYRIGHTS
 * ~~~~~~~~~~
 *   Lock file stuff was derived from Taylor UUCP with these copyrights:
 *   Copyright (C) 1991, 1992 Ian Lance Taylor
 *   Uri Blumenthal <uri@watson.ibm.com> (C) 1994
 *   Paul Cadach, <paul@paul.east.alma-ata.su> (C) 1994
 *
 *   Rest of serial code
 *   Copyright (C) 1995 by Mark Rejhon
 *
 *   The code in this module is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of 
 *   the License, or (at your option) any later version.
 *
 *
 * $Date: 1995/02/25 22:38:01 $
 * $Source: /home/src/dosemu0.60/dosemu/RCS/serial.c,v $
 * $Revision: 2.9 $
 * $State: Exp $
 * $Log: serial.c,v $
 * Revision 2.9  1995/02/25  22:38:01  root
 * *** empty log message ***
 *
 * These are older RCS log messages, which may be deleted if you want:
 *
 * Revision 2.8  1995/02/05  16:52:03  * Prep for Scotts patches.
 * Revision 2.8  1995/02/05  16:52:03  * Prep for Scotts patches.
 * Revision 2.7  1994/11/13  00:40:45  * Prep for Hans's latest.
 * Revision 2.6  1994/11/03  11:43:26  * Checkin Prior to Jochen's Latest.
 * Revision 2.5  1994/09/20  01:53:26  * Prep for pre53_21.
 * Revision 2.4  1994/09/11  01:01:23  * Prep for pre53_19.
 * Revision 2.3  1994/08/14  02:52:04  * Rain's CLEANUP and MOUSE for X add'n
 * Revision 2.2  1994/07/04  23:59:23  * Prep for Markkk's NCURSES patches.
 * Revision 2.1  1994/06/12  23:15:37  * Wrap up for release of DOSEMU0.52
 * Revision 1.33  1994/05/13  17:21:00     * pre51_15.
 * Revision 2.9.1.1  1994/05/10  22:46:03  * May be final for DOSEMU 0.52
 * Revision 2.9.0.9  1994/05/10  22:08:26  * Major addn of comments, bugfixes
 * Revision 2.9.0.8  1994/05/10  16:15:55  * Backup before I hack code. :-)
 * Revision 2.9.0.7  1994/04/09  14:48:57  * Bug with initialization fixed.
 * Revision 2.9.0.6  1994/04/08  00:19:58  * Added xmit timeout for 8250 mode.
 *                                           Improved startup serial init.
 * Revision 2.9.0.5  1994/04/07  06:42:18  * This got into pre0.51 release 4.
 * Revision 2.9.0.4  1994/04/06  14:49:25  * Cleanup of RCS stuff.
 * Revision 2.9.0.3  1994/04/06  14:48:30  * More accurate bootup condition
 * Revision 2.9.0.2  1994/04/06  13:12:55  * Speed optimization, static decls
 * Revision 2.9.0.1  1994/04/06  12:56:02  * Fix minor bug with IRQ config
 */

/**************************** DECLARATIONS *******************************/

/* 
 * A SUPER_DBG level of 0 is for simple debugging.
 * A SUPER_DBG level of 1 means plenty of debug output on some of the
 *  most critical information.
 * A SUPER_DBG level of 2 means really-heavy debug output, including all
 *  port reads and writes, and every character received and transmitted!
 * A SUPER_DBG level of 3 adds even more debugging messages, those related 
 *  to flagging for serial interrupts, or PIC-driven code.
 *
 * You must recompile everytime this constant is modified.
 */
#define SUPER_DBG 0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <pwd.h>

#include "config.h"
#include "cpu.h"
#include "emu.h"
#include "dosio.h"
#include "serial.h"
#include "mouse.h"
#include "bitops.h"
#include "pic.h"

serial_t com[MAX_SER];

/* The following are positive constants that adjust the soonness of the next
 * receive or transmit interrupt in FIFO mode.  These are a little
 * bit sensitive, and may dissappear when better timer code arrives.
 */
#define TIMEOUT_TX   2
#define TIMEOUT_RX   3

/* Maximum number of 115200ths of a seconds worth of chars to buffer
 * If more characters than this is in the buffer, then wait till it
 * drops below this value, to trigger the next transmit interrupt
 * Right now this is set to 1/10th of a second.
 */
#define MAX_TX_BUF_TICKS        11520L

static int get_msr(int num);
static void transmit_engine(int num);
static void receive_engine(int num);
static void interrupt_engine(int num);
static void serial_timer_update(void);
void pic_serial_run(void);

static int tty_already_locked(char *nam);
static int tty_lock(char *path, int mode);

/* Some very useful functions, mainly for Modem Status register operation.
 * CONVERT_BIT returns 'outbit' only if 'inbit' in 'testbyte' is set.
 * DELTA_BIT   returns 'outbit' if 'inbit' changed between bytes 'old' and 'new'
 * TRAIL_EDGE  returns 'outbit' if 'inbit' is on in 'old' and off in 'new'
 */
static inline int CONVERT_BIT(int testbyte, int inbit, int outbit)
{
  if (testbyte & inbit) return outbit;
  return 0;
}

static inline int DELTA_BIT(int old, int new, int inbit, int output)
{
  if((old ^ new) & inbit) return output;
  return 0;
}

static inline int TRAIL_EDGE(int old, int new, int inbit, int outbit)
{
  if((old & inbit) > (new  & inbit)) return outbit;
  return 0;
}
 
 
/*************************************************************************/
/*                 MISCALLENOUS serial support functions                 */
/*************************************************************************/

/* This function flushes the internal unix receive buffer [num = port] */
static void
rx_buffer_dump(int num)
{
  tcflush(com[num].fd,TCIFLUSH);
}


/* This function flushes the internal unix transmit buffer [num = port] */
static void
tx_buffer_dump(int num)
{
  tcflush(com[num].fd,TCOFLUSH);
}


/* This function checks for newly received data and fills the UART
 * FIFO (16550 mode) or receive register (16450 mode).  [num = port]
 */
static void
uart_fill(int num)
{
  static u_char bytes[RX_FIFO_SIZE];
  static int i,size;
  
  if (com[num].MCR & UART_MCR_LOOP) return;	/* Return if loopback */
  
  if (com[num].fifo_enable) {
    /* The FIFO must be dequeued in here!  The following section is coded 
     * for maximum speed efficiency instead of size and logic.
     */
    if (com[num].RX_FIFO_BYTES == RX_FIFO_SIZE) {
      com[num].uart_full = 1;
    }
    else {
      /* Do a block read up to the amount of empty space left in FIFO */
      size = RPT_SYSCALL(read(com[num].fd, bytes, (RX_FIFO_SIZE - com[num].RX_FIFO_BYTES)));
 
      if (size > 0) {			/* Note that size is -1 if error */
        com[num].rx_timeout = TIMEOUT_RX;	/* Reset timeout counter */
 
        for (i = 0; i < size; i++) {
          com[num].RX_FIFO_BYTES++;
          com[num].RX_FIFO[com[num].RX_FIFO_END] = bytes[i];
          com[num].RX_FIFO_END = (com[num].RX_FIFO_END +1) & (RX_FIFO_SIZE -1);
        }
 
        com[num].uart_full = (com[num].RX_FIFO_BYTES == RX_FIFO_SIZE);
        com[num].LSR |= UART_LSR_DR;		/* Set recv data ready bit */
 
        if (com[num].RX_FIFO_BYTES >= com[num].RX_FIFO_TRIGGER) {
          com[num].int_type |= RX_INTR;		/* flag receive interrupt */
          interrupt_engine(num);		/* Do next interrupt */
        }
      }
      else if (com[num].RX_FIFO_BYTES) {
        com[num].LSR |= UART_LSR_DR;		/* Set recv data ready bit */
      }
    }
  }
 
  else if (!(com[num].LSR & UART_LSR_DR)) {
    /* Normal mode.  Don't overwrite if data is already waiting. */
    /* Now Copy one byte to RX register.                         */
    if (RPT_SYSCALL(read(com[num].fd,bytes,1)) > 0) {
      com[num].LSR |= UART_LSR_DR;	/* Set received data ready bit */
      com[num].RX = bytes[0];		/* Put byte into RBR */
      com[num].uart_full = 1;		/* RBR is occupied */
      com[num].int_type |= RX_INTR;	/* flag receive interrupt */
      interrupt_engine(num);		/* Do next interrupt */
    }
    else {
      com[num].uart_full = 0;
    }
  }
}


/* This function clears the specified XMIT and/or RCVR FIFO's.  This is
 * done on initialization, and when changing from 16450 to 16550 mode, and
 * vice versa.  [num = port, fifo = flags to indicate which fifos to clear]
 */ 
static void 
uart_clear_fifo(int num, int fifo)
{
  /* Should clearing a UART cause a THRE interrupt if it's enabled? XXXXXX */
  if (fifo & UART_FCR_CLEAR_RCVR) {	/* Clear receive FIFO */
    /* Preserve THR empty state, clear error bits and recv data ready bit */
    com[num].LSR &= ~(UART_LSR_ERR | UART_LSR_DR);
    com[num].RX_FIFO_START = 0;		/* Beginning of rec FIFO queue */
    com[num].RX_FIFO_END = 0;		/* End of rec FIFO queue */
    com[num].RX_FIFO_BYTES = 0;		/* Number of bytes in rec FIFO queue */
    com[num].uart_full = 0;		/* UART is now empty */
    com[num].rx_timeout = 0;		/* Receive intr already occured */
    rx_buffer_dump(num);		/* Clear receive buffer */
    com[num].int_type &= ~(LS_INTR | RX_INTR);	/* Clear LS and RX intrs */
    if (((com[num].IIR & UART_IIR_ID) == UART_IIR_RDI) ||
        ((com[num].IIR & UART_IIR_ID) == UART_IIR_RLSI))
    {
      com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
    }
  }
  if (fifo & UART_FCR_CLEAR_XMIT) {	/* Clear transmit FIFO */
    /* Preserve recv data ready bit and error bits, and set THR empty */
    com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;
    com[num].TX_FIFO_START = 0;		/* Start of xmit FIFO queue */
    com[num].TX_FIFO_END = 0;		/* End of xmit FIFO queue */
    com[num].TX_FIFO_BYTES = 0;		/* Number of bytes in xmit FIFO */
    com[num].tx_trigger = 0;            /* Transmit intr already occured */
    com[num].tx_overflow = 0;		/* Not in overflow state */
    com[num].int_type &= ~TX_INTR;	/* Clear TX intr */
    tx_buffer_dump(num);		/* Clear transmit buffer */
    if ((com[num].IIR & UART_IIR_ID) == UART_IIR_THRI) {
      com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
    }
  }
}


/* This function updates the line settings of a serial line (parity,
 * stop bits, word size, and baudrate) to conform to the value
 * stored in the Line Control Register (com[].LCR) and the Baudrate
 * Divisor Latch Registers (com[].dlm and com[].dll)     [num = port]
 */
static void
ser_termios(int num)
{
  speed_t baud;
  long int rounddiv;
  
  /* The following is the same as (com[num].dlm * 256) + com[num].dll */
  #define DIVISOR ((com[num].dlm << 8) | com[num].dll)

  /* Return if not a tty */
  if (tcgetattr(com[num].fd, &com[num].newset) == -1) {
    #if SUPER_DBG > 0
      s_printf("SER%d: Line Control: NOT A TTY.\n",num);
    #endif
    return;
  }
  s_printf("SER%d: LCR = 0x%x, ",num,com[num].LCR);

  /* Set the word size */
  com[num].newset.c_cflag &= ~CSIZE;
  switch (com[num].LCR & UART_LCR_WLEN8) {
  case UART_LCR_WLEN5:
    com[num].newset.c_cflag |= CS5;
    s_printf("5");
    break;
  case UART_LCR_WLEN6:
    com[num].newset.c_cflag |= CS6;
    s_printf("6");
    break;
  case UART_LCR_WLEN7:
    com[num].newset.c_cflag |= CS7;
    s_printf("7");
    break;
  case UART_LCR_WLEN8:
    com[num].newset.c_cflag |= CS8;
    s_printf("8");
    break;
  }

  /* Set the parity.  Rarely-used MARK and SPACE parities not supported yet */
  if (com[num].LCR & UART_LCR_PARITY) {
    com[num].newset.c_cflag |= PARENB;
    if (com[num].LCR & UART_LCR_EPAR) {
      com[num].newset.c_cflag &= ~PARODD;
      s_printf("E");
    }
    else {
      com[num].newset.c_cflag |= PARODD;
      s_printf("O");
    }
  }
  else {
    com[num].newset.c_cflag &= ~PARENB;
    s_printf("N");
  }

  /* Set the stop bits: UART_LCR_STOP set means 2 stop bits, 1 otherwise */
  if (com[num].LCR & UART_LCR_STOP) {
    /* This is actually 1.5 stop bit when word size is 5 bits */
    com[num].newset.c_cflag |= CSTOPB;
    s_printf("2, ");
  }
  else {
    com[num].newset.c_cflag &= ~CSTOPB;
    s_printf("1, ");
  }

  /* Linux 1.1.65 and above supports 115200 and 57600 directly, while
   * Linux 1.1.64 and below do not support them.  For these kernels, make 
   * B115200 and B57600 equal to B38400.  These defines also may be 
   * important if DOSEMU is ported to other nix-like operating systems.
   */
  #ifndef B115200
  #define B115200 B38400
  #endif
  #ifndef B57600
  #define B57600 B38400
  #endif
                       
  /* The following sets the baudrate.  These nested IF statements rounds
   * upwards to the next higher baudrate. (ie, rounds downwards to the next
   * valid divisor value) The formula is:  bps = 1843200 / (divisor * 16)
   */
  if ((DIVISOR < DIV_57600) && DIVISOR) {               /* 115200 bps */
    s_printf("bps = 115200, ");
    rounddiv = DIV_115200;
    baud = B115200;
  } else if (DIVISOR < DIV_38400) {                     /* 57600 bps */
    s_printf("bps = 57600, ");
    rounddiv = DIV_57600;
    baud = B57600;
  } else if (DIVISOR < DIV_19200) {			/* 38400 bps */
    s_printf("bps = 38400, ");
    rounddiv = DIV_38400;
    baud = B38400;
  } else if (DIVISOR < DIV_9600) {			/* 19200 bps */
    s_printf("bps = 19200, ");
    rounddiv = DIV_19200;
    baud = B19200;
  } else if (DIVISOR < DIV_4800) {			/* 9600 bps */
    s_printf("bps = 9600, ");
    rounddiv = DIV_9600;
    baud = B9600;
  } else if (DIVISOR < DIV_2400) {			/* 4800 bps */
    s_printf("bps = 4800, ");
    rounddiv = DIV_4800;
    baud = B4800;
  } else if (DIVISOR < DIV_1800) {			/* 2400 bps */
    s_printf("bps = 2400, ");
    rounddiv = DIV_2400;
    baud = B2400;
  } else if (DIVISOR < DIV_1200) {			/* 1800 bps */
    s_printf("bps = 1800, ");
    rounddiv = DIV_1800;
    baud = B1800;
  } else if (DIVISOR < DIV_600) {			/* 1200 bps */
    s_printf("bps = 1200, ");
    rounddiv = DIV_1200;
    baud = B1200;
  } else if (DIVISOR < DIV_300) {			/* 600 bps */
    s_printf("bps = 600, ");
    rounddiv = DIV_600;
    baud = B600;
  } else if (DIVISOR < DIV_150) {			/* 300 bps */
    s_printf("bps = 300, ");
    rounddiv = DIV_300;
    baud = B300;
  } else if (DIVISOR < DIV_110) {			/* 150 bps */
    s_printf("bps = 150, ");
    rounddiv = DIV_150;
    baud = B150;
  } else if (DIVISOR < DIV_50) {			/* 110 bps */
    s_printf("bps = 110, ");
    rounddiv = DIV_110;
    baud = B110;
  } else {						/* 50 bps */
    s_printf("bps = 50, ");
    rounddiv = DIV_50;
    baud = B50;
  }
  s_printf("divisor 0x%x -> 0x%x\n", DIVISOR, rounddiv);

  /* Number of 115200ths of a second for each char to transmit,
   * This assumes 10 bits per characters, although math can easily
   * be done here to accomodate the parity and character size.
   * Remember: One bit takes exactly (rounddiv / 115200)ths of a second.
   */
  com[num].char_time = rounddiv * 10;
  
  /* Save the newbaudrate value */
  com[num].newbaud = baud;

  /* These are required to increase chances that 57600/115200 will work. */
  /* At least, these were needed on my system, Linux 1.2 and Libc 4.6.27 */
  com[num].newset.c_cflag &= ~CBAUD;
  com[num].newset.c_cflag |= baud;
  
  /* The following does the actual system calls to set the line parameters */
  cfsetispeed(&com[num].newset, baud);
  cfsetospeed(&com[num].newset, baud);
  tcsetattr(com[num].fd, TCSANOW, &com[num].newset);
  
  /* Many mouse drivers require this, they detect for Framing Errors
   * coming from the mouse, during initialization, usually right after
   * the LCR register is set, so this is why this line of code is here
   */
  if (com[num].mouse) {
    com[num].LSR |= UART_LSR_FE; 		/* Set framing error */
    com[num].int_type |= LS_INTR;		/* Flag for RLSI */
    interrupt_engine(num);			/* Do next interrupt */
  }
}


/*  Determines if the tty is already locked.  Stolen from uri-dip-3.3.7k
 *  Nice work Uri Blumenthal & Ian Lance Taylor!
 *  [nam = complete path to lock file, return = nonzero if locked]
 */
static int
tty_already_locked(char *nam)
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
static int
tty_lock(char *path, int mode)
{
  char saved_path[PATH_MAX];
  char dev_nam[20];
  struct passwd *pw;
  pid_t ime;
  int cwrote;

  /* Check that lockfiles can be created! */
  if((mode == 1 || mode == 2) && geteuid() != (uid_t)0) {
    s_printf("DOSEMU: Need to be suid root to create Lock Files!\n"
	     "        Serial port on %s not configured!\n", path);
    error("\nDOSEMU: Need to be suid root to create Lock Files!\n"
	  "        Serial port on %s not configured!\n", path);
    return(-1);
  }

  bzero(dev_nam, sizeof(dev_nam));
  sprintf(saved_path, "%s/%s%s", config.tty_lockdir, config.tty_lockfile, 
         (strrchr(path, '/')+1));
  strcpy(dev_nam, path);
  
  if (mode == 1) {      /* lock */
    if (path == NULL) return(0);        /* standard input */
    {
      FILE *fd;
      if (tty_already_locked(saved_path) == 1) {
        s_printf("DOSEMU: attempt to use already locked tty %s\n", saved_path);
        error("\nDOSEMU: attempt to use already locked tty %s\n", saved_path);
        return (-1);
      }
      if ((fd = fopen(saved_path, "w")) == (FILE *)0) {
        s_printf("DOSEMU: lock: (%s): %s\n", saved_path, strerror(errno));
        error("\nDOSEMU: tty: lock: (%s): %s\n", saved_path, strerror(errno));
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
      error("\nDOSEMU: tty: lock: UUCP user %s unknown!\n", OWNER_LOCKS);
      return(0);        /* keep the lock anyway */
    }
    
    (void) chown(saved_path, pw->pw_uid, pw->pw_gid);
    (void) chmod(saved_path, 0644);
  } 
  else if (mode == 2) { /* re-acquire a lock after a fork() */
    FILE *fd;

    if ((fd = fopen(saved_path, "w")) == (FILE *)0) {
      s_printf("DOSEMU:tty_lock(%s) reaquire: %s\n", 
              saved_path, strerror(errno));
      error("\nDOSEMU:tty_lock: reacquire (%s): %s\n",
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
    (void) chown(saved_path, getuid(), getgid());
    return(0);
  } 
  else {    /* unlock */
    FILE *fd;

    if ((fd = fopen(saved_path, "w")) == (FILE *)0) {
      s_printf("DOSEMU:tty_lock: can't reopen to delete: %s\n",
             strerror(errno));
      return (-1);
    }
      
    if (unlink(saved_path) < 0) {
      s_printf("DOSEMU: tty: unlock: (%s): %s\n", saved_path,
             strerror(errno));
      error("\nDOSEMU: tty: unlock: (%s): %s\n", saved_path,
             strerror(errno));
      return(-1);
    }
  }
  return(0);
}


/* This function opens the serial port for DOSEMU.  Normally called only
 * by do_ser_init below.   [num = port, return = file descriptor]
 */
static int
ser_open(int num)
{
  s_printf("SER%d: Running ser_open, fd=%d\n",num, com[num].fd);
  
  if (com[num].fd != -1) return (com[num].fd);
  
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
    com[num].fd = -1;
    return(-1);
  }
  
  if (com[num].dev[0] == 0) {
    s_printf("SER%d: Device file not yet defined!\n",num);
    return (-1);
  }
  
  com[num].fd = RPT_SYSCALL(open(com[num].dev, O_RDWR | O_NONBLOCK));
  RPT_SYSCALL(tcgetattr(com[num].fd, &com[num].oldset));
  return (com[num].fd);
}


/* This function closes the serial port for DOSEMU.  Normally called 
 * only by do_ser_init below.   [num = port, return = file error code]
 */
static int
ser_close(int num)
{
  static int i;
  s_printf("SER%d: Running ser_close\n",num);
  uart_clear_fifo(num,UART_FCR_CLEAR_CMD);
  
  /* save current dosemu settings of the file and restore the old settings
   * before closing the file down. 
   */
  RPT_SYSCALL(tcgetattr(com[num].fd, &com[num].newset));
  RPT_SYSCALL(tcsetattr(com[num].fd, TCSANOW, &com[num].oldset));
  i = RPT_SYSCALL(close(com[num].fd));
  com[num].fd = -1;
  
  /* Clear the lockfile from DOSEMU */
  if (com[num].dev_locked) {
    if (tty_lock(com[num].dev, 0) >= 0) 
      com[num].dev_locked = FALSE;
  }
  return (i);
}


/* The following function is the main initialization routine.  This 
 * routine is called only by do_ser_init, which is the master routine.
 * What this does is to set up the environment, define default variables, 
 * and the emulated UART's initialization state, and open/initialize the 
 * serial line.   [num = port]
 */
static void
do_ser_init(int num)
{
  int data = 0;
  int i;
  
  /* The following section sets up default com port, interrupt, base
  ** port address, and device path if they are undefined. The defaults are:
  **
  **   COM1:   irq = 4    base_port = 0x3F8    device = /dev/cua0
  **   COM2:   irq = 3    base_port = 0x2F8    device = /dev/cua1
  **   COM3:   irq = 4    base_port = 0x3E8    device = /dev/cua2
  **   COM4:   irq = 3    base_port = 0x2E8    device = /dev/cua3
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
    case 4:  strcpy(com[num].dev, "/dev/cua3"); break;
    case 3:  strcpy(com[num].dev, "/dev/cua2"); break;
    case 2:  strcpy(com[num].dev, "/dev/cua1"); break;
    default: strcpy(com[num].dev, "/dev/cua0"); break;
    }
  }
 
  /* convert irq number to pic_ilevel number and set up interrupt
   * if irq is invalid, no interrupt will be assigned 
   */
  if(com[num].interrupt < 16) {
    com[num].interrupt = pic_irq_list[com[num].interrupt];
    s_printf("SER%d: enabling interrupt %d\n", num, com[num].interrupt);
    pic_seti(com[num].interrupt,pic_serial_run,0);
    pic_unmaski(com[num].interrupt);
  }
  irq_source_num[com[num].interrupt] = num;	/* map interrupt to port */

  /*** The following is where the real initialization begins ***/

  /* Information about serial port added to debug file */
  s_printf("SER%d: COM%d, intlevel=%d, base=0x%x\n", 
        num, com[num].real_comport, com[num].interrupt, com[num].base_port);

  /* Write serial port information into BIOS data area 0040:0000
   * This is for DOS and many programs to recognize ports automatically
   */
  if ((com[num].real_comport >= 1) && (com[num].real_comport <= 4)) {
    *((u_short *) (0x400) + (com[num].real_comport-1)) = com[num].base_port;

    /* Debugging to determine whether memory location was written properly */
    s_printf("SER%d: BIOS memory location 0x%x has value of 0x%x\n", num,
	(int)((u_short *) (0x400) + (com[num].real_comport-1)), 
        *((u_short *) (0x400) + (com[num].real_comport-1)) );
  }

  /* first call to serial timer update func to initialize the timer */
  /* value, before the com[num] structure is initialized */
  serial_timer_update();

  /* Set file descriptor as unused, then attempt to open serial port */
  com[num].fd = -1;
  ser_open(num);
  
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
  com[num].newset.c_line = 0;
  com[num].newset.c_cc[VMIN] = 1;
  com[num].newset.c_cc[VTIME] = 0;
  tcsetattr(com[num].fd, TCSANOW, &com[num].newset);

  com[num].dll = 0x30;			/* Baudrate divisor LSB: 2400bps */
  com[num].dlm = 0;			/* Baudrate divisor MSB: 2400bps */
  com[num].char_time = DIV_2400 * 10;	/* 115200ths of second per char */
  com[num].TX = 0;			/* Transmit Holding Register */
  com[num].RX = 0;			/* Received Byte Register */
  com[num].IER = 0;			/* Interrupt Enable Register */
  com[num].IIR = UART_IIR_NO_INT;	/* Interrupt I.D. Register */
  com[num].LCR = UART_LCR_WLEN8;	/* Line Control Register: 5N1 */
  com[num].DLAB = 0;			/* DLAB for baudrate change */
  com[num].FCReg = 0; 			/* FIFO Control Register */
  com[num].RX_FIFO_TRIGGER = 1;		/* Receive FIFO trigger level */
  com[num].MCR = 0;			/* Modem Control Register */
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;   /* Txmit Hold Reg Empty */
  com[num].MSR = 0;			/* Modem Status Register */
  com[num].SCR = 0; 			/* Scratch Register */
  com[num].int_enab = 0;		/* FLAG: Interrupts disabled */
  com[num].int_pend = 0;		/* FLAG: No interrupts pending */
  com[num].int_type = TX_INTR;		/* FLAG: THRE interrupt waiting */
  com[num].uart_full = 0;		/* FLAG: UART full flag */
  com[num].fifo_enable = 0;		/* FLAG: FIFO enabled */
  com[num].tx_timer = 0;		/* Transmit timer */
  com[num].tx_trigger = 0;		/* FLAG: Dont start more xmit ints */
  com[num].rx_timeout = TIMEOUT_RX;	/* FLAG: Receive timeout */
  com[num].tx_overflow = 0;		/* FLAG: Outgoing buffer overflow */
  uart_clear_fifo(num,UART_FCR_CLEAR_CMD);	/* Initialize FIFOs */

  #if SUPER_DBG > 1
    s_printf("SER%d: do_ser_init: running ser_termios\n",num);
  #endif
  ser_termios(num);			/* Set line settings now */
  com[num].MSR = get_msr(num);		/* Get current modem status */

  /* Pull down DTR.  This is the most natural for most communications */
  /* devices, including mice so that DTR rises during mouse init.     */
  ioctl(com[num].fd, TIOCMSET, &data);
}


/* 
 * DANG_BEGIN_FUNCTION serial_init
 * 
 * description:
 * The following is the master serial initialization function called 
 * externally (from this serial.c module) during DOSEMU startup. It
 * initializes all the configured serial ports.
 *
 * DANG_END_FUNCTION
 */
void
serial_init(void)
{
  int i;
  warn("SERIAL $Header: /home/src/dosemu0.60/dosemu/RCS/serial.c,v 2.9 1995/02/25 22:38:01 root Exp root $\n");
  s_printf("SER: Running serial_init, %d serial ports\n", config.num_ser);

  /* Clean the BIOS data area at 0040:0000 for serial ports */
  *(u_short *) 0x400 = 0;
  *(u_short *) 0x402 = 0;
  *(u_short *) 0x404 = 0;
  *(u_short *) 0x406 = 0;

  /* Do UART init here - Need to set up registers and init the lines. */
  for (i = 0; i < config.num_ser; i++) {
    com[i].fd = -1;
    com[i].dev_locked = FALSE;
    
    /* Only initialize port if:
     * - Not using a mouse on the port.  
     * - If using mouse, only initialize port for using the mouse directly 
     *    if at the console and not running in an Xwindow (since in Xwindows,
     *    dosemu will use Xwindows mouse events instead of direct access
     * - not init if using mouse, but not in a console.
     */
    if ( (!com[i].mouse) || (!(config.usesX || !config.console))) do_ser_init(i);
  }
}

/* Like serial_init, this is the master function that is called externally,
 * but at the end, when the user quits DOSEMU.  It deinitializes all the
 * configured serial ports.
 */
void
serial_close(void)
{
  static int i;
  s_printf("SER: Running serial_close\n");
  for (i = 0; i < config.num_ser; i++) {
    if ( ( !com[i].mouse) || (!(config.usesX || !config.console))) {
      RPT_SYSCALL(tcsetattr(com[i].fd, TCSANOW, &com[i].oldset));
      ser_close(i);
    }
  }
}

/* The following de-initializes the mouse on the serial port that the mouse
 * has been enabled on.  For mouse sharing purposes, this is the function
 * that is called when the user switches out of the VC running DOSEMU.
 * (Also, this silly function name needs to be changed soon.)
 */
void
child_close_mouse(void)
{
  static u_char i, rtrn;
  if (!(config.usesX || !config.console)) {
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
void
child_open_mouse(void)
{
  static u_char i;
  if (!(config.usesX || !config.console)) {
    s_printf("MOUSE: OPEN function starting.\n");
    for (i = 0; i < config.num_ser; i++) {
      s_printf("MOUSE: OPEN port=%d, type=%d, dev=%s, valid=%d\n",
                i, mice->type, com[i].dev, com[i].mouse);
      if (com[i].mouse == TRUE) {
        s_printf("MOUSE: OPEN port=%d: Running ser-open.\n", i);
        com[i].fd = -1;
        ser_open(i);
        tcgetattr(com[i].fd, &com[i].newset);
      }
    }
  }
}


/*************************************************************************/
/*                      RECEIVE handling functions                       */
/*************************************************************************/

/* This function returns a received character.  
 * The character comes from the RBR register (or FIFO), which would have 
 * been previously filled by the uart_fill routine (which does the 
 * actual reads from the serial line) or loopback code.  This function
 * usually runs through the do_serial_in routine.    [num = port]
 */
static int
get_rx(int num)
{
  static int val;
  com[num].rx_timeout = TIMEOUT_RX;		/* Reset timeout counter */
  if (com[num].fifo_enable) {
    /* if there are data in fifo then read from from the fifo.
    ** if the fifo is empty, read last byte received.  This part is 
    ** supposed optimized for speed/compatibility, not compactness/simplicity.
    */
    if (com[num].RX_FIFO_BYTES) {		/* Is there data in fifo? */
      /* Get byte from fifo */
      val = com[num].RX_FIFO[com[num].RX_FIFO_START];
      com[num].RX_FIFO_START = (com[num].RX_FIFO_START +1) & (RX_FIFO_SIZE -1);
      com[num].RX_FIFO_BYTES--;
 
      /* if fifo empty, turn off data-ready flag, and clear interrupt */
      if (!com[num].RX_FIFO_BYTES) {
        com[num].int_type &= ~RX_INTR;		/* No interrupt needed */
        com[num].LSR &= ~UART_LSR_DR;		/* No data waiting */
        if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RDI) {
          com[num].IIR = UART_IIR_FIFO | UART_IIR_NO_INT;    /* clear int */
          interrupt_engine(num);		/* Do next interrupt */
        }
      }
      else if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RDI) {
        if (com[num].RX_FIFO_BYTES < com[num].RX_FIFO_TRIGGER) {
          com[num].int_type &= ~RX_INTR;	/* No interrupt needed */
          com[num].IIR = UART_IIR_FIFO | UART_IIR_NO_INT;    /* clear int */
          interrupt_engine(num);		/* Do next interrupt */
        }
        else {
          /* Clear timeout bit */
          com[num].IIR = (com[num].IIR & UART_IIR_ID) | UART_IIR_FIFO;
        }
      }
    }
    else {
      if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RDI) {
        com[num].IIR = UART_IIR_FIFO | UART_IIR_NO_INT;    /* clear int */
        interrupt_engine(num);			/* Do next interrupt */
      }
      val = 0;				/* Return a 0 for empty FIFO */
      #if SUPER_DBG > 1
        s_printf("SER%d: Reading from empty FIFO\n",num);
      #endif
    } 
    return (val);	/* Return the byte */
  }
  else {		/* non-FIFO mode */
    com[num].LSR &= ~UART_LSR_DR;		/* No data waiting */
    com[num].int_type &= ~RX_INTR;		/* No interrupt needed */
    if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RDI) {
      com[num].IIR = UART_IIR_NO_INT;		/* Clear RDI interrupt */
      interrupt_engine(num);			/* Do next interrupt */
    }
    return com[num].RX;
  }
}


/* The following function is pre-interrupt initialization called right
 * before executing the Received Data Ready ISR (Interrupt Service Routine).
 * [intnum = software interrupt number]
 */
static int
beg_rxint(int intnum)
{
  static u_char num;
  num = irq_source_num[intnum];
  com[num].int_type &= ~RX_INTR;	/* No more interrupt needed */
  com[num].rx_timeout = 0;		/* 0 means int has occured */
  if (!com[num].fifo_enable)
    com[num].IIR = UART_IIR_RDI;
  else if (com[num].RX_FIFO_BYTES >= com[num].RX_FIFO_TRIGGER)
    com[num].IIR = (com[num].IIR & ~0x7) | UART_IIR_RDI;
  else
    com[num].IIR = UART_IIR_CTI | UART_IIR_FIFO;
  #if SUPER_DBG > 1
    s_printf("SER%d: ---BEGIN---Receive Data Ready Interrupt\n",num);
  #endif
  return 0;
}


/* This function is the post-interrupt initialization called right after
 * the Received Data Ready ISR.   [intnum = software interrupt number]
 */ 
static int
end_rxint(int intnum)
{
  static u_char num;
  num = irq_source_num[intnum];
  #if SUPER_DBG > 1
    s_printf("SER%d: ---END---Receive Data Ready Interrupt\n",num);
  #endif
  com[num].int_pend = 0;	/* No longer pending interrupt */
  receive_engine(num);		/* Warning: Causes some stack overflows */
  transmit_engine(num);
  interrupt_engine(num);	/* Do next interrupt */
  return 0;
}


/*************************************************************************/
/*                    MODEM STATUS handling functions                    */
/*************************************************************************/

/* This function computes delta into bits 0-3, of the changed state of
 * bits 4-7 compared between two sets of MSR values.  (For example delta
 * bit 0 is set if bit number 4 are different between the two sets of values)
 * One of the bit is actually a trailing edge bit (for the ring indicator)
 * instead of a delta bit.    [oldmsr = old value, newmsr = new value]
 */
static inline int
msr_delta(int oldmsr, int newmsr)
{
  /* There were some errors (compiler bug?  Formula too complex?) when I tried
  ** to put all four DELTA_BIT & TRAIL_EDGE in the same variable assignment.
  */
  int delta;

  delta  = DELTA_BIT(oldmsr, newmsr, UART_MSR_CTS, UART_MSR_DCTS);
  delta |= DELTA_BIT(oldmsr, newmsr, UART_MSR_DSR, UART_MSR_DDSR);
  delta |= TRAIL_EDGE(oldmsr, newmsr, UART_MSR_RI, UART_MSR_TERI);
  delta |= DELTA_BIT(oldmsr, newmsr, UART_MSR_DCD, UART_MSR_DDCD);
  return delta;
}


/* This function returns the value in the MSR (Modem Status Register)
 * and does the expected UART operation whenever the MSR is read.
 * [num = port]
 */
static int
get_msr(int num)
{
  int val;

  val = com[num].MSR;				/* Save old MSR value */
  com[num].MSR &= UART_MSR_STAT; 		/* Clear delta bits */
  com[num].int_type &= ~MS_INTR;		/* No MSI needed anymore */
  if ((com[num].IIR & UART_IIR_ID) == UART_IIR_MSI) {	/* Is MSI flagged? */
    /* Clear MSI and do next interrupt, if any. */
    com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
    interrupt_engine(num);
  }
  return val;					/* Return MSR value */
}


/* This function is the pre-interrupt initialization called right before
 * the Modem Status ISR.   [intnum = software interrupt number]
 */ 
static int
beg_msi(int intnum)
{
  u_char num;

  num = irq_source_num[intnum];		/* port number that caused int */
  com[num].int_type &= ~MS_INTR;	/* Prevent further MSI occurances */
  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_MSI;
  #if SUPER_DBG > 1
    s_printf("SER%d: ---BEGIN---Modem Status Interrupt\n",num);
  #endif
  return 0;
}


/* This function is the post-interrupt initialization called right after
 * the Modem Status ISR.   [intnum = software interrupt number]
 */ 
static int
end_msi(int intnum)
{
  u_char num;

  num = irq_source_num[intnum];		/* Port that had caused int */
  com[num].int_type &= ~MS_INTR;	/* MSI interrupt finished. */
  #if SUPER_DBG > 1
    s_printf("SER%d: ---END---Modem Status Interrupt\n",num);
  #endif
  com[num].int_pend = 0;		/* interrupt no longer pending */
  interrupt_engine(num);		/* Do next interrupt */
  return 0;
}


/*************************************************************************/
/*                     LINE STATUS handling functions                    */
/*************************************************************************/

/* This function returns the value in the LSR (Line Status Register)
 * and does the expected UART operation whenever the LSR is read.
 * [num = port]
 */
static int
get_lsr(int num)
{
  static int val;

  val = com[num].LSR;			/* Save old LSR value */
  com[num].int_type &= ~LS_INTR;	/* Clear line stat int flag */
  com[num].LSR &= ~UART_LSR_ERR;	/* Clear error bits */
  if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RLSI) {  /* Is RLSI flagged? */
    /* Clear the RLSI flag and do next interrupt, if any. */
    com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
    interrupt_engine(num);
  }
  
  return (val);                         /* Return LSR value */
}


/* This function is the pre-interrupt initialization called right before
 * the Receiver Line Status ISR.   [intnum = software interrupt number]
 */ 
static int
beg_lsi(int intnum)
{
  u_char num;

  num = irq_source_num[intnum];		/* Port that caused interrupt */
  com[num].int_type &= ~LS_INTR;	/* No more interrupt needed */
  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_RLSI;
  #if SUPER_DBG > 1
    s_printf("SER%d: ---START---Line Status Interrupt\n",num);
  #endif
  return 0;
}


/* This function is the post-interrupt initialization called right after
 * the Receiver Line Status ISR.   [intnum = software interrupt number]
 */ 
static int
end_lsi(int intnum)
{
  u_char num;

  num = irq_source_num[intnum];		/* Port that caused interrupt */
  #if SUPER_DBG > 1
    s_printf("SER%d: ---END---Line Status Interrupt\n",num);
  #endif
  com[num].int_pend = 0;		/* Interrupt no longer pending. */
  interrupt_engine(num);		/* Do next interrupt, if any. */
  return 0;
}


/*************************************************************************/
/*                      TRANSMIT handling functions                      */
/*************************************************************************/

/* This function transmits a character.  This function is called mainly
 * through do_serial_out, when the Transmit Register is written to.
 * The end result is that the character is put into the THR or the 
 * transmit FIFO. (or receive fifo if in Loopback test mode)
 * [num = port, val = character to transmit]
 */
static void
put_tx(int num, int val)
{
  static int rtrn;
  if (com[num].DLAB) {		/* If DLAB line set, don't transmit. */
    com[num].dll = val;			/* Simulate write to DLL register */
    return;				/*  (Divisor Latch LSB) */
  }

  /* Update the transmit timer */
  com[num].tx_timer += com[num].char_time;

  com[num].TX = val;			/* Mainly used in overflow cases */
  com[num].tx_trigger = 1;		/* Time to trigger next tx int */
  com[num].int_type &= ~TX_INTR;	/* Clear transmit interrupt */
  com[num].LSR &= ~(UART_LSR_TEMT | UART_LSR_THRE);	/* THR not empty */

  /* Clear Transmit Holding Register interrupt, etc */
  if ((com[num].IIR & UART_IIR_ID) == UART_IIR_THRI) {
    com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
    interrupt_engine(num);		/* Do next interrupt */
  }

  /* Loop-back writes.  Parity is currently not calculated.  No
   * UART diagnostics programs including COMTEST.EXE, that I tried,
   * complained about parity problems during loopback tests anyway!
   * Even some real UART clones don't calculate parity during loopback.
   */
  if (com[num].MCR & UART_MCR_LOOP) {
    com[num].rx_timeout = TIMEOUT_RX;		/* Reset timeout counter */
    switch (com[num].LCR & UART_LCR_WLEN8) {	/* Word size adjustment */
    case UART_LCR_WLEN7:  val &= 0x7f;  break;
    case UART_LCR_WLEN6:  val &= 0x3f;  break;
    case UART_LCR_WLEN5:  val &= 0x1f;  break;
    }
    if (com[num].fifo_enable) {		/* Is it in FIFO mode? */
      if (com[num].RX_FIFO_BYTES == RX_FIFO_SIZE) {	/* Is FIFO full? */
        #if SUPER_DBG > 1
          s_printf("SER%d: Loopback overrun!\n",num);
        #endif
        com[num].LSR |= UART_LSR_OE;		/* Indicate overrun error */
        com[num].int_type |= LS_INTR;		/* Flag for RLSI */
        interrupt_engine(num);			/* Do next interrupt */
      }
      else {					/* FIFO not full */
        com[num].RX_FIFO_BYTES++;		/* Put char into recv FIFO */
        com[num].RX_FIFO[com[num].RX_FIFO_END] = val;
        com[num].RX_FIFO_END = (com[num].RX_FIFO_END +1) & (RX_FIFO_SIZE -1);
        if (com[num].RX_FIFO_BYTES >= com[num].RX_FIFO_TRIGGER) {
          com[num].int_type |= RX_INTR;		/* Flag receive interrupt */
          interrupt_engine(num);		/* Do next interrupt */
        }
      }
      com[num].LSR |= UART_LSR_DR;		/* Flag Data Ready bit */
    }
    else {				/* FIFOs not enabled */
      com[num].RX = val;			/* Overwrite old byte */
      if (com[num].LSR & UART_LSR_DR) {		/* Was data waiting? */
        com[num].LSR |= UART_LSR_OE;		/* Indicate overrun error */
        com[num].int_type |= LS_INTR;		/* Flag for RLSI */
        interrupt_engine(num);			/* Do next interrupt */
      }
      else {
        com[num].LSR |= UART_LSR_DR;		/* Flag Data Ready bit */
        com[num].int_type |= RX_INTR;		/* Flag receive interrupt */
        interrupt_engine(num);			/* Do next interrupt */
      }
    }
    #ifdef 0
      #if SUPER_DBG > 1
        s_printf("SER%d: loopback write!\n",num);
      #endif
    #endif
    return;
  }
  /* Else, not in loopback mode */
  
  if (com[num].fifo_enable) {			/* Is FIFO enabled? */
    if (com[num].TX_FIFO_BYTES == TX_FIFO_SIZE) {	/* Is FIFO full? */
      rtrn = RPT_SYSCALL(write(com[num].fd,&com[num].TX_FIFO[com[num].TX_FIFO_START],1));
      if (rtrn != 1) {				/* Did transmit fail? */
        com[num].tx_overflow = 1;		/* Set overflow flag */
      }
      else {					
        /* Squeeze char into FIFO */
        com[num].TX_FIFO[com[num].TX_FIFO_END] = val;
        
        /* Update FIFO queue pointers */
        com[num].TX_FIFO_END = (com[num].TX_FIFO_END +1) & (TX_FIFO_SIZE-1);
        com[num].TX_FIFO_START = (com[num].TX_FIFO_START+1)&(TX_FIFO_SIZE-1);
        
      }
    } 
    else {					/* FIFO not full */
      com[num].TX_FIFO[com[num].TX_FIFO_END] = val;  /* Put char into FIFO */
      com[num].TX_FIFO_END = (com[num].TX_FIFO_END +1) & (TX_FIFO_SIZE-1);
      com[num].TX_FIFO_BYTES++;
    } 
  } 
  else { 				/* Not in FIFO mode */
    rtrn = RPT_SYSCALL(write(com[num].fd, &val, 1));   /* Attempt char xmit */
    if (rtrn != 1) 				/* Did transmit fail? */
      com[num].tx_overflow = 1; 		/* Set overflow flag */
  }
}


/* This function is the pre-interrupt initialization called right before
 * the Transmit Holding Reg Empty ISR.  [intnum = software interrupt number]
 */ 
static int
beg_txint(int intnum)
{                  
  u_char num;

  num = irq_source_num[intnum];		/* Port that caused interrupt */
  com[num].int_type &= ~TX_INTR;	/* No more interrupt needed */
  com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;  /* Set TEMT/THRE */
  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_THRI;
  #if SUPER_DBG > 1
    s_printf("SER%d: ---BEGIN---Transmit Interrupt\n",num);
  #endif
  return 0;
}


/* This function is the post-interrupt initialization called right after
 * the Transmit Holding Reg Empty ISR.  [intnum = software interrupt number]
 */ 
static int
end_txint(int intnum)
{
  u_char num;

  num = irq_source_num[intnum];		/* Port that caused interrupt */
  #if SUPER_DBG > 1
    s_printf("SER%d: ---END---Transmit Interrupt\n",num);
  #endif
  com[num].int_pend = 0;		/* no longer pending interrupt */
  receive_engine(num);			/* Receive housekeeping */
  transmit_engine(num);			/* Transmit housekeeping */
  interrupt_engine(num);		/* Do next interrupt if any. */
  return 0;
}


/*************************************************************************/
/*            Miscallenous UART REGISTER handling functions              */
/*************************************************************************/

/* This function handles writes to the FCR (FIFO Control Register).
 * [num = port, val = new value to write to FCR]
 */
static void
put_fcr(int num, int val)
{
  val &= 0xcf;				/* bits 4,5 are reserved. */
  /* Bits 1-6 are only programmed when bit 0 is set (FIFO enabled) */
  if (val & UART_FCR_ENABLE_FIFO) {
    com[num].fifo_enable = 1;		/* Enabled FIFO flag */

    /* fifos are reset when we change from 16450 to 16550 mode.*/
    if (!(com[num].FCReg & UART_FCR_ENABLE_FIFO))
      uart_clear_fifo(num, UART_FCR_CLEAR_CMD);

    /* Various flags to indicate that fifos are enabled. */
    com[num].FCReg = (val & ~UART_FCR_TRIGGER_14) | UART_FCR_ENABLE_FIFO;
    com[num].IIR |= UART_IIR_FIFO;

    /* Commands to reset either of the two fifos.  The clear-FIFO bits
     * are disposed right after the FIFO's are cleared, and are not saved.
     */
    if (val & UART_FCR_CLEAR_CMD)
      uart_clear_fifo(num, val & UART_FCR_CLEAR_CMD);

    /* Don't need to emulate RXRDY,TXRDY pins, used for bus-mastering. */
    if (val & UART_FCR_DMA_SELECT) {
      #if SUPER_DBG > 0
        s_printf("SER%d: FCR: Attempt to change RXRDY & TXRDY pin modes\n",num);
      #endif
    }

    /* Assign special variable for trigger value for easy access. */
    switch (val & UART_FCR_TRIGGER_14) {
    case UART_FCR_TRIGGER_1:   com[num].RX_FIFO_TRIGGER = 1;	break;
    case UART_FCR_TRIGGER_4:   com[num].RX_FIFO_TRIGGER = 4;	break;
    case UART_FCR_TRIGGER_8:   com[num].RX_FIFO_TRIGGER = 8;	break;
    case UART_FCR_TRIGGER_14:  com[num].RX_FIFO_TRIGGER = 14;	break;
    }
    #if SUPER_DBG > 1
      s_printf("SER%d: FCR: Trigger Level is %d\n",
      	num,com[num].RX_FIFO_TRIGGER);
    #endif
  }
  else {				/* FIFO are disabled */
    /* If uart was set in 16550 mode, this will reset it back to 16450 mode.
     * If it already was in 16450 mode, there is no harm done.
     */
    com[num].fifo_enable = 0;			/* Disabled FIFO flag */
    com[num].FCReg &= (~UART_FCR_ENABLE_FIFO);	/* Flag FIFO as disabled */
    com[num].IIR &= ~UART_IIR_FIFO;		/* Flag FIFO off in IIR */
    uart_clear_fifo(num, UART_FCR_CLEAR_CMD);	/* Clear FIFO */
  }
}


/* This function handles writes to the LCR (Line Control Register).
 * [num = port, val = new value to write to LCR]
 */
static void
put_lcr(int num, int val)
{
  com[num].LCR = val;			/* Set new LCR value */
  if (val & UART_LCR_DLAB) {		/* Is Baudrate Divisor Latch set? */
    s_printf("SER%d: LCR = 0x%x, DLAB high.\n", num, val);
    com[num].DLAB = 1;			/* Baudrate Divisor Latch flag */
    com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
    /* IIR is cleared here for safety reasons because if a receive/transmit
     * interrupt is currently pending, DOSEMU will crash because it will
     * access the baudrate divisor LSB value instead of the RBR/THR 
     * registers...
     */
  }
  else {
    com[num].DLAB = 0;			/* Baudrate Divisor Latch flag */
    ser_termios(num);			/* Sets new line settings */
  }
}


/* This function handles writes to the MCR (Modem Control Register).
 * [num = port, val = new value to write to MCR]
 */
static void
put_mcr(int num, int val)
{
  static int newmsr, delta;
  static int changed;
  static int control;
  control = 0;
  changed = com[num].MCR ^ val;			/* Bitmask of changed bits */
  com[num].MCR = val & UART_MCR_VALID;		/* Set valid bits for MCR */

  if (val & UART_MCR_LOOP) {		/* Is Loopback Mode set? */
    /* If loopback just enabled, clear FIFO and turn off DTR & RTS on line */
    if (changed & UART_MCR_LOOP) {		/* Was loopback just set? */
      uart_clear_fifo(num,UART_FCR_CLEAR_CMD);	/* Clear FIFO's */
      ioctl(com[num].fd, TIOCMSET, &control);	/* Lower DTR / RTS */
    }

    /* During a UART Loopback test these bits are, Write(Out) => Read(In)
     * MCR Bit 1 RTS => MSR Bit 4 CTS,	MCR Bit 2 OUT1 => MSR Bit 6 RI
     * MCR Bit 0 DTR => MSR Bit 5 DSR,	MCR Bit 3 OUT2 => MSR Bit 7 DCD
     */
    newmsr  = CONVERT_BIT(val,UART_MCR_RTS,UART_MSR_CTS);
    newmsr |= CONVERT_BIT(val,UART_MCR_DTR,UART_MSR_DSR);
    newmsr |= CONVERT_BIT(val,UART_MCR_OUT1,UART_MSR_RI);
    newmsr |= CONVERT_BIT(val,UART_MCR_OUT2,UART_MSR_DCD);

    /* Compute delta bits of MSR */
    delta = msr_delta(com[num].MSR, newmsr);

    /* Update the MSR and its delta bits, not erasing old delta bits!! */
    com[num].MSR = (com[num].MSR & UART_MSR_DELTA) | delta | newmsr;

    /* Set the MSI interrupt flag if loopback changed the modem status */
    if (delta) {
      com[num].int_type |= MS_INTR;		/* No more MSI needed */
      interrupt_engine(num);			/* Do next interrupt if any */
    }
  }
  else {				/* It's not in Loopback Mode */
    /* Was loopback mode just turned off now?  Then reset FIFO */
    if (changed & UART_MCR_LOOP) uart_clear_fifo(num,UART_FCR_CLEAR_CMD);

    /* Set interrupt enable flag according to OUT2 bit in MCR */
    com[num].int_enab = (val & UART_MCR_OUT2);

    /* Set RTS & DTR on serial line if they (or the loopback state) changed */
    if (changed & (UART_MCR_RTS | UART_MCR_DTR | UART_MCR_LOOP)) {
      control = ((val & UART_MCR_RTS) ? TIOCM_RTS : 0)
              | ((val & UART_MCR_DTR) ? TIOCM_DTR : 0);
      ioctl(com[num].fd, TIOCMSET, &control);
      #if SUPER_DBG > 0
        if (changed & UART_MCR_DTR)
          s_printf("SER%d: MCR: DTR -> %d\n",num,(val & UART_MCR_DTR));
        if (changed & UART_MCR_RTS)
          s_printf("SER%d: MCR: RTS -> %d\n",num,(val & UART_MCR_RTS));
      #endif
    }
  }
}


/* This function handles writes to the LSR (Line Status Register).
 * [num = port, val = new value to write to LSR]
 */
static void
put_lsr(int num, int val) 
{
  com[num].LSR = val & 0x1F;			/* Bits 6 to 8 are ignored */
  if (val & UART_LSR_THRE) {
    com[num].int_type |= TX_INTR;		/* Flag for THRI */
    com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;
  }
  else {
    com[num].LSR &= ~(UART_LSR_THRE | UART_LSR_TEMT);	/* Clear THRE state */
    com[num].int_type &= ~TX_INTR;		/* Unflag THRI */
    com[num].tx_trigger = 1;			/* Trigger next xmit int */
  }

  if (val & UART_LSR_DR)			/* Is data ready bit set? */
    com[num].int_type |= RX_INTR;		/* Flag for RDI */
  else
    com[num].int_type &= ~RX_INTR;		/* Unflag RDI */

  if (val & UART_LSR_ERR)			/* Any error bits set? */
    com[num].int_type |= LS_INTR;		/* Flag for RLSI */
  else
    com[num].int_type &= ~LS_INTR;		/* Unflag RLSI */
  interrupt_engine(num); 			/* Do next interrupt if any */
}


/* This function handles writes to the MSR (Modem Status Register).
 * [num = port, val = new value to write to MSR]
 */
static inline void
put_msr(int num, int val)
{
  com[num].MSR = (com[num].MSR & UART_MSR_STAT) | (val & UART_MSR_DELTA);
  if (val & UART_MSR_DELTA)			/* Any delta bits set? */
    com[num].int_type |= MS_INTR;		/* Flag for MSI */
  else
    com[num].int_type &= ~MS_INTR;		/* Unflag MSI */
  interrupt_engine(num);			/* Do next interrupt if any */
}


/*************************************************************************/
/*            PORT I/O to UART registers: INPUT AND OUTPUT               */
/*************************************************************************/

/* The following function returns a value from an I/O port.  The port
 * is an I/O address such as 0x3F8 (the base port address of COM1). 
 * There are 8 I/O addresses for each serial port which ranges from
 * the base port (ie 0x3F8) to the base port plus seven (ie 0x3FF).
 * [num = abritary port number for serial line, address = I/O port address]
 */
int
do_serial_in(int num, int address)
{
  static int val;
  switch (address - com[num].base_port) {
  case UART_RX:		/* Read from Received Byte Register */	
/*case UART_DLL:*/      /* or Read from Baudrate Divisor Latch LSB */
    if (com[num].DLAB) {	/* Is DLAB set? */
      val = com[num].dll;	/* Then read Divisor Latch LSB */
      #if SUPER_DBG > 0
        s_printf("SER%d: Read Divisor LSB = 0x%x\n",num,val);
      #endif
    }
    else {
      val = get_rx(num);	/* Else, read Received Byte Register */
      #if SUPER_DBG > 1
        s_printf("SER%d: Receive 0x%x\n",num,val);
      #endif
    }
    break;

  case UART_IER:	/* Read from Interrupt Enable Register */
/*case UART_DLM:*/      /* or Read from Baudrate Divisor Latch MSB */
    if (com[num].DLAB) {	/* Is DLAB set? */
      val = com[num].dlm;	/* Then read Divisor Latch MSB */
      #if SUPER_DBG > 0
        s_printf("SER%d: Read Divisor MSB = 0x%x\n",num,val);
      #endif
    }
    else {
      val = com[num].IER;	/* Else, read Interrupt Enable Register */
      #if SUPER_DBG > 0
        s_printf("SER%d: Read IER = 0x%x\n",num,val);
      #endif
    }
    break;

  case UART_IIR:	/* Read from Interrupt Identification Register */
    val = com[num].IIR;
    #if SUPER_DBG > 1
      s_printf("SER%d: Read IIR = 0x%x\n",num,val);
    #endif
    if ((com[num].IIR & UART_IIR_ID) == UART_IIR_THRI) {
      com[num].int_type &= ~TX_INTR;		/* Unflag tx interrupt */
      com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
      #if SUPER_DBG > 1
        s_printf("SER%d: Read IIR: THRI INT cleared: 0x%x\n",num,com[num].IIR);
      #endif
      interrupt_engine(num);			/* Do next interrupt */
    }
    break;

  case UART_LCR:	/* Read from Line Control Register */
    val = com[num].LCR;
    #if SUPER_DBG > 1
      s_printf("SER%d: Read LCR = 0x%x\n",num,val);
    #endif
    break;

  case UART_MCR:	/* Read from Modem Control Register */
    val = com[num].MCR;
    #if SUPER_DBG > 1
      s_printf("SER%d: Read MCR = 0x%x\n",num,val);
    #endif
    break;

  case UART_LSR:	/* Read from Line Status Register */
    val = get_lsr(num);
    #if SUPER_DBG > 1
      s_printf("SER%d: Read LSR = 0x%x\n",num,val);
    #endif
    break;

  case UART_MSR:	/* Read from Modem Status Register */
    val = get_msr(num);
    #if SUPER_DBG > 1
      s_printf("SER%d: Read MSR = 0x%x\n",num,val);
    #endif
    break;

  case UART_SCR:   	/* Read from Scratch Register */
    val = com[num].SCR;
    #if SUPER_DBG > 1
      s_printf("SER%d: Read SCR = 0x%x\n",num,val);
    #endif
    break;

  default:		/* The following code should never execute. */
    error("ERROR: Port read 0x%x out of bounds for serial port %d\n",
    	address,num);
    val = 0;
    break;
  }
  return val;
}


/* The following function writes a value to an I/O port.  The port
 * is an I/O address such as 0x3F8 (the base port address of COM1). 
 * [num = abritary port number for serial line, address = I/O port address,
 * val = value to write to I/O port address]
 */
int
do_serial_out(int num, int address, int val)
{
  switch (address - com[num].base_port) {
  case UART_TX:		/* Write to Transmit Holding Register */
/*case UART_DLL:*/	/* or write to Baudrate Divisor Latch LSB */
    if (com[num].DLAB) {	/* If DLAB set, */
      com[num].dll = val;	/* then write to Divisor Latch LSB */
      #if SUPER_DBG > 1
        s_printf("SER%d: Divisor LSB = 0x%02x\n", num, val);
      #endif
    }
    else {
      put_tx(num, val);		/* else, Transmit character (write to THR) */
      #if SUPER_DBG > 1
        if (com[num].MCR & UART_MCR_LOOP)
          s_printf("SER%d: Transmit 0x%x Loopback\n",num,val);
        else
          s_printf("SER%d: Transmit 0x%x\n",num,val);
      #endif
    }
    break;

  case UART_IER:	/* Write to Interrupt Enable Register */
/*case UART_DLM:*/	/* or write to Baudrate Divisor Latch MSB */
    if (com[num].DLAB) {	/* If DLAB set, */
      com[num].dlm = val;	/* then write to Divisor Latch MSB */
      #if SUPER_DBG > 1
        s_printf("SER%d: Divisor MSB = 0x%x\n", num, val);
      #endif
    }
    else {			/* Else, write to Interrupt Enable Register */
      if ( !(com[num].IER & 2) && (val & 2) ) {
        /* Flag to allow THRI if enable THRE went from state 0 -> 1 */
        com[num].tx_trigger = 1;
      }
      com[num].IER = (val & 0xF);	/* Write to IER */
      #if SUPER_DBG > 0
        s_printf("SER%d: IER = 0x%x\n", num, val);
      #endif
      interrupt_engine(num);		/* Do next interrupt if any */
    }
    break;

  case UART_FCR:	/* Write to FIFO Control Register */
    #if SUPER_DBG > 0
      s_printf("SER%d: FCR = 0x%x\n",num,val);
    #endif
    put_fcr(num, val);
    break;

  case UART_LCR: 	/* Write to Line Control Register */
    /* The debug message is in the ser_termios routine via put_lcr */
    put_lcr(num, val);
    break;

  case UART_MCR:	/* Write to Modem Control Register */
    #if SUPER_DBG > 0
      s_printf("SER%d: MCR = 0x%x\n",num,val);
    #endif
    put_mcr(num, val);
    break;

  case UART_LSR:	/* Write to Line Status Register */
    put_lsr(num, val);		/* writeable only to lower 6 bits */
    #if SUPER_DBG > 0
      s_printf("SER%d: LSR = 0x%x -> 0x%x\n",num,val,com[num].LSR);
    #endif
    break;

  case UART_MSR:	/* Write to Modem Status Register */
    put_msr(num, val);		/* writeable only to lower 4 bits */
    #if SUPER_DBG > 0
      s_printf("SER%d: MSR = 0x%x -> 0x%x\n",num,val,com[num].MSR);
    #endif
    break;

  case UART_SCR:	/* Write to Scratch Register */
    #if SUPER_DBG > 0
      s_printf("SER%d: SCR = 0x%x\n",num,val);
    #endif
    com[num].SCR = val;
    break;

  default:		/* The following should never execute */
    error("ERROR: Port write 0x%x out of bounds for serial port %d\n",
    	address,num);
    break;
  }
  return 0;
}


/**************************************************************************/
/*                         BIOS INTERRUPT 0x14                            */
/**************************************************************************/

/* The following function exeuctes a BIOS interrupt 0x14 function.
 * This code by Mark Rejhon replaced some very buggy, old int14 interface
 * a while back.  Due to avoiding blocking/delaying during character 
 * receive/transmit calls, these routines are not flawless.
 * XXXXX - Change this to assembly inline code sometime, perhaps? 
 */
void 
int14(u_char ii)
{
  static int num;
  
  /* Translate the requested COM port number in the DL register, into
   * the necessary arbritary port number system used throughout this module.
   */
  for(num = 0; num < config.num_ser; num++)
    if (com[num].real_comport == (LO(dx)+1)) break;

  if (num >= config.num_ser) return;	/* Exit if not on supported port */

  switch (HI(ax)) {
  case 0:		/* Initialize serial port. */
    /* I wonder if I should also raise DTR in this function, but the
     * problem is that DOSEMU calls this function at bootup, and the
     * DTR is not supposed to be raised at that point, since real
     * DOS doesn't raise the DTR until communications software requests
     * it to be.  XXXXX - This could be a bug in DOSEMU?  If so, then
     * that should be fixed, and code added here to raise DTR of port.
     */
    s_printf("SER%d: INT14 0x0: Initialize port %d, AL=0x%x\n", 
	num, LO(dx), LO(ax));

    /* The following sets character size, parity, and stopbits at once */
    com[num].LCR = (com[num].LCR & ~UART_LCR_PARA) | (LO(ax) & UART_LCR_PARA);

    /* Set the Baudrate Divisor Latch values */
    switch (LO(ax) & 0xF0) {
    case 0x00:   com[num].dlm = 0x04;  com[num].dll = 0x17;  break;
    case 0x20:   com[num].dlm = 0x03;  com[num].dll = 0x00;  break;
    case 0x40:   com[num].dlm = 0x01;  com[num].dll = 0x80;  break;
    case 0x60:   com[num].dlm = 0x00;  com[num].dll = 0xc0;  break;
    case 0x80:   com[num].dlm = 0x00;  com[num].dll = 0x60;  break;
    case 0xa0:   com[num].dlm = 0x00;  com[num].dll = 0x30;  break;
    case 0xc0:   com[num].dlm = 0x00;  com[num].dll = 0x18;  break;
    case 0xe0:   com[num].dlm = 0x00;  com[num].dll = 0x0c;  break;
    }
    ser_termios(num);		/* Update line settings */
    uart_fill(num);		/* Fill UART with received data */
    HI(ax) = get_lsr(num);	/* Line Status */
    LO(ax) = get_msr(num);	/* Modem Status */
    s_printf("SER%d: INT14 0x0: Return with AL=0x%x AH=0x%x\n", 
	num, LO(ax), HI(ax));
    break;
    
  case 1:		/* Write character function */
    #if SUPER_DBG > 1
      s_printf("SER%d: INT14 0x1: Write char 0x%x\n",num,LO(ax));
    #endif
    do_serial_out(num, com[num].base_port, LO(ax));	/* Transmit char */
    uart_fill(num);		/* Fill UART with received data */
    HI(ax) = get_lsr(num);	/* Line Status */
    LO(ax) = get_msr(num);	/* Modem Status */
    break;
    
  case 2:		/* Read character function */
    uart_fill(num);		/* Fill UART with received data */
    if (com[num].LSR & UART_LSR_DR) {	/* Was a character received? */
      LO(ax) = do_serial_in(num, com[num].base_port);	/* Read character */
      #if SUPER_DBG > 1
        s_printf("SER%d: INT14 0x2: Read char 0x%x\n",num,LO(ax));
      #endif
      HI(ax) = get_lsr(num) & ~0x80;	/* Character was received */
    }
    else {
      #if SUPER_DBG > 1
        s_printf("SER%d: INT14 0x2: Read char timeout.\n",num);
      #endif
      HI(ax) = get_lsr(num) | 0x80;	/* Timeout */
    }
    break;
    
  case 3:		/* Port Status request function. */
    uart_fill(num);		/* Fill UART with received data */
    HI(ax) = get_lsr(num);	/* Line Status */
    LO(ax) = get_msr(num);	/* Modem Status */
    #if SUPER_DBG > 1
      s_printf("SER%d: INT14 0x3: Port Status, AH=0x%x AL=0x%x\n",
               num,HI(ax),LO(ax));
    #endif
    break;
    
  case 4:			/* Extended initialize. Not supported. */
    s_printf("SER%d: INT14 0x4: Unsupported Extended serial initialize\n",num);
    return;
    
  default:			/* This runs if nothing handles the func? */
    error("SER%d: INT14 0x%x: Unsupported interrupt on port %d\n",
	num,HI(ax),LO(dx));
    return;
  }
}


/**************************************************************************/
/*                          The SERIAL ENGINES                            */
/**************************************************************************/

/* This function does housekeeping for serial receive operations.
 * Duties of this function include keeping the UART FIFO/RBR register filled,
 * and checking if it's time to generate a hardware interrupt (RDI).
 * [num = port]
 */
static void
receive_engine(int num)		/* Internal 16550 Receive emulation */ 
{
  if (com[num].MCR & UART_MCR_LOOP) return;	/* Return if loopback */

  /* Occasional stack overflows occured when "uart_fill" done inside intr */
  if (!com[num].int_pend) 
    uart_fill(num);

  if (com[num].LSR & UART_LSR_DR) {	/* Is data waiting? */
    if (com[num].fifo_enable) {		/* Is it in FIFO mode? */
      if (com[num].rx_timeout) {		/* Has get_rx run since int? */
        com[num].rx_timeout--;			/* Decrement counter */
        if (!com[num].rx_timeout) {		/* Has timeout counted down? */
          #if SUPER_DBG > 2
            s_printf("SER%d: Func receive_engine setting RX_INTR\n",num);
          #endif
          com[num].int_type |= RX_INTR;		/* Flag recv interrupt */
          interrupt_engine(num);		/* Do next interrupt */
        }
      }
    }
    else { 				/* Not in FIFO mode */
      if (com[num].rx_timeout) {		/* Has get_rx run since int? */
        com[num].rx_timeout = 0;		/* Reset timeout counter */
        #if SUPER_DBG > 2
          s_printf("SER%d: Func receive_engine setting RX_INTR\n",num);
        #endif
        com[num].int_type |= RX_INTR;		/* Flag recv interrupt */
        interrupt_engine(num);			/* Do next interrupt */
      }
    }
  }
}


/* This function does housekeeping for serial transmit operations.
 * Duties of this function include attempting to transmit the data out
 * of the XMIT FIFO/THR register, and checking if it's time to generate
 * a hardware interrupt (THRI).    [num = port]
 */
static void
transmit_engine(int num)      /* Internal 16550 Transmission emulation */
{
  static int rtrn;

  #if 0
    /* This is for transmit timer debugging in case it screws up
     * com[].tx_timer is number of 115200ths of a second worth of chars
     * that have been transmitted, and com[].char_time is 115200ths
     * of a second per character.
     */
    s_printf("SER%d: tx_timer = %d, char_time = %d\n",num,com[num].tx_timer,com[num].char_time);
  #endif

  /* Give system time to transmit */
  if (com[num].tx_timer > MAX_TX_BUF_TICKS) return;
  
  #if 0
    s_printf("SER%d: Exceeded %d\n", num, MAX_TX_BUF_TICKS);
    return;
  }
  else {
    s_printf("SER%d: Within %d\n", num, MAX_TX_BUF_TICKS);
  }
  #endif
  
  if (com[num].tx_overflow) {		/* Is it in overflow state? */
    rtrn = RPT_SYSCALL(write(com[num].fd, &com[num].TX, 1));  /* Write port */
    if (rtrn == 1)                               /* Did it succeed? */
      com[num].tx_overflow = 0;                  /* Exit overflow state */
  }
  else if (com[num].fifo_enable) {	/* Is FIFO enabled? */

    /* Clear as much of the transmit FIFO as possible! */
    while (com[num].TX_FIFO_BYTES > 0) {		/* Any data in fifo? */
      rtrn = RPT_SYSCALL(write(com[num].fd, &com[num].TX_FIFO[com[num].TX_FIFO_START], 1));
      if (rtrn != 1) break;				/* Exit Loop if fail */
      com[num].TX_FIFO_START = (com[num].TX_FIFO_START+1) & (TX_FIFO_SIZE-1);
      com[num].TX_FIFO_BYTES--; 			/* Decr # of bytes */
    }
     
    /* Is FIFO empty, and is it time to trigger an xmit int? */
    if (!com[num].TX_FIFO_BYTES && com[num].tx_trigger) {
      #if SUPER_DBG > 2
        s_printf("SER%d: Func transmit_engine setting TX_INTR\n",num);
      #endif
      com[num].tx_trigger = 0;
      com[num].int_type |= TX_INTR;		/* Set xmit interrupt */
      com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;
      interrupt_engine(num);			/* Do next interrupt */
    }
  }
  else {					/* Not in FIFO mode */
    if (com[num].tx_trigger) { 			/* Is it time to trigger int */
      #if SUPER_DBG > 2
        s_printf("SER%d: Func transmit_engine setting TX_INTR\n",num);
      #endif
      com[num].tx_trigger = 0;
      com[num].int_type |= TX_INTR;		/* Set xmit interrupt */
      com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;
      interrupt_engine(num);			/* Do next interrupt */
    }  
  }
}


/* This function does housekeeping for modem status operations.
 * Duties of this function include polling the serial line for its status
 * and updating the MSR, and checking if it's time to generate a hardware
 * interrupt (MSI).    [num = port]
 */
static void
modstat_engine(int num)		/* Internal Modem Status processing */ 
{
  static int control;
  int newmsr, delta;

  control = 0;
  if (!(com[num].MCR & UART_MCR_LOOP)) {	/* Not in Loopback mode */
    ioctl(com[num].fd, TIOCMGET, &control);	/* WARNING: Non re-entrant! */
    newmsr = CONVERT_BIT(control, TIOCM_CTS, UART_MSR_CTS) |
             CONVERT_BIT(control, TIOCM_DSR, UART_MSR_DSR) |
             CONVERT_BIT(control, TIOCM_RNG, UART_MSR_RI) |
             CONVERT_BIT(control, TIOCM_CAR, UART_MSR_DCD);
    delta = msr_delta(com[num].MSR, newmsr);

    if (delta) {
      com[num].MSR = newmsr | delta | (com[num].MSR & UART_MSR_DELTA);
      #if SUPER_DBG > 2
        s_printf("SER%d: Func modstat_engine setting MS_INTR\n",num);
      #endif
      com[num].int_type |= MS_INTR;
      interrupt_engine(num);
      #if SUPER_DBG > 1
        s_printf("SER%d: Modem Status Change: MSR -> 0x%x\n",num,newmsr);
      #endif
    }
  }
}


/* There is no need for a Line Status Engine function right now since there
 * is not much justification for one, since all the error bits that causes
 * interrupt, are handled by Linux and are difficult for this serial code
 * to detect.  When easy non-blocking break signal handling is introduced,
 * there will be justification for a simple Line Status housekeeping
 * function whose purpose is to detect an error condition (mainly a 
 * break signal sent from the remote) and generate a hardware interrupt
 * on its occurance (RLSI).
 */
#if 0
static void
linestat_engine(int num)		/* Internal Line Status processing */
{
  #if SUPER_DBG > 2
    s_printf("SER%d: Func linestat_engine setting LS_INTR\n",num);
  #endif
  com[num].int_type |= LS_INTR;
  interrupt_engine(num);
}
#endif



/* This function is the  interrupt scheduler.  Its purpose is to
 * invoke any currently pending serial interrupt.  If interrupts
 * are not enabled, the Interrupt Identification Register is set
 * and the function returns.  This is part of the original
 * interrupt_engine.  The prioritizing and execution of the interrupt
 * is done by pic_serial_run().  [num = port]
 *
 * Right now, this engine queues the interrupt.  Later, this can be upgraded
 * to call the interrupts ON THE SPOT within this function (whenever 
 * interrupts are enabled, such as by the STI instruction or that a 
 * previous interrupt has exited and re-enabled interrupts as the STI 
 * instruction would).  This would prove a more accurate 16550 emulation
 * especially for applications that are extremely fussy about interrupt 
 * occuring at the right time.    [num = port]
 */
static void
interrupt_engine(int num)
{
  u_char tmp;
  
  /* Quit if all interrupts disabled, DLAB high, interrupt pending, or
   * that the old interrupt has not been cleared.
   */
  if (!com[num].IER || com[num].int_pend || com[num].DLAB) {
    return;
  }

  /* At this point, we don't much care which function is requested; that  
   * is taken care of in the interrupt_engine.  However, if interrupts are
   * disabled, then the Interrupt Identification Register must be set.
   *
   * The int_type values have been switched from priority order to IER 
   * order, to speed up testing.  The actual priority is controlled by
   * the "if" tests in the pic_serial_run() function, and is unchanged,
   * of course.
   */

  /* See if a requested interrupt is enabled */
  tmp = (com[num].int_type & com[num].IER);
  if (tmp) {
    if (com[num].int_enab && com[num].interrupt) {
      com[num].int_pend = 1;
      /* irq_source_num[com[num].interrupt] = num; */
      #if SUPER_DBG > 2
        s_printf("SER%d: Func pic_request intlevel=%d, int_type=%d\n", 
                 num, com[num].interrupt, com[num].int_type);
      #endif 
      pic_request(com[num].interrupt);
    }
   
    /* Interrupts for this UART are disabled, but we should check whether IER 
     * is set for any interrupt.  If so, then initialize the IIR where needed.
     */
    else if (tmp & LS_INTR) {
      com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_RLSI;
    }
    else if (tmp & RX_INTR) {
      if (!com[num].fifo_enable)
        com[num].IIR = UART_IIR_RDI;
      else if (com[num].RX_FIFO_BYTES >= com[num].RX_FIFO_TRIGGER)
        com[num].IIR = (com[num].IIR & ~0x7) | UART_IIR_RDI;
      else
        com[num].IIR = UART_IIR_CTI | UART_IIR_FIFO;
    }
    else if (tmp & TX_INTR) {
      com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_THRI;
    }
    else if (tmp & MS_INTR) {
      com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_MSI;
    }
    return;
  }
}  


/* This function is called by the priority iunterrupt controller when a
 * serial interrupt occurs.  It executes the highest priority serial
 * interrupt for that port. (Priority order is: RLSI, RDI, THRI, MSI)
 *
 * Because it is theoretically possible for things to change between the
 * interrupt trigger and the actual interrupt, some checks must be 
 * repeated.
 */
void
pic_serial_run(void)
{
  u_char tmp;
  u_char num;

  /* Make sure requested interrupt is still enabled, then run it. 
   * These checks are in priority order.
   *
   * The IER check is delayed until interrupt time, at which this 
   * function executes.  This cause incompatibilities with some
   * programs that expect serial interrupts to be processed
   * immediately on occurance.  The disreprecancies include:
   *   - Possibility of a new, higher priority interrupt to take precedence
   *      before a currently flagged interrupt has a chance to execute.
   *   - Possibility to cancel an interrupt by fulfilling an interrupt
   *      condition, before the interrupt actually executes.
   *
   * Incompatibilities include the 'X00 fossil driver' (use BNU instead)
   * and the 'Natural Connection' software package.
   *
   * In some cases, do_irq() does not execute the interrupt code right
   * away. (Maybe it exits back to vm86 before end of interrupt?).
   * This is a problem with the X00 fossil driver which is having extra
   * side effects like scrambling the order of received characters!
   * (Use the BNU fossil driver instead, it works great)
   */
   
  num = irq_source_num[pic_ilevel];
  tmp = com[num].int_type & com[num].IER;
  
  #if SUPER_DBG > 2
    s_printf("SER%d: Func pic_serial_run interrupt flags = %d\n", 
              num, com[num].int_type);
  #endif 

  if (tmp & LS_INTR) {		/* Receiver Line Status Interrupt */
    beg_lsi(pic_ilevel);
    do_irq();
    end_lsi(pic_ilevel);
  }
  else if (tmp & RX_INTR) {	/* Received Data Interrupt */
    beg_rxint(pic_ilevel);
    do_irq();
    end_rxint(pic_ilevel);
  }
  else if (tmp & TX_INTR) {	/* Transmit Data Interrupt */
    beg_txint(pic_ilevel);
    do_irq();
    end_txint(pic_ilevel);
  }
  else if (tmp & MS_INTR) {	/* Modem Status Interrupt */
    beg_msi(pic_ilevel);
    do_irq();
    end_msi(pic_ilevel);
  }
  else {
    /* No interrupt flagged?  Then the interrupt was cancelled sometime
     * after the interrupt was flagged, but before pic_serial_run executed.
     * But we must run do_irq() anyway at this point, or serial will crash.
     *
     * James Larry, how do you cancel a PIC interrupt, when we have
     * come this far?  (This is preferable)
     */
    do_irq();
    com[num].int_pend = 0;
  }
  interrupt_engine(num);	/* Flag next interrupt if any */
}


/* This function updates a timer counter, mainly used for the timing of 
 * the transmission so that data gets written to the serial device at
 * a constant rate, without buffer overflows.
 */
static void
serial_timer_update(void)
{
  static struct timeval tp;		/* Current timer value */
  static struct timeval oldtp;		/* Timer value from last call */
  static long int elapsed;		/* No of 115200ths seconds elapsed */
  static int i;				/* Loop index */

  /* Get system time */
  gettimeofday(&tp, NULL);

  /* compute the number of 115200hs of seconds since last timer update */
  elapsed  = (tp.tv_sec - oldtp.tv_sec) * 115200;
  elapsed += ((tp.tv_usec - oldtp.tv_usec) * 1152) / 10000;

  /* Reset to 0 if the timer had wrapped around back to 0, just in case */
  if (elapsed < 0) {
    s_printf("SER: Timer wrapped around back to 0!\n");
    elapsed = 0;
  }

  /* Save the old timer values for next time */
  oldtp.tv_sec  = tp.tv_sec;
  oldtp.tv_usec = tp.tv_usec;

  /* Update all the transmit timers */
  for (i = 0; i < config.num_ser; i++) {
    if (com[i].tx_timer > elapsed)
      com[i].tx_timer -= elapsed;
    else
      com[i].tx_timer = 0;
  }
}


/**************************************************************************/
/*                         The MAIN ENGINE LOOP                           */
/**************************************************************************/

/* This is the main housekeeping function, which should be called about
 * 20 to 100 times per second, but should be self-compensating for less
 * frequent executions.  (I might eventually add code to compensate for
 * more frequent executions, but this is not necessary right now)
 * Serial performance improves (up to a certain point) if you increase the
 * frequency of executing this function.
 */
void
serial_run(void)	
{
  static int i;
  stage = ++stage & 7;   /* Counters increments up to 7, wraps back to 0 */

  serial_timer_update();

  /* Do interrupt checking in a logically efficient manner.  For example,
   * The FIFO should be given time to fill up on slower computers, since
   * they easily sieze emulation CPU time.  And modem status checking
   * should not be done more frequently than necessary.  And interrupts
   * should be given time to run through in the hardware interrupt queue.
   */
  for (i = 0; i < config.num_ser; i++) {
    if ((stage % 2) == 0) receive_engine(i);	/* Receive operations */
    if ((stage % 2) == 1) transmit_engine(i);	/* Transmit operations */
    if (stage == 1)       modstat_engine(i);  	/* Modem Status operations */

    /* Trigger any interrupts, just in case */
    interrupt_engine(i);		/* The master interrupts invoker */
  }
  return;
}
