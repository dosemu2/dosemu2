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
#define UART_IIR_ID	0x0e	/* Mask for the interrupt ID */
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

/* different types of mice */
#define MOUSE_MICROSOFT     0
#define MOUSE_MOUSESYSTEMS3 1
#define MOUSE_MOUSESYSTEMS5 2
#define MOUSE_MMSERIES      3
#define MOUSE_LOGITECH      4

typedef struct serial_struct {
  char dev[255];
  int base_port;
  int interrupt;

  int fd;

  int dready;
  u_char in_rxinterrupt;	/* Receive interrupt flag */
  u_char in_txinterrupt;	/* Transmit interrupt flag */
  u_char in_msinterrupt;	/* Modem status interrupt flag */
  u_char in_lsinterrupt;        /* Line status interrupt flag */

  boolean mouse;		/* set if mouse sharing activated */
  int mtype;			/* type of mouse */
  boolean modem;		/* modem */

  /* UART info */
  int dll, dlm;
  u_char TX;
  u_char RX;
  u_char IER;
  u_char IIR;
  u_char LCR;
  u_char FCReg;		/* "FCR" exists somewhere else, so I use "FCReg" */
  u_char MCR;
  u_char LSR;
  u_char MSR;
  u_char SCR;

  struct termios oldsettings;
  u_char RX_FIFO[16];
  u_char RX_FIFO_START;
  u_char RX_FIFO_END;
  u_char RX_FIFO_TRIGGER_VAL;
  u_char RX_BYTES_IN_FIFO;
  u_char TX_SHIFT;
  u_char TX_FIFO[16];
  u_char TX_FIFO_START;
  u_char TX_FIFO_END;
  u_char TX_BYTES_IN_FIFO;
  u_char DLAB;
  struct termios newsettings;
  speed_t newbaud;

} serial_t;

/*#define DLAB(num) (com[num].LCR & UART_LCR_DLAB)*/

extern serial_t com[MAX_SER];

#define MAX_SER 2

/* SER_QUEUE_LEN *MUST* be a power of 2 minus one. */
#define SER_QUEUE_LEN   (1024)
#define SER_QUEUE_LOOP  (SER_QUEUE_LEN - 1)       /* For speed's sake */
 
struct ser_param {
  volatile char queue[SER_QUEUE_LEN];
  volatile int start, end;
  volatile int num_ints;
  volatile u_char dready;
};

typedef struct param_struct {
  volatile struct ser_param ser[MAX_SER];
} param_t;

param_t serial_parm, *param;

#endif /* SERIAL_H */
