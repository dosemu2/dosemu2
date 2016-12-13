/* DANG_BEGIN_MODULE
 *
 * REMARK
 * ser_defs.h: Include file for all files in the 'serial' subdirectory.
 * Please send bug reports and bugfixes to marky@magmacom.com
 * Please read the files in this 'serial' subdirectory for more info.
 *
 * /REMARK
 * This module is maintained by Stas Sergeev <stsp@users.sourceforge.net>
 *
 * COPYRIGHTS
 * ~~~~~~~~~~
 *   UART defs derived from Theodore Ts'o work: /linux/include/linux/serial.h
 *   Other code Copyright (C) 1995 by Mark Rejhon
 *
 *   The code in this module is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 * DANG_END_MODULE
 */
#ifndef SER_DEFS_H
#define SER_DEFS_H

#include "serial.h"

/* DANG_BEGIN_REMARK
 * Extensions to serial debugging.
 *
 * SER_DEBUG_MAIN   (0 or 1)
 *   - extra debug output on the most critical information.
 *
 * SER_DEBUG_HEAVY   (0 or 1)
 *   - super-heavy extra debug output, including all ports reads and writes,
 *      and every character received and transmitted!
 *
 * SER_DEBUG_INTERRUPT   (0 or 1)
 *   - additional debug output related to serial interrupt code,
 *      including flagging serial interrupts, or PIC-driven code.
 *
 * SER_DEBUG_FOSSIL_RW   (0 or 1)
 *   - heavy FOSSIL debug output, including all reads and writes.
 *
 * SER_DEBUG_FOSSIL_STATUS   (0 or 1)
 *   - super-heavy FOSSIL debug output, including all status checks.
 *
 * You must recompile dosemu everytime one of these constants are modified.
 * Just type 'make' in the dosemu dir and it will recompile the changes only.
 * DANG_END_REMARK
 */
#define SER_DEBUG_MAIN       1		/* 0 or 1 */
#define SER_DEBUG_HEAVY      1		/* 0 or 1 */
#define SER_DEBUG_INTERRUPT  1		/* 0 or 1 */
#define SER_DEBUG_FOSSIL_RW  1       	/* 0 or 1 */
#define SER_DEBUG_FOSSIL_STATUS 0	/* 0 or 1 */

/*
 * DANG_BEGIN_REMARK
 *
 * IMPORTANT INFO about com[] variable array structure used in serial.c
 *
 * Most of the serial variables are stored in the com[] array.
 * The com[] array is a structure in itself.   Take a look at the
 * 'serial_t' struct declaration in the serial.h module for more info
 * about this.  Only the most commonly referenced global variables
 * are listed here:
 *
 *   config.num_ser         Number of serial ports active.
 *   com[x].base_port       The base port address of emulated serial port.
 *   com[x].real_comport    The COM port number.
 *   com[x].interrupt       The PIC interrupt level (based on IRQ number)
 *   com[x].mouse           Flag  mouse (to enable extended features)
 *   com[x].fd              File descriptor for port device
 *   com[x].dev[]           Filename of port port device
 *   com[x].dev_locked      Flag whether device has been locked
 *
 * The arbritary example variable 'x' in com[x] can have a minimum value
 * of 0 and a maximum value of (config.numser - 1).  There can be no gaps
 * for the value 'x', even though gaps between actual COM ports are permitted.
 * It is strongly noted that the 'x' does not equal the COM port number.
 * This example code illustrates the fact, and how the com[] array works:
 *
 *   for (i = 0; i < config.numser; i++)
 *     s_printf("COM port number %d has a base address of %x",
 *              com[i].real_comport, com[i].base_port);
 *
 * DANG_END_REMARK
 */

/* Defines to control existence of the debug outputting commands.
 * The C compiler is smart enough to not generate extra code when certain
 * debugging output commands are not defined to anything in particular.
 */
#if SER_DEBUG_MAIN
#define s1_printf 1
#else
#define s1_printf 0
#endif

#if SER_DEBUG_HEAVY
#define s2_printf 1
#else
#define s2_printf 0
#endif

#if SER_DEBUG_INTERRUPT
#define s3_printf 1
#else
#define s3_printf 0
#endif

/************************************************************
 *  The following UART definitions are derived from         *
 *  linux/include/linux/serial.h, a work by Theodore Ts'o.  *
 *  These definitions should NOT be changed!                *
 ************************************************************/

/* These are the UART port assignments, expressed as offsets from the base
 * register.  These assignments should hold for any serial port based on
 * a 8250, 16450, or 16550(A).
 */
