/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* DANG_BEGIN_MODULE
 * 
 * REMARK
 * ser_ports.c: Serial ports for DOSEMU: Software emulated 16550 UART!
 * Please read the README.serial file in this directory for more info!
 * 
 * Copyright (C) 1995 by Mark Rejhon
 *
 * The code in this module is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This module is maintained by Mark Rejhon at these Email addresses:
 *      marky@magmacom.com
 *      ag115@freenet.carleton.ca
 *
 * /REMARK
 * DANG_END_MODULE
 */

/**************************** DECLARATIONS *******************************/

#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <Linux/serial.h>

#include "config.h"
#include "emu.h"
#include "mouse.h"
#include "ser_defs.h"


/*************************************************************************/
/*                 MISCELLANOUS serial support functions                 */
/*************************************************************************/

/* This is a function used for translating a bit to a different bit,
 * depending on a bit inside an integer 'testval'.  It will return 'outbit'
 * if the bit 'inbit' is set in 'testval'.
 */
inline int convert_bit(int testval, int inbit, int outbit)
{
  if (testval & inbit)
    return outbit;
  else
    return 0;
}


/* This function flushes the internal unix receive buffer [num = port] */
static inline void rx_buffer_dump(int num)
{
  tcflush(com[num].fd,TCIFLUSH);
}


/* This function flushes the internal unix transmit buffer [num = port] */
static inline void tx_buffer_dump(int num)
{
  tcflush(com[num].fd,TCOFLUSH);
}


/* This function updates the flow control status depending on buffer condition */
static inline void flow_control_update(int num)
{
  int control;
  if (RX_BUF_BYTES(num) == 0) {			/* buffer empty? */
    control = TIOCM_RTS;
    ioctl(com[num].fd, TIOCMBIS, &control);		/* Raise RTS */
  }
  else {						/* Buffer has chars */
    control = TIOCM_RTS;
    ioctl(com[num].fd, TIOCMBIC, &control);		/* Lower RTS */
#if 0
    com[num].rx_timer = RX_READ_FREQ;	/* Reset rcv read() timer */
#endif
  }
}


/* This function slides the contents of the receive buffer to the 
 * bottom of the buffer.  A sliding buffer is used instead of
 * a circular buffer because this way, a single read() can easily
 * put data straight into our internal receive buffer!
 */
void rx_buffer_slide(int num)
{
  if (com[num].rx_buf_start == 0)
    return;
  /* Move existing chars in receive buffer to the start of buffer */
  memmove(com[num].rx_buf, com[num].rx_buf + com[num].rx_buf_start,
    RX_BUF_BYTES(num));

  /* Update start and end pointers in buffer */        
  com[num].rx_buf_end -= com[num].rx_buf_start;
  com[num].rx_buf_start = 0;
}

void tx_buffer_slide(int num)
{
  if (com[num].tx_buf_start == 0)
    return;
  /* Move existing chars in receive buffer to the start of buffer */
  memmove(com[num].tx_buf, com[num].tx_buf + com[num].tx_buf_start,
    TX_BUF_BYTES(num));

  /* Update start and end pointers in buffer */        
  com[num].tx_buf_end -= com[num].tx_buf_start;
  com[num].tx_buf_start = 0;
}


/* This function checks for newly received data and fills the UART
 * FIFO (16550 mode) or receive register (16450 mode).  
 *
 * Note: The receive buffer is now a sliding buffer instead of 
 * a queue.  This has been found to be more efficient here.
 *
 * [num = port]
 */
