/*
 * $Log: serial.h,v $
 * Revision 2.2  1994/11/03  11:43:26  root
 * Checkin Prior to Jochen's Latest.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.20  1994/05/13  17:21:00  root
 * pre51_15.
 *
 * Revision 2.9.1.1  1994/05/10  22:53:03  root
 * Corresponds to serial 2.9.1.1, may be final for DOSEMU 0.52
 *
 * Revision 1.19  1994/04/09  18:41:52  root
 * Prior to Lutz's kernel enhancements.
 *
 * Revision 2.9.0.6  1994/04/08  00:20:39  root
 * Corressponds to serial.c 2.9.0.6
 *
 * Revision 2.9.0.5  1994/04/07  06:42:44  root
 * This got into pre0.51 release 4.
 *
 * Revision 2.9  1994/04/06  00:55:51  root
 * Made serial config more flexible, and up to 4 ports.
 *
 * Revision 1.16  1994/04/04  22:51:55  root
 * Patches for PS/2 mouse.
 *
 * Revision 2.8  1994/03/31  22:57:34  root
 * Corresponds to 2.8 of serial.c
 *
 * Revision 2.7  1994/03/30  08:07:51  root
 * New variables for 2.7
 *
 * Revision 2.5.1.1  1994/03/26  09:59:00  root
 * This is what got into pre0.51 (with a minor modification)
 *
 * Revision 2.5  1994/03/24  07:15:48  root
 * CHECKPOINT: This may go into the 0.51 release.
 *
 * Revision 2.2  1994/03/23  07:14:52  root
 * Survived interrupts overhaul.  From now on, I correspond serial.h
 * as closely as possible to serial.c
 *
 */

#ifndef SERIAL_H
#define SERIAL_H

/* the UART definitions are quoted from linux/include/linux/serial.h, a work
 * by Theodore Ts'o
 */

/*
 * These are the UART port assignments, expressed as offsets from the base
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

/*
 * These are the definitions for the FIFO Control Register
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

/*
 * These are the definitions for the Line Control Register
 *
 * Note: if the word length is 5 bits (UART_LCR_WLEN5), then setting
 * UART_LCR_STOP will select 1.5 stop bits, not 2 stop bits.
 */
#define UART_LCR_DLAB	0x80	/* Devisor latch access bit */
#define UART_LCR_SBC	0x40	/* Set break control */
#define UART_LCR_SPAR	0x20	/* Stick parity (?) */
#define UART_LCR_EPAR	0x10	/* Even paraity select */
#define UART_LCR_PARITY	0x08	/* Parity Enable */
#define UART_LCR_STOP	0x04	/* Stop bits: 0=1 stop bit, 1= 2 stop bits */
#define UART_LCR_WLEN5  0x00	/* Wordlength: 5 bits */
#define UART_LCR_WLEN6  0x01	/* Wordlength: 6 bits */
#define UART_LCR_WLEN7  0x02	/* Wordlength: 7 bits */
#define UART_LCR_WLEN8  0x03	/* Wordlength: 8 bits */
#define UART_LCR_PARA   0x1f    /* Parity, Stop bits and Wordlength */

/*
 * These are the definitions for the Line Status Register
 */
#define UART_LSR_TEMT	0x40	/* Transmitter empty */
#define UART_LSR_THRE	0x20	/* Transmit-hold-register empty */
#define UART_LSR_BI	0x10	/* Break interrupt indicator */
#define UART_LSR_FE	0x08	/* Frame error indicator */
#define UART_LSR_PE	0x04	/* Parity error indicator */
#define UART_LSR_OE	0x02	/* Overrun error indicator */
#define UART_LSR_DR	0x01	/* Receiver data ready */
#define UART_LSR_ERR    0x1e    /* All the error indicators */

/*
 * These are the definitions for the Interrupt Indentification Register
 */
#define UART_IIR_NO_INT	0x01	/* No interrupts pending */
#define UART_IIR_MSI	0x00	/* Modem status interrupt */
#define UART_IIR_THRI	0x02	/* Transmitter holding register empty */
#define UART_IIR_RDI	0x04	/* Receiver data interrupt */
#define UART_IIR_RLSI	0x06	/* Receiver line status interrupt */
#define UART_IIR_CTI    0x0c	/* Character timeout indication */
#define UART_IIR_ID	0x06	/* Mask for the interrupt ID */
#define UART_IIR_FIFO_ENABLE_1 0x40
#define UART_IIR_FIFO_ENABLE_2 0x80
#define UART_IIR_FIFO (UART_IIR_FIFO_ENABLE_1|UART_IIR_FIFO_ENABLE_2)

/*
 * These are the definitions for the Interrupt Enable Register
 */