#define UART_RX		0	/* In:  Receive buffer (DLAB=0) */
#define UART_TX		0	/* Out: Transmit buffer (DLAB=0) */
#define UART_DLL	0	/* Out: Devisor Latch Low (DLAB=1) */
#define UART_DLM	1	/* Out: Devisor Latch High (DLAB=1) */
#define UART_IER	1	/* Out: Interrupt Enable Register */
#define UART_IIR	2	/* In:  Interrupt ID Register */
#define UART_FCR	2	/* Out: FIFO Control Register */
#define UART_LCR	3	/* Out: Line Control Register */
#define UART_MCR	4	/* Out: Modem Control Register */
#define UART_LSR	5	/* In:  Line Status Register */
#define UART_MSR	6	/* In:  Modem Status Register */
#define UART_SCR	7	/* I/O: Scratch Register */

/* These are the definitions for the FIFO Control Register
 */
#define UART_FCR_ENABLE_FIFO	0x01	/* Enable the FIFO */
#define UART_FCR_CLEAR_RCVR	0x02	/* Clear the RCVR FIFO */
#define UART_FCR_CLEAR_XMIT	0x04	/* Clear the XMIT FIFO */
#define UART_FCR_DMA_SELECT	0x08	/* For DMA applications */
#define UART_FCR_TRIGGER_MASK	0xC0	/* Mask for the FIFO trigger range */
#define UART_FCR_TRIGGER_1	0x00	/* Mask for trigger set at 1 */
#define UART_FCR_TRIGGER_4	0x40	/* Mask for trigger set at 4 */
#define UART_FCR_TRIGGER_8	0x80	/* Mask for trigger set at 8 */
#define UART_FCR_TRIGGER_14	0xC0	/* Mask for trigger set at 14 */

#define UART_FCR_CLEAR_CMD	(UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT)
#define UART_FCR_SETUP_CMD	(UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_8)

/* These are the definitions for the Line Control Register
 *
 * Note: if the word length is 5 bits (UART_LCR_WLEN5), then setting
 * UART_LCR_STOP will select 1.5 stop bits, not 2 stop bits.
 * UART_LCR_SPAR is rarely used, but if this is set, it means to
 * stick the parity bit high or low, depending on UART_LCR_PARITY.
 */
#define UART_LCR_DLAB	0x80	/* Devisor latch access bit */
#define UART_LCR_SBC	0x40	/* Set break control */
#define UART_LCR_SPAR	0x20	/* Stick parity bit continously high or low */
#define UART_LCR_EPAR	0x10	/* Even paraity select */
#define UART_LCR_PARITY	0x08	/* Parity Enable */
#define UART_LCR_STOP	0x04	/* Stop bits: 0=1 stop bit, 1= 2 stop bits */
#define UART_LCR_WLEN5  0x00	/* Wordlength: 5 bits */
#define UART_LCR_WLEN6  0x01	/* Wordlength: 6 bits */
#define UART_LCR_WLEN7  0x02	/* Wordlength: 7 bits */
#define UART_LCR_WLEN8  0x03	/* Wordlength: 8 bits */
#define UART_LCR_PARA   0x1f    /* Parity, Stop bits and Wordlength */

/* These are the definitions for the Line Status Register
 */
#define UART_LSR_TEMT	0x40	/* Transmitter empty */
#define UART_LSR_THRE	0x20	/* Transmit-hold-register empty */
#define UART_LSR_BI	0x10	/* Break interrupt indicator */
#define UART_LSR_FE	0x08	/* Frame error indicator */
#define UART_LSR_PE	0x04	/* Parity error indicator */
#define UART_LSR_OE	0x02	/* Overrun error indicator */
#define UART_LSR_DR	0x01	/* Receiver data ready */
#define UART_LSR_ERR    0x1e    /* All the error indicators */

/* These are the definitions for the Interrupt Indentification Register
 */
#define UART_IIR_NO_INT	0x01	/* No interrupts pending */
#define UART_IIR_MSI	0x00	/* Modem status interrupt */
#define UART_IIR_THRI	0x02	/* Transmitter holding register empty */
#define UART_IIR_RDI	0x04	/* Receiver data interrupt */
#define UART_IIR_RLSI	0x06	/* Receiver line status interrupt */
#define UART_IIR_CND_MASK 7
#if 0
#define UART_IIR_CTI    0x0c	/* Character timeout indication */
#define UART_IIR_ID	0x06	/* Mask for the interrupt ID */
#define UART_IIR_FIFO_ENABLE_1 0x40
#define UART_IIR_FIFO_ENABLE_2 0x80
#define UART_IIR_FIFO (UART_IIR_FIFO_ENABLE_1|UART_IIR_FIFO_ENABLE_2)
#else
#define IIR_FIFO_ENABLE 3
#endif

/* These are the definitions for the Interrupt Enable Register
 */