void uart_fill(int num)
{
  int size = 0;
  
  /* Return if in loopback mode */
  if (com[num].MCR & UART_MCR_LOOP) return;

  /* If DOSEMU is given direct rts/cts control then update rts/cts status. */
  if (com[num].system_rtscts) flow_control_update(num);

  /* Is it time to do another read() of the serial device yet? 
   * The rx_timer is used to prevent system load caused by empty read()'s 
   * It also skip the following code block if the receive buffer 
   * contains enough data for a full FIFO (at least 16 bytes).
   * The receive buffer is a sliding buffer.
   */
  if (RX_BUF_BYTES(num) < com[num].rx_fifo_size) {
#if 0
    if (com[num].rx_timer == 0) {
#endif    
      /* Slide the buffer contents to the bottom */
      rx_buffer_slide(num);
    
      /* Do a block read of data.  
       * Guaranteed minimum requested read size of (RX_BUFFER_SIZE - 16)!
       */
      size = RPT_SYSCALL(read(com[num].fd, 
                              &com[num].rx_buf[com[num].rx_buf_end],
                              RX_BUFFER_SIZE - com[num].rx_buf_end));
      if (size < 0)
        return;
      if(s3_printf) s_printf("SER%d: Got %i bytes, %i in buffer\n",num,
        size, RX_BUF_BYTES(num));
#if 0    
      if (size == 0) { 				/* No characters read? */
        com[num].rx_timer = RX_READ_FREQ;	/* Reset rcv read() timer */
      }
      else
#endif
      if (size > 0) {		/* Note that size is -1 if error */
        com[num].rx_timeout = TIMEOUT_RX;	/* Reset timeout counter */
        com[num].rx_buf_end += size;
      }
#if 0
    }
#endif
  }

  if (RX_BUF_BYTES(num)) {		/* Is data waiting in the buffer? */
    if (com[num].fifo_enable) {		/* Is it in 16550 FIFO mode? */

      com[num].LSR |= UART_LSR_DR;		/* Set recv data ready bit */

      /* The following code is to emulate the 16 byte (configurable)
       * limitation of the receive FIFO, for compatibility purposes.
       * Reset the receive FIFO counter up to a value of 16 (configurable)
       * if we are not within an interrupt.
       */
      com[num].rx_fifo_bytes = RX_BUF_BYTES(num);
      if (com[num].rx_fifo_bytes > com[num].rx_fifo_size)
	      com[num].rx_fifo_bytes = com[num].rx_fifo_size;
      
      /* Has it gone above the receive FIFO trigger level? */
      if (com[num].rx_fifo_bytes >= com[num].rx_fifo_trigger) {
        if(s3_printf) s_printf("SER%d: Func uart_fill requesting RX_INTR\n",num);
        com[num].LSRqueued |= UART_LSR_DR;	/* Update queued LSR */
        serial_int_engine(num, RX_INTR);	/* Update interrupt status */
      }
      return;
    }
    /* Else, the following code executes if emulated UART is in 16450 mode. */    

    com[num].rx_timeout = 0;			/* Reset receive timeout */
    com[num].LSRqueued |= UART_LSR_DR;		/* Update queued LSR */
    if(s3_printf) s_printf("SER%d: Func uart_fill requesting RX_INTR\n",num);
    serial_int_engine(num, RX_INTR);		/* Update interrupt status */
  }
}


/* This function clears the specified XMIT and/or RCVR FIFO's.  This is
 * done on initialization, and when changing from 16450 to 16550 mode, and
 * vice versa.  [num = port, fifo = flags to indicate which fifos to clear]
 */ 