#define UART_IER_MSI	0x08	/* Enable Modem status interrupt */
#define UART_IER_RLSI	0x04	/* Enable receiver line status interrupt */
#define UART_IER_THRI	0x02	/* Enable Transmitter holding register int. */
#define UART_IER_RDI	0x01	/* Enable receiver data interrupt */

/*
 * These are the definitions for the Modem Control Register
 */
#define UART_MCR_LOOP	0x10	/* Enable loopback test mode */
#define UART_MCR_OUT2	0x08	/* Out2 complement */
#define UART_MCR_OUT1	0x04	/* Out1 complement */
#define UART_MCR_RTS	0x02	/* RTS complement */
#define UART_MCR_DTR	0x01	/* DTR complement */
#define UART_MCR_VALID	0x1F	/* The valid registers of the MCR */

/*
 * These are the definitions for the Modem Status Register
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

#include <termios.h>

/* These are baudrate divisors.  BaudRate = 1843200 / (DIVISOR * 16) */
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

/* Interrupts pending flag bits for com[num].int_type */
#if NEW_PIC==2
/* These bits must match the UART_IER_xxx bits, above */
#define RX_INTR    UART_IER_RDI  /* 1 */
#define TX_INTR    UART_IER_THRI /* 2 */
#define LS_INTR    UART_IER_RLSI /* 4 */
#define MS_INTR    UART_IER_MSI  /* 8 */
#else
#define MS_INTR    1
#define TX_INTR    2
#define RX_INTR    4
#define LS_INTR    8
#endif

/* The following sets the size of receive and transmit FIFOs.        */
/* They MUST be a power of 2 in order to work.  Bigger FIFOs can     */
/* speed throughput in certain cases, but may not be as compatible.  */
/* Don't exceed 512 bytes, to prevent DOS "Stack Overflow" stupdity. */
#define RX_FIFO_SIZE            16
#define TX_FIFO_SIZE            16

int stage;			/* Stage counter for serial interrupts */
u_char irq_source_num[255];	/* Index to map from IRQ no. to serial port */
u_char com_port_used[17];       /* Used for auto-assign comport config */
typedef struct serial_struct {
  char dev[255];		/* String to hold path to device file */
  int real_comport;		/* The actual COMx port number. 0 for invalid */
  int base_port;		/* Base port address handled by device */
  int interrupt;		/* IRQ line handled by device */
  int fd;			/* File descriptor of device */
  boolean mouse;		/* Flag to turn on mouse sharing features */

  u_char int_pend;		/* Interrupt Pending Flag */
  u_char int_type;		/* Interrupts Waiting Flags */
  u_char int_enab;		/* Interrupts Enable = OUT2 of MCR */
  u_char tx_timeout;		/* Receive Interrupt timeout counter */
  u_char rx_timeout;		/* Transmit Interrupt timeout counter */
  u_char ms_counter;            /* [FUTURE USE] Counter to forced MS check */
  u_char uart_full;		/* UART full flag */
  u_char fifo_enable;		/* FIFO enabled flag */
  u_char tx_overflow;		/* Full outgoing buffer flag. */

  int dll, dlm;		/* Baudrate divisor LSB and MSB */
  u_char TX;		/* Transmit Holding Register */
  u_char RX;		/* Received Data Register */
  u_char IER;		/* Interrupt Enable Register */
  u_char IIR;		/* Interrupt Identification Register */
  u_char LCR;		/* Line Control Register */
  u_char FCReg;		/* Fifo Control Register (name conflict) */
  u_char MCR;		/* Modem Control Register */
  u_char LSR;		/* Line Status Register */
  u_char MSR;		/* Modem Status Register */
  u_char SCR;		/* Scratch Pad Register */

  u_char RX_FIFO[RX_FIFO_SIZE];		/* Receive Receive Fifo */
  u_char RX_FIFO_START;			/* Receive Fifo queue start */
  u_char RX_FIFO_END;			/* Receive Fifo queue end */
  u_char RX_FIFO_TRIGGER;		/* Receive Fifo trigger value */
  u_char RX_FIFO_BYTES;			/* # of bytes in Receive Fifo */
  u_char TX_SHIFT;			/* Transmit shift register */
  u_char TX_FIFO[TX_FIFO_SIZE];		/* Transmit Fifo */
  u_char TX_FIFO_START;			/* Transmit Fifo queue start */
  u_char TX_FIFO_END;			/* Transmit Fifo queue end */
  u_char TX_FIFO_BYTES;			/* # of bytes in Transmit Fifo */
  u_char DLAB;				/* Divisor Latch enabled */

  struct termios oldset;
  struct termios newset;
  speed_t newbaud;
} serial_t;

int debug1,debug2;			/* Global debugging variables */
int count1,count2;			/* Used only by programmers   */

#define MAX_SER 4
extern serial_t com[MAX_SER];
extern inline void serial_run(void);
extern inline int do_serial_in(int, int);
extern inline int do_serial_out(int, int, int);

#endif /* SERIAL_H */