#define UART_IER_MSI	0x08	/* Enable Modem status interrupt */
#define UART_IER_RLSI	0x04	/* Enable receiver line status interrupt */
#define UART_IER_THRI	0x02	/* Enable Transmitter holding register int. */
#define UART_IER_RDI	0x01	/* Enable receiver data interrupt */
#define UART_IER_VALID	0x0f

/* These are the definitions for the Modem Control Register
 */
#define UART_MCR_LOOP	0x10	/* Enable loopback test mode */
#define UART_MCR_OUT2	0x08	/* Out2 complement */
#define UART_MCR_OUT1	0x04	/* Out1 complement */
#define UART_MCR_RTS	0x02	/* RTS complement */
#define UART_MCR_DTR	0x01	/* DTR complement */
#define UART_MCR_VALID	0x1F	/* The valid registers of the MCR */

/* These are the definitions for the Modem Status Register
 */
#define UART_MSR_DCD	0x80	/* Data Carrier Detect */
#define UART_MSR_RI	0x40	/* Ring Indicator */
#define UART_MSR_DSR	0x20	/* Data Set Ready */
#define UART_MSR_CTS	0x10	/* Clear to Send */
#define UART_MSR_DDCD	0x08	/* Delta DCD */
#define UART_MSR_TERI	0x04	/* Trailing edge ring indicator */
#define UART_MSR_DDSR	0x02	/* Delta DSR */
#define UART_MSR_DCTS	0x01	/* Delta CTS */
#define UART_MSR_DELTA	0x0F	/* Any of the delta bits! */
#define UART_MSR_STAT	0xF0	/* Any of the non-delta bits! */

/* These are baudrate divisors.  BaudRate = 1843200 / (DIVISOR * 16)
 */
#define DIV_50      0x900
#define	DIV_110     0x417
#define	DIV_150     0x300
#define	DIV_300     0x180
#define	DIV_600     0x0C0
#define	DIV_1200    0x060
#define	DIV_1800    0x040
#define	DIV_2000    0x03A
#define	DIV_2400    0x030
#define	DIV_3600    0x020
#define	DIV_4800    0x018
#define	DIV_7200    0x010
#define	DIV_9600    0x00C
#define	DIV_19200   0x006
#define	DIV_38400   0x003
#define	DIV_57600   0x002
#define	DIV_115200  0x001

/* Interrupts pending flag bits for com[num].int_type
 * These bits must match the UART_IER_xxx bits, above
 */
#define RX_INTR    UART_IER_RDI    /* 1 */
#define TX_INTR    UART_IER_THRI   /* 2 */
#define LS_INTR    UART_IER_RLSI   /* 4 */
#define MS_INTR    UART_IER_MSI    /* 8 */


/**********************************************************
 *  The following are definitions that can be fine-tuned  *
 **********************************************************/

/* The following are positive constants that adjust the soonness of the next
 * receive or transmit interrupt in FIFO mode.  These are a little
 * bit sensitive, and may dissappear when better timer code arrives.
 */
#define TIMEOUT_RX   3

/* Maximum number of 115200ths of a seconds worth of chars to buffer
 * If more characters than this is in the buffer, then wait till it
 * drops below this value, to trigger the next transmit interrupt
 * Right now this is set to 1/10th sec worth of buffered xmit bytes.
 */
#define TX_BUF_THRESHOLD        11520L

/* Frequency of read() on the serial device.  This is in format of
 * 115200ths between every read().  Right now this is set to 1/60th
 * of a second between reads, (1920/115200) if no data was waiting.
 * This is to reduce system load caused by read() on the serial device.
 */
#define RX_READ_FREQ            1920L

/* These are sizes for the internal recieve and transmit buffers.
 * They must be at least 16 bytes because these double as FIFO's,
 * The 16-byte limitation is emulated, though, for compatibility
 * purposes.  (Although this may be configurable eventually)
 *
 * DANG_FIXTHIS Why does a RX_BUFFER_SIZE of 256 cause slower performance than a size of 128?
 */
#define RX_BUFFER_SIZE            128

/* Minimum frequency for modem status checks, in 115200ths seconds
 * between checks of the modem status.  Right now this is set to
 * 1/30th of a second (3840/115200)
 */
#define MS_MIN_FREQ		3840L

struct serial_drv;

struct iir {
  u_char mask;
  union {
    struct {
      u_char cti:1;
      u_char rsv:1;
      u_char fifo_64b:1;
      u_char fifo_enable:2;
    } flg;
    u_char flags:5;
  };
};