void uart_clear_fifo(int num, int fifo)
{
  /* DANG_FIXTHIS Should clearing UART cause THRE int if it's enabled? */

  if(s1_printf) s_printf("SER%d: Clear FIFO.\n",num);
  
  /* Clear the receive FIFO */
  if (fifo & UART_FCR_CLEAR_RCVR) {
    /* Preserve THR empty state, clear error bits and recv data ready bit */
    com[num].LSR &= ~(UART_LSR_ERR | UART_LSR_DR);
    com[num].LSRqueued &= ~(UART_LSR_ERR | UART_LSR_DR);
    com[num].rx_buf_start = 0;		/* Beginning of rec FIFO queue */
    com[num].rx_buf_end = 0;		/* End of rec FIFO queue */
    com[num].rx_fifo_bytes = 0;         /* Number of bytes in emulated FIFO */
    com[num].rx_timeout = 0;		/* Receive intr already occured */
    com[num].int_condition &= ~(LS_INTR | RX_INTR);  /* Clear LS/RX conds */
    rx_buffer_dump(num);		/* Clear receive buffer */
  }

  /* Clear the transmit FIFO */
  if (fifo & UART_FCR_CLEAR_XMIT) {
    /* Preserve recv data ready bit and error bits, and set THR empty */
    com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;
    com[num].LSRqueued &= ~(UART_LSR_TEMT | UART_LSR_THRE);
    com[num].tx_buf_start = 0;		/* Start of xmit FIFO queue */
    com[num].tx_buf_end = 0;		/* End of xmit FIFO queue */
    com[num].tx_trigger = 0;            /* Transmit intr already occured */
    com[num].tx_overflow = 0;		/* Not in overflow state */
    com[num].int_condition &= ~TX_INTR;	/* Clear TX int condition */
    tx_buffer_dump(num);		/* Clear transmit buffer */
  }
  com[num].IIR &= ~UART_IIR_FIFO;

  serial_int_engine(num, 0);		/* Update interrupt status */
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
  struct serial_struct ser_info;
  
  /* The following is the same as (com[num].dlm * 256) + com[num].dll */
  #define DIVISOR ((com[num].dlm << 8) | com[num].dll)

  /* Return if not a tty */
  if (tcgetattr(com[num].fd, &com[num].newset) == -1) {
    if(s1_printf) s_printf("SER%d: Line Control: NOT A TTY (%s).\n",num,strerror(errno));
    return;
  }
  ioctl(com[num].fd, TIOCGSERIAL, &ser_info);
  ser_info.flags |= ASYNC_LOW_LATENCY;
  ioctl(com[num].fd, TIOCSSERIAL, &ser_info);

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

  /* Number of 115200ths of a second for each char to transmit,
   * This assumes 10 bits per characters, although math can easily
   * be done here to accomodate the parity and character size.
   * Remember: One bit takes exactly (rounddiv / 115200)ths of a second.
   * DANG_FIXTHIS Fix the calculation assumption
   */
  com[num].tx_char_time = rounddiv * 10;
  
  /* Save the newbaudrate value */
  com[num].newbaud = baud;

#ifdef __linux__
  /* These are required to increase chances that 57600/115200 will work. */
  /* At least, these were needed on my system, Linux 1.2 and Libc 4.6.27 */
  com[num].newset.c_cflag &= ~CBAUD;
  com[num].newset.c_cflag |= baud;
#endif
  
  /* The following does the actual system calls to set the line parameters */
  cfsetispeed(&com[num].newset, baud);
  cfsetospeed(&com[num].newset, baud);
  tcsetattr(com[num].fd, TCSADRAIN, &com[num].newset);

#if 0
  /* Many mouse drivers require this, they detect for Framing Errors
   * coming from the mouse, during initialization, usually right after
   * the LCR register is set, so this is why this line of code is here
   */
  if (com[num].mouse) {
    com[num].LSRqueued |= UART_LSR_FE; 		/* Set framing error */
    if(s3_printf) s_printf("SER%d: Func ser_termios requesting LS_INTR\n",num);
    serial_int_engine(num, LS_INTR);		/* Update interrupt status */
  }
#endif

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
static int get_rx(int num)
{
  int val;
  com[num].rx_timeout = TIMEOUT_RX;		/* Reset timeout counter */

  /* If the Received-Data-Ready bit is clear then return a 0
   * since we're not supposed to read from the buffer right now!
   */
  if ( !(com[num].LSR & UART_LSR_DR)) return 0;
  
  /* Is the FIFO enabled? */
  if (com[num].fifo_enable) {
    /* Read data from the receive buffer.  The 16-byte (configurable)
     * limitation of the FIFO is emulated (even though the receive
     * buffer is bigger) for compatibility purposes.  Note, that the 
     * following code is optimized for speed rather than compactness.
     */
    com[num].rx_fifo_bytes = RX_BUF_BYTES(num);
    if (com[num].rx_fifo_bytes > com[num].rx_fifo_size)
	      com[num].rx_fifo_bytes = com[num].rx_fifo_size;
     
    /* Is the receive FIFO empty? */
    if (com[num].rx_fifo_bytes == 0) return 0;

    val = com[num].rx_buf[com[num].rx_buf_start];	/* Get byte */
    com[num].rx_buf_start++;
    com[num].rx_fifo_bytes--;		/* Emulated 16-byte limitation */
 
    /* Did the FIFO become "empty" right now? */
    if (!com[num].rx_fifo_bytes) {
      
      /* Clear the interrupt flag, and data-waiting flag. */
      com[num].int_condition &= ~RX_INTR;
      com[num].LSR &= ~UART_LSR_DR;
      com[num].LSRqueued &= ~UART_LSR_DR;
        
      /* If receive interrupt is set, then update interrupt status */
      if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RDI)
        serial_int_engine(num, 0);		/* Update interrupt status */
    }
      
    /* The FIFO is not empty, so is an interrupt flagged right now? */
    else if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RDI) {
      
      /* Did the FIFO drop below the trigger level? */
      if (RX_BUF_BYTES(num) < com[num].rx_fifo_trigger) {
        com[num].int_condition &= ~RX_INTR;	/* Clear receive condition */

        /* DANG_FIXTHIS Is this safe to put this here? */
        serial_int_engine(num, 0);		/* Update interrupt status */
      }
      else {		
        /* No, the FIFO didn't drop below trigger, but clear timeout bit */
        com[num].IIR = (com[num].IIR & UART_IIR_ID) | UART_IIR_FIFO;
      }
    }
    return val;		/* Return received byte */
  }
  
  /* The following code executes only if in non-FIFO mode */

  /* Get byte from internal receive queue */
  val = com[num].rx_buf[com[num].rx_buf_start];
  /* Update receive queue pointer and number of chars waiting */
  if (RX_BUF_BYTES(num)) {
    com[num].rx_buf_start++;
  }
  com[num].int_condition &= ~RX_INTR;
  if (!RX_BUF_BYTES(num)) {
    /* Clear data waiting status and interrupt condition flag */
    com[num].LSR &= ~UART_LSR_DR;
    com[num].LSRqueued &= ~UART_LSR_DR;
  }
  /* If receive interrupt is set, then update interrupt status */
  if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RDI) {
    /* DANG_FIXTHIS Is this safe to put this here? */
    serial_int_engine(num, 0);			/* Update interrupt stuats */
  }

  return val;		/* Return received byte */
}


