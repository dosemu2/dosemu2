/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "emu.h"
#include "ser_defs.h"
#include "tty_io.h"

/* This function flushes the internal unix receive buffer [num = port] */
void rx_buffer_dump(int num)
{
  tcflush(com[num].fd,TCIFLUSH);
}

/* This function flushes the internal unix transmit buffer [num = port] */
void tx_buffer_dump(int num)
{
  tcflush(com[num].fd,TCOFLUSH);
}

int serial_get_tx_queued(int num)
{
  int ret, queued;
  ret = ioctl(com[num].fd, TIOCOUTQ, &queued);
  if (ret < 0)
    return ret;
  return queued;
}

/* This function updates the line settings of a serial line (parity,
 * stop bits, word size, and baudrate) to conform to the value
 * stored in the Line Control Register (com[].LCR) and the Baudrate
 * Divisor Latch Registers (com[].dlm and com[].dll)     [num = port]
 */
void ser_termios(int num)
{
  speed_t baud;
  long int rounddiv;

  if (com[num].fifo)
    return;

  /* The following is the same as (com[num].dlm * 256) + com[num].dll */
  #define DIVISOR ((com[num].dlm << 8) | com[num].dll)

  /* Return if not a tty */
  if (tcgetattr(com[num].fd, &com[num].newset) == -1) {
    if(s1_printf) s_printf("SER%d: Line Control: NOT A TTY (%s).\n",num,strerror(errno));
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
   *
   * Note: 38400, 57600 and 115200 won't work properly if setserial has
   * the 'spd_hi' or the 'spd_vhi' setting turned on (they're obsolete!)
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
  s_printf("divisor 0x%x -> 0x%lx\n", DIVISOR, rounddiv);

  /* The following does the actual system calls to set the line parameters */
  cfsetispeed(&com[num].newset, baud);
  cfsetospeed(&com[num].newset, baud);
  if (debug_level('s') > 7) {
    s_printf("SER%d: iflag=%x oflag=%x cflag=%x lflag=%x\n", num,
	    com[num].newset.c_iflag, com[num].newset.c_oflag,
	    com[num].newset.c_cflag, com[num].newset.c_lflag);
  }
  tcsetattr(com[num].fd, TCSANOW, &com[num].newset);
#if 0
  /* Many mouse drivers require this, they detect for Framing Errors
   * coming from the mouse, during initialization, usually right after
   * the LCR register is set, so this is why this line of code is here
   */
  if (com[num].mouse) {
    com[num].LSR |= UART_LSR_FE; 		/* Set framing error */
    if(s3_printf) s_printf("SER%d: Func ser_termios requesting LS_INTR\n",num);
    serial_int_engine(num, LS_INTR);		/* Update interrupt status */
  }
#endif
}

int serial_brkctl(int num, int brkflg)
{
  int ret;
  /* there is change of break state */
  if (brkflg) {
    s_printf("SER%d: Setting BREAK state.\n", num);
    tcdrain(com[num].fd);
    ret = ioctl(com[num].fd, TIOCSBRK);
  } else {
    s_printf("SER%d: Clearing BREAK state.\n", num);
    ret = ioctl(com[num].fd, TIOCCBRK);
  }
  return ret;
}

ssize_t serial_write(int num, char *buf, size_t len)
{
  if (com[num].fifo)	// R/O fifo
    return len;
  return RPT_SYSCALL(write(com[num].fd, buf, len));   /* Attempt char xmit */
}