typedef struct {
  				/*   MAIN VARIABLES  */
  serial_t *cfg;
  int num;
  int fd;			/* File descriptor of device */
  boolean opened;
  boolean is_file;
  boolean ro;
  boolean dev_locked;           /* Flag to indicate that device is locked */
  boolean fossil_active;	/* Flag: FOSSIL emulation active */
  u_char fossil_info[19];	/* FOSSIL driver info buffer */
  struct vec_t ivec;
  				/*   MODEM STATUS  */
//  long int ms_freq;		/* Frequency of Modem Status (MS) check */
  long int ms_timer;            /* Countdown to forced MS check */
  				/*   RECEIVE  */
  long int rx_timer;		/* Countdown to next read() system call */
  u_char rx_timeout;		/* Recieve Interrupt timeout counter */
  u_char rx_fifo_trigger;	/* Receive Fifo trigger value */
  int rx_fifo_size;		/* Size of receive FIFO to emulate */
  				/*   MISCELLANEOUS  */
  int interrupt;		/* IRQ line handled by device */
  u_char int_condition;		/* Interrupt Condition flags - TX/RX/MS/LS */

  /* The following are serial port registers */
  int dll, dlm;		/* Baudrate divisor LSB and MSB */
  u_char IER;		/* Interrupt Enable Register */
  struct iir IIR;	/* Interrupt Identification Register */
  u_char LCR;		/* Line Control Register */
  u_char FCReg;		/* Fifo Control Register (.FCR is a name conflict) */
  u_char MCR;		/* Modem Control Register */
  u_char LSR;		/* Line Status Register */
  u_char MSR;		/* Modem Status Register */
  u_char SCR;		/* Scratch Pad Register */

  /* The following are the transmit and receive buffer variables
   * They are bigger than the 16 bytes of a real FIFO to improve
   * performance, but the 16-byte limitation of the receive FIFO
   * is still emulated using a counter, to improve compatibility.
   */
  u_char rx_buf[RX_BUFFER_SIZE];	/* Receive Buffer */
  u_char rx_buf_start;			/* Receive Buffer queue start */
  u_char rx_buf_end;			/* Receive Buffer queue end */

  int tx_cnt;
  int fossil_blkrd_tid;

  struct termios oldset;		/* Original termios settings */
  struct termios newset;		/* Current termios settings */

  struct serial_drv *drv;
} com_t;

extern com_t com[MAX_SER];

#define RX_BUF_BYTES(num) (com[num].rx_buf_end - com[num].rx_buf_start)
//#define RX_FIFO_BYTES(num) min(RX_BUF_BYTES(num), com[num].rx_fifo_size)
#define TX_BUF_BYTES(num) (com[num].tx_buf_end - com[num].tx_buf_start)
#define INT_REQUEST(num)  (com[num].int_condition & com[num].IER)
#define INT_ENAB(num)  (com[num].MCR & UART_MCR_OUT2)
#define TX_TRIGGER(num) (!(com[num].LSR & UART_LSR_THRE))
#define FIFO_ENABLED(num) (com[num].IIR.flg.fifo_enable == IIR_FIFO_ENABLE)
#define DLAB(num) (com[num].LCR & UART_LCR_DLAB)

/*******************************************************************
 *  Function declarations in order to resolve function references  *
 *******************************************************************/

int convert_bit(int, int, int);
void serial_int_engine(int, int);
void serial_timer_update(void);
void uart_clear_fifo(int, int);
int pic_serial_run(int);
void fossil_int14(int);
void ser_termios(int num);
void modstat_engine(int num);
int msr_compute_delta_bits(int oldmsr, int newmsr);
int ser_open(int num);
void receive_engine(int num, int size);
void transmit_engine(int num);
void rx_buffer_slide(int num);
void tx_buffer_slide(int num);
int serial_get_tx_queued(int num);
void serial_update(int num);

void rx_buffer_dump(int num);
void tx_buffer_dump(int num);
int serial_get_tx_queued(int num);
void ser_termios(int num);
int serial_brkctl(int num, int brkflg);
ssize_t serial_write(int num, char *buf, size_t len);
int serial_dtr(int num, int flag);
int serial_rts(int num, int flag);
int ser_open(int num);
int ser_close(int num);
int uart_fill(int num);
int serial_get_msr(int num);

struct serial_drv {
  void (*rx_buffer_dump)(com_t *com);
  void (*tx_buffer_dump)(com_t *com);
  int (*serial_get_tx_queued)(com_t *com);
  void (*ser_termios)(com_t *com);
  int (*serial_brkctl)(com_t *com, int brkflg);
  ssize_t (*serial_write)(com_t *com, char *buf, size_t len);
  int (*serial_dtr)(com_t *com, int flag);
  int (*serial_rts)(com_t *com, int flag);
  int (*ser_open)(com_t *com);
  int (*ser_close)(com_t *com);
  int (*uart_fill)(com_t *com);
  int (*serial_get_msr)(com_t *com);
  char *name;
};

#endif /* SER_DEFS_H */