/*************************************************************************/
/*                    MODEM STATUS handling functions                    */
/*************************************************************************/




/* The following computes the MSR delta bits.  It is done by computing
 * the difference between two MSR values (oldmsr and newmsr) by xor'ing
 * bits 4-7 of them.   The exception is the RI (ring) bit which is a
 * trailing edge bit (the RI trailing edge bit is set on only when RI
 * goes from on to off).  [oldmsr = old value, newmsr = new value]
 */
inline int msr_compute_delta_bits(int oldmsr, int newmsr)
{
  int delta;
  
  /* Compute difference bits, and restrict RI bit to trailing-edge */
  delta = (oldmsr ^ newmsr) & ~(newmsr & UART_MSR_RI);

  /* Shift bits to lowest 4 bits and mask other bits except delta bits */
  delta = (delta >> 4) & UART_MSR_DELTA;

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

  modstat_engine(num);				/* Get the fresh MSR status */
  val = com[num].MSR;				/* Save old MSR value */
  com[num].MSR &= UART_MSR_STAT; 		/* Clear delta bits */
  com[num].int_condition &= ~MS_INTR;		/* MSI condition satisfied */

  /* If the modem status interrupt is set, update interrupt status */
  if ((com[num].IIR & UART_IIR_ID) == UART_IIR_MSI)
    serial_int_engine(num, 0);			/* Update interrupt status */

  return val;					/* Return MSR value */
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
  int val;

  val = com[num].LSR;			/* Save old LSR value */
  com[num].int_condition &= ~LS_INTR;	/* RLSI int condition satisfied */
  com[num].LSR &= ~UART_LSR_ERR;	/* Clear error bits */

  /* If RLSI interrupt condition is set, then update interrupt status */
  if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RLSI)
    serial_int_engine(num, 0);		/* Update interrupt status */
  
  return (val);                         /* Return LSR value */
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
static void put_tx(int num, int val)
{
  int rtrn;
#if 0  
  /* Update the transmit timer */
  com[num].tx_timer += com[num].tx_char_time;
#endif
  com[num].int_condition &= ~TX_INTR;	/* TX interrupt condition satisifed */
  com[num].LSR &= ~UART_LSR_TEMT;	/* TEMT not empty */

  /* If Transmit interrupt status is set, then update interrupt status */
  if ((com[num].IIR & UART_IIR_ID) == UART_IIR_THRI)
    serial_int_engine(num, 0);		/* Update interrupt status */

  com[num].TX = val;			/* Mainly used in overflow cases */
  com[num].tx_trigger = 1;		/* Time to trigger next tx int */

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
    
      /* Is the FIFO full? */
      if (RX_BUF_BYTES(num) >= com[num].rx_fifo_size) {
        if(s3_printf) s_printf("SER%d: Func put_tx loopback overrun requesting LS_INTR\n",num);
        com[num].LSR |= UART_LSR_OE;		/* Indicate overrun error */
        com[num].LSRqueued |= UART_LSR_OE;	/* Update queued LSR bits */
        serial_int_engine(num, LS_INTR);	/* Update interrupt status */
      }
      else { /* FIFO not full */
        
        /* Put char into recv FIFO */
        com[num].rx_buf[com[num].rx_buf_end] = val;
        com[num].rx_buf_end++;
        com[num].rx_fifo_bytes = RX_BUF_BYTES(num);

	/* If the buffer touches the top, slide chars to bottom of buffer */
        if ((com[num].rx_buf_end - 1) >= RX_BUFFER_SIZE) rx_buffer_slide(num);
        
        /* Is it the past the receive FIFO trigger level? */
        if (RX_BUF_BYTES(num) >= com[num].rx_fifo_trigger) {
          com[num].rx_timeout = 0;
          com[num].LSRqueued |= UART_LSR_DR;	/* Flag Data Ready bit */
          if(s3_printf) s_printf("SER%d: Func put_tx loopback requesting RX_INTR\n",num);
          serial_int_engine(num, RX_INTR);	/* Update interrupt status */
        }
      }
      com[num].LSR |= UART_LSR_DR;	/* Flag Data Ready bit */
    }
    else {				/* FIFOs not enabled */
      com[num].rx_buf[com[num].rx_buf_start] = val;  /* Overwrite old byte */
      if (com[num].LSR & UART_LSR_DR) {		/* Was data waiting? */
        com[num].LSR |= UART_LSR_OE;		/* Indicate overrun error */
        com[num].LSRqueued |= UART_LSR_OE;	/* Update queued LSR bits */
        if(s3_printf) s_printf("SER%d: Func put_tx loopback overrun requesting LS_INTR\n",num);
        serial_int_engine(num, LS_INTR);	/* Update interrupt status */
      }
      else { 
        com[num].LSR |= UART_LSR_DR; 		/* Flag Data Ready bit */
        com[num].LSRqueued |= UART_LSR_DR; 	/* Flag Data Ready bit */
        com[num].rx_timeout = 0;
        if(s3_printf) s_printf("SER%d: Func put_tx loopback requesting RX_INTR\n",num);
        serial_int_engine(num, RX_INTR);	/* Update interrupt status */
      }
    }
    return;
  }
  /* Else, not in loopback mode */

  if (com[num].fifo_enable) {			/* Is FIFO enabled? */
    if (TX_BUF_BYTES(num) >= TX_BUFFER_SIZE) {	/* Is FIFO already full? */
      /* try to write the character directly to the tty, hoping it
       * will be queued there. Hmmm... */
      rtrn = RPT_SYSCALL(write(com[num].fd,&com[num].tx_buf[com[num].tx_buf_start],1));
      if (rtrn != 1) {				/* Did transmit fail? */
        com[num].tx_overflow = 1;		/* Set overflow flag */
      }
      else {
        /* char written */
        com[num].tx_buf_start++;
        /* Squeeze char into FIFO */
        tx_buffer_slide(num);
        com[num].tx_buf[com[num].tx_buf_end] = val;
        /* Update FIFO queue pointers */
        com[num].tx_buf_end++;
      }
    } 
    else {					/* FIFO not full */
      com[num].tx_buf[com[num].tx_buf_end] = val;  /* Put char into FIFO */
      com[num].tx_buf_end++;
    } 
  } 
  else { 				/* Not in FIFO mode */
    rtrn = RPT_SYSCALL(write(com[num].fd, &val, 1));   /* Attempt char xmit */
    if (rtrn != 1) 				/* Did transmit fail? */
      com[num].tx_overflow = 1; 		/* Set overflow flag */
#if 0
    /* This slows the transfer. There might be better way of avoiding
     * queueing. */
    else tcdrain(com[num].fd);
#endif
  }
  if (!com[num].fifo_enable ||
       TX_BUF_BYTES(num) >= (TX_BUFFER_SIZE-1)) 	/* Is FIFO full? */
      com[num].LSR &= ~UART_LSR_THRE;		/* THR full */

  transmit_engine(num);
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
      if(s1_printf) s_printf("SER%d: FCR: Attempt to change RXRDY & TXRDY pin modes\n",num);
    }

    /* Assign special variable for trigger value for easy access. */
    switch (val & UART_FCR_TRIGGER_14) {
    case UART_FCR_TRIGGER_1:   com[num].rx_fifo_trigger = 1;	break;
    case UART_FCR_TRIGGER_4:   com[num].rx_fifo_trigger = 4;	break;
    case UART_FCR_TRIGGER_8:   com[num].rx_fifo_trigger = 8;	break;
    case UART_FCR_TRIGGER_14:  com[num].rx_fifo_trigger = 14;	break;
    }
    if(s2_printf) s_printf("SER%d: FCR: Trigger Level is %d\n",num,com[num].rx_fifo_trigger);
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
  int changed = com[num].LCR ^ val;    /* bitmask of changed bits */
  
  com[num].LCR = val;                  /* Set new LCR value */
  
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
    s_printf("SER%d: LCR = 0x%x, DLAB low.\n", num, val);
    com[num].DLAB = 0;			/* Baudrate Divisor Latch flag */
    ser_termios(num);			/* Sets new line settings */
  }

  if (changed & UART_LCR_SBC) {
    /* there is change of break state */
    if (val & UART_LCR_SBC) {
      s_printf("SER%d: Setting BREAK state.\n", num);
      tcdrain(com[num].fd);
      ioctl(com[num].fd, TIOCSBRK);
    } else {
      s_printf("SER%d: Clearing BREAK state.\n", num);
      ioctl(com[num].fd, TIOCCBRK);
    }
  }
}


