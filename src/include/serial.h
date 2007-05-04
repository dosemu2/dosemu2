/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* DANG_BEGIN_MODULE
 *
 * REMARK
 * serial.h: Include file for port data array for DOSEMU serial.
 * Please send bug reports and bugfixes to marky@magmacom.com
 * Please read the files in the 'serial' subdirectory for more info.
 * /REMARK
 *
 * This module is maintained by Mark Rejhon at these Email addresses:
 *      marky@magmacom.com
 *      ag115@freenet.carleton.ca
 *
 * COPYRIGHTS
 * ~~~~~~~~~~
 *   Copyright (C) 1995 by Mark Rejhon
 *
 *   All of this code is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as  
 *   published by the Free Software Foundation; either version 2 of 
 *   the License, or (at your option) any later version.
 *
 * DANG_END_MODULE
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <termios.h>
#include "extern.h"

/* These are sizes for the internal recieve and transmit buffers.
 * They must be at least 16 bytes because these double as FIFO's,
 * The 16-byte limitation is emulated, though, for compatibility
 * purposes.  (Although this may be configurable eventually)
 *
 * DANG_FIXTHIS Why does a RX_BUFFER_SIZE of 256 cause slower performance than a size of 128?
 */
#define RX_BUFFER_SIZE            128
#define TX_BUFFER_SIZE            64

extern int no_local_video; /* used by virtual port code */
EXTERN u_char irq_source_num[255];	/* Index to map from IRQ no. to serial port */
EXTERN u_char com_port_used[17];       /* Used for auto-assign comport config */

typedef struct {
  				/*   MAIN VARIABLES  */
  char *dev;			/* String to hold path to device file */
  int fd;			/* File descriptor of device */
  int real_comport;		/* The actual COMx port number. 0 for invalid */
  ioport_t base_port;		/* Base port address handled by device */
  int interrupt;		/* IRQ line handled by device */
  boolean virtual;		/* virtual modem */
  boolean pseudo;		/* pseudo-tty is used */
  boolean mouse;		/* Flag to turn on mouse sharing features */
  boolean dev_locked;           /* Flag to indicate that device is locked */
  boolean fossil_active;	/* Flag: FOSSIL emulation active */
  u_char fossil_info[19];	/* FOSSIL driver info buffer */
  int system_rtscts;		/* Flag: emulate RTS or let system handle it */
  				/*   MODEM STATUS  */
  long int ms_freq;		/* Frequency of Modem Status (MS) check */
  long int ms_timer;            /* Countdown to forced MS check */
  				/*   RECEIVE  */
  long int rx_timer;		/* Countdown to next read() system call */
  u_char rx_timeout;		/* Recieve Interrupt timeout counter */
  u_char rx_fifo_trigger;	/* Receive Fifo trigger value */
  int rx_fifo_bytes;		/* Receive Fifo bytes-waiting counter */
  int rx_fifo_size;		/* Size of receive FIFO to emulate */
  				/*   TRANSMIT  */
  long int tx_timer;            /* Countdown to next char being xmitted */
  long int tx_char_time;        /* Number of 115200ths of sec per char */
  u_char tx_trigger;		/* Flag whether Xmit int should be triggered */
  u_char tx_overflow;		/* Full outgoing buffer flag. */
  				/*   MISCELLANEOUS  */
  u_char int_request;		/* Interrupt Request flags - TX/RX/MS/LS */
  u_char int_condition;		/* Interrupt Condition flags - TX/RX/MS/LS */
  u_char int_enab;		/* Interrupt Enabled flag (OUT2 of MCR) */
  u_char fifo_enable;		/* FIFO enabled flag */
  u_char uart_full;		/* UART full flag */
  speed_t newbaud;		/* Currently set bps rate */

  /* The following are serial port registers */
  int dll, dlm;		/* Baudrate divisor LSB and MSB */
  u_char DLAB;		/* Divisor Latch enabled */
  u_char TX;		/* Transmit Holding Register */
  u_char RX;		/* Received Data Register */
  u_char IER;		/* Interrupt Enable Register */
  u_char IIR;		/* Interrupt Identification Register */
  u_char LCR;		/* Line Control Register */
  u_char FCReg;		/* Fifo Control Register (.FCR is a name conflict) */
  u_char MCR;		/* Modem Control Register */
  u_char LSR;		/* Line Status Register */
  u_char MSR;		/* Modem Status Register */
  u_char SCR;		/* Scratch Pad Register */
  u_char LSRqueued;     /* One-byte LSR queue for interrupts */
  u_char MSRqueued;     /* One-byte MSR queue for interrupts */

  /* The following are the transmit and receive buffer variables
   * They are bigger than the 16 bytes of a real FIFO to improve
   * performance, but the 16-byte limitation of the receive FIFO
   * is still emulated using a counter, to improve compatibility.
   */
  u_char rx_buf[RX_BUFFER_SIZE];	/* Receive Buffer */
  u_char rx_buf_start;			/* Receive Buffer queue start */
  u_char rx_buf_end;			/* Receive Buffer queue end */
  u_char tx_buf[TX_BUFFER_SIZE];	/* Transmit Buffer */
  u_char tx_buf_start;			/* Transmit Buffer queue start */
  u_char tx_buf_end;			/* Transmit Buffer queue end */

  struct termios oldset;		/* Original termios settings */
  struct termios newset;		/* Current termios settings */
} serial_t;

#define MAX_SER 4
EXTERN serial_t com[MAX_SER];

#define RX_BUF_BYTES(num) (com[num].rx_buf_end - com[num].rx_buf_start)
#define TX_BUF_BYTES(num) (com[num].tx_buf_end - com[num].tx_buf_start)

extern int int14(void);
extern void serial_run(void);
extern int do_serial_in(int, ioport_t);
extern int do_serial_out(int, ioport_t, int);
extern void serial_helper(void);
extern void child_close_mouse(void);
extern void child_open_mouse(void);

#endif /* SERIAL_H */