/* This function handles writes to the MCR (Modem Control Register).
 * [num = port, val = new value to write to MCR]
 */
static void
put_mcr(int num, int val)
{
  int newmsr, delta;
  int changed;
  int control;
  changed = com[num].MCR ^ val;			/* Bitmask of changed bits */
  com[num].MCR = val & UART_MCR_VALID;		/* Set valid bits for MCR */

  if (val & UART_MCR_LOOP) {		/* Is Loopback Mode set? */
    /* If loopback just enabled, clear FIFO and turn off DTR & RTS on line */
    if (changed & UART_MCR_LOOP) {		/* Was loopback just set? */
      if (com[num].fifo_enable)
        uart_clear_fifo(num,UART_FCR_CLEAR_CMD);	/* Clear FIFO's */
      control = TIOCM_DTR | TIOCM_RTS;		/* DTR and RTS to be cleared */
      ioctl(com[num].fd, TIOCMBIC, &control);	/* Clear DTR and RTS */
    }

    /* During a UART Loopback test these bits are, Write(Out) => Read(In)
     * MCR Bit 1 RTS => MSR Bit 4 CTS,	MCR Bit 2 OUT1 => MSR Bit 6 RI
     * MCR Bit 0 DTR => MSR Bit 5 DSR,	MCR Bit 3 OUT2 => MSR Bit 7 DCD
     */
    newmsr  = convert_bit(val, UART_MCR_RTS, UART_MSR_CTS);
    newmsr |= convert_bit(val, UART_MCR_DTR, UART_MSR_DSR);
    newmsr |= convert_bit(val, UART_MCR_OUT1, UART_MSR_RI);
    newmsr |= convert_bit(val, UART_MCR_OUT2, UART_MSR_DCD);

    /* Compute delta bits of MSR */
    delta = msr_compute_delta_bits(com[num].MSR, newmsr);

    /* Update the MSR and its delta bits, not erasing old delta bits!! */
    com[num].MSR = (com[num].MSR & UART_MSR_DELTA) | delta | newmsr;
    com[num].MSRqueued = com[num].MSR;

    /* Set the MSI interrupt flag if loopback changed the modem status */
    if(s3_printf) s_printf("SER%d: Func put_mcr loopback requesting MS_INTR\n",num);
    if (delta) serial_int_engine(num, MS_INTR);    /* Update interrupt status */

  }
  else {				/* It's not in Loopback Mode */
    /* Was loopback mode just turned off now?  Then reset FIFO */
    if (changed & UART_MCR_LOOP) {
      if (com[num].fifo_enable)
        uart_clear_fifo(num,UART_FCR_CLEAR_CMD);
    }

    /* Set interrupt enable flag according to OUT2 bit in MCR */
    com[num].int_enab = (val & UART_MCR_OUT2) ? 1 : 0;
    if (com[num].int_enab) {
      if(s3_printf) s_printf("SER%d: Update interrupt status after MCR update\n",num);
      serial_int_engine(num, 0);	/* Update interrupt status */
    }

    /* Force RTS & DTR reinitialization if the loopback state has changed */
    if (UART_MCR_LOOP) changed |= UART_MCR_RTS | UART_MCR_DTR;

    /* Update DTR setting on serial device only if DTR state changed */
    if (changed & UART_MCR_DTR) {
      if(s1_printf) s_printf("SER%d: MCR: DTR -> %d\n",num,(val & UART_MCR_DTR));
      control = TIOCM_DTR;
      if (val & UART_MCR_DTR) 
        ioctl(com[num].fd, TIOCMBIS, &control);
      else
        ioctl(com[num].fd, TIOCMBIC, &control);
    }

    /* Update RTS setting on serial device only if RTS state changed */
    if ((changed & UART_MCR_RTS) && !com[num].system_rtscts) {
      if(s1_printf) s_printf("SER%d: MCR: RTS -> %d\n",num,(val & UART_MCR_RTS));
      control = TIOCM_RTS;
      if (val & UART_MCR_RTS)
        ioctl(com[num].fd, TIOCMBIS, &control);
      else
        ioctl(com[num].fd, TIOCMBIC, &control);
    }
  }
}


/* This function handles writes to the LSR (Line Status Register).
 * [num = port, val = new value to write to LSR]
 */
static void
put_lsr(int num, int val) 
{
  int int_type = 0;
  
  com[num].LSR = val & 0x1F;			/* Bits 6,7,8 are ignored */

  if (val & UART_LSR_ERR)                       /* Any error bits set? */
    int_type |= LS_INTR;			/* Flag RLSI interrupt */
  else
    com[num].int_condition &= ~LS_INTR;         /* Unflag RLSI condition */

  if (val & UART_LSR_DR)                        /* Is data ready bit set? */
    int_type |= RX_INTR;			/* Flag RDI interrupt */
  else
    com[num].int_condition &= ~RX_INTR;         /* Unflag RDI condition */

  if (val & UART_LSR_THRE) {
    com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;       /* Set THRE state */
    int_type |= TX_INTR;			/* Flag TX interrupt */
  }
  else {
    com[num].LSR &= ~(UART_LSR_THRE | UART_LSR_TEMT);   /* Clear THRE state */
    com[num].int_condition &= ~TX_INTR;         /* Unflag THRI condition */
    com[num].tx_trigger = 1;                    /* Trigger next xmit int */
  }

  /* Update queued LSR register */
  com[num].LSRqueued = com[num].LSR;

  /* Update interrupt status */
  if(s3_printf) s_printf("SER%d: Func put_lsr caused int_type = %d\n",num,int_type);
  serial_int_engine(num, int_type);
}


/* This function handles writes to the MSR (Modem Status Register).
 * [num = port, val = new value to write to MSR]
 */
static inline void
put_msr(int num, int val)
{
  /* Update MSR register */
  com[num].MSR = (com[num].MSR & UART_MSR_STAT) | (val & UART_MSR_DELTA);

  /* Update queued MSR register */
  com[num].MSRqueued = (com[num].MSRqueued & UART_MSR_DELTA) | com[num].MSR;
  
  /* Update interrupt status */
  if (com[num].MSRqueued & UART_MSR_DELTA) {
    if(s3_printf) s_printf("SER%d: Func put_msr requesting MS_INTR\n",num);
    serial_int_engine(num, MS_INTR);
  }
  else {
    serial_int_engine(num, 0);
  }
}


/*************************************************************************/
/*            PORT I/O to UART registers: INPUT AND OUTPUT               */
/*************************************************************************/

/* DANG_BEGIN_FUNCTION do_serial_in
 * The following function returns a value from an I/O port.  The port
 * is an I/O address such as 0x3F8 (the base port address of COM1). 
 * There are 8 I/O addresses for each serial port which ranges from
 * the base port (ie 0x3F8) to the base port plus seven (ie 0x3FF).
 * [num = abritary port number for serial line, address = I/O port address]
 * DANG_END_FUNCTION
 */
int
do_serial_in(int num, ioport_t address)
{
  int val;

  /* delayed open happens here */
  if (com[num].fd == -1) {
    if (ser_open(num) >= 0)
      ser_set_params(num);
  }
  if (com[num].fd < 0)
    return 0;

  switch (address - com[num].base_port) {
  case UART_RX:		/* Read from Received Byte Register */	
/*case UART_DLL:*/      /* or Read from Baudrate Divisor Latch LSB */
    if (com[num].DLAB) {	/* Is DLAB set? */
      val = com[num].dll;	/* Then read Divisor Latch LSB */
      if(s1_printf) s_printf("SER%d: Read Divisor LSB = 0x%x\n",num,val);
    }
    else {
      val = get_rx(num);	/* Else, read Received Byte Register */
      if(s2_printf) s_printf("SER%d: Receive 0x%x\n",num,val);
    }
    break;

  case UART_IER:	/* Read from Interrupt Enable Register */
/*case UART_DLM:*/      /* or Read from Baudrate Divisor Latch MSB */
    if (com[num].DLAB) {	/* Is DLAB set? */
      val = com[num].dlm;	/* Then read Divisor Latch MSB */
      if(s1_printf) s_printf("SER%d: Read Divisor MSB = 0x%x\n",num,val);
    }
    else {
      val = com[num].IER;	/* Else, read Interrupt Enable Register */
      if(s1_printf) s_printf("SER%d: Read IER = 0x%x\n",num,val);
    }
    break;

  case UART_IIR:	/* Read from Interrupt Identification Register */
    val = com[num].IIR;
    if ((com[num].IIR & UART_IIR_ID) == UART_IIR_THRI) {
      if(s2_printf) s_printf("SER%d: Read IIR = 0x%x (THRI now cleared)\n",num,val);
      com[num].int_condition &= ~TX_INTR;	/* Unflag TX int condition */
      serial_int_engine(num, 0);		/* Update interrupt status */
    }
    else {
      if(s2_printf) s_printf("SER%d: Read IIR = 0x%x\n",num,val);
    }
    break;

  case UART_LCR:	/* Read from Line Control Register */
    val = com[num].LCR;
    if(s2_printf) s_printf("SER%d: Read LCR = 0x%x\n",num,val);
    break;

  case UART_MCR:	/* Read from Modem Control Register */
    val = com[num].MCR;
    if(s2_printf) s_printf("SER%d: Read MCR = 0x%x\n",num,val);
    break;

  case UART_LSR:	/* Read from Line Status Register */
    val = get_lsr(num);
    if(s2_printf) s_printf("SER%d: Read LSR = 0x%x\n",num,val);
    break;

  case UART_MSR:	/* Read from Modem Status Register */
    val = get_msr(num);
    if(s2_printf) s_printf("SER%d: Read MSR = 0x%x\n",num,val);
    break;

  case UART_SCR:   	/* Read from Scratch Register */
    val = com[num].SCR;
    if(s2_printf) s_printf("SER%d: Read SCR = 0x%x\n",num,val);
    break;

  default:		/* The following code should never execute. */
    s_printf("ERROR: Port read 0x%x out of bounds for serial port %d\n",
    	address,num);
    val = 0;
    break;
  }

  serial_run();		/* See if some work is to be done */

  return val;
}


/* DANG_BEGIN_FUNCTION do_serial_out 
 * The following function writes a value to an I/O port.  The port
 * is an I/O address such as 0x3F8 (the base port address of COM1). 
 * [num = abritary port number for serial line, address = I/O port address,
 * val = value to write to I/O port address]
 * DANG_END_FUNCTION
 */
int
do_serial_out(int num, ioport_t address, int val)
{

  /* delayed open happens here */
  if (com[num].fd == -1) {
    if (ser_open(num) >= 0)
      ser_set_params(num);
  }
  if (com[num].fd < 0)
    return 0;

  switch (address - com[num].base_port) {
  case UART_TX:		/* Write to Transmit Holding Register */
/*case UART_DLL:*/	/* or write to Baudrate Divisor Latch LSB */
    if (com[num].DLAB) {	/* If DLAB set, */
      com[num].dll = val;	/* then write to Divisor Latch LSB */
      if(s2_printf) s_printf("SER%d: Divisor LSB = 0x%02x\n", num, val);
    }
    else {
      put_tx(num, val);		/* else, Transmit character (write to THR) */
      if (s2_printf) {
        if (com[num].MCR & UART_MCR_LOOP)
          s_printf("SER%d: Transmit 0x%x Loopback\n",num,val);
        else
          s_printf("SER%d: Transmit 0x%x\n",num,val);
      }
    }
    break;

  case UART_IER:	/* Write to Interrupt Enable Register */
/*case UART_DLM:*/	/* or write to Baudrate Divisor Latch MSB */
    if (com[num].DLAB) {	/* If DLAB set, */
      com[num].dlm = val;	/* then write to Divisor Latch MSB */
      if(s2_printf) s_printf("SER%d: Divisor MSB = 0x%x\n", num, val);
    }
    else {			/* Else, write to Interrupt Enable Register */
      if ( !(com[num].IER & 2) && (val & 2) ) {
        /* Flag to allow THRI if enable THRE went from state 0 -> 1 */
        com[num].tx_trigger = 1;
      }
      com[num].IER = (val & 0xF);	/* Write to IER */
      if(s1_printf) s_printf("SER%d: Write IER = 0x%x\n", num, val);
      serial_int_engine(num, 0);		/* Update interrupt status */
    }
    break;

  case UART_FCR:	/* Write to FIFO Control Register */
    if(s1_printf) s_printf("SER%d: FCR = 0x%x\n",num,val);
    put_fcr(num, val);
    break;

  case UART_LCR: 	/* Write to Line Control Register */
    /* The debug message is in the ser_termios routine via put_lcr */
    put_lcr(num, val);
    break;

  case UART_MCR:	/* Write to Modem Control Register */
    if(s1_printf) s_printf("SER%d: MCR = 0x%x\n",num,val);
    put_mcr(num, val);
    break;

  case UART_LSR:	/* Write to Line Status Register */
    put_lsr(num, val);		/* writeable only to lower 6 bits */
    if(s1_printf) s_printf("SER%d: LSR = 0x%x -> 0x%x\n",num,val,com[num].LSR);
    break;

  case UART_MSR:	/* Write to Modem Status Register */
    put_msr(num, val);		/* writeable only to lower 4 bits */
    if(s1_printf) s_printf("SER%d: MSR = 0x%x -> 0x%x\n",num,val,com[num].MSR);
    break;

  case UART_SCR:	/* Write to Scratch Register */
    if(s1_printf) s_printf("SER%d: SCR = 0x%x\n",num,val);
    com[num].SCR = val;
    break;

  default:		/* The following should never execute */
    s_printf("ERROR: Port write 0x%x out of bounds for serial port %d\n",
             address,num);
    break;
  }

  serial_run();		/* See if some work is to be done */

  return 0;
}
