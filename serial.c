/* serial.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * Basic 16550 UART emulation.
 *
 * $Date: 1994/03/18 23:17:51 $
 * $Source: /home/src/dosemu0.50pl1/RCS/serial.c,v $
 * $Revision: 1.21 $
 * $State: Exp $
 *
 * $Log: serial.c,v $
 * Revision 1.21  1994/03/18  23:17:51  root
 * Latest from Mark Rejhon <mdrejhon@undergrad.math.uwaterloo.ca>.
 *
 * Revision 1.84  1994/03/18  03:00:18  root
 * Bugfixes to 1.83.  Major updates were rearranging the order of subroutines,
 * as well as changing the way some UART bits were handled, especially the
 * interrupt bits.
 *
 * Revision 1.83  1994/03/18  00:11:43  root
 * Major update...
 *
 * Revision 1.82  1994/03/16  18:39:42  root
 * Yay!  The modem status interrupt finally works.  Now my FOSSIL driver
 * works with it!
 *
 * Revision 1.81  1994/03/16  10:25:21  root
 * Finally works with dosemu0.60d
 *
 * Revision 1.80  1994/03/15  02:00:20  root
 * Patched Jame's pre6d changes into my working version of serial.c
 *
 * Revision 1.78  1994/03/14  19:30:39  root
 * Encapsulated the QSTART and QEND variables to queue specific routines
 *
 * Revision 1.76  1994/03/14  05:45:41  root
 * Great.  Updated baudrate change code to be more flexible.
 * Now rounds up to the nearest baudrate (or down to 38400 if above 38400)
 *
 * Revision 1.73  1994/03/14  03:35:22  root
 * Hmm, doesn't work well.  Wonder what went wrong?!
 *
 * Revision 1.71  1994/03/13  09:10:42  root
 * Ok... a couple more fixes.
 *
 * Revision 1.70  1994/03/13  08:52:46  root
 * Finally some more bugfixes.  Also added Overrun error detection.
 *
 * Revision 1.64  1994/03/13  05:39:36  root
 * So far it works... Debug output is more clean!
 *
 * Revision 1.63  1994/03/13  04:57:27  root
 * A few bugfixes there and here, as well as more debug.
 *
 * Revision 1.62  1994/03/13  04:25:09  root
 * Finally merged my 0.60b modifications to 0.60c version of serial.c
 *
 * Revision 1.61  1994/03/13  03:09:29  root
 * Updated for today's debugging session, basing on fixes of 1.51+1.60
 *
 * Revision 1.51  1994/03/12  01:42:23  root
 * Whoopsie doo... A few compile errors fixed.
 *
 * Revision 1.50  1994/03/12  01:36:53  root
 * MAJOR update of the INT 14 interface.  Now to try it out... =:-)
 *
 * Revision 1.43  1994/03/11  23:36:27  root
 * Gasp.  Gross INT 14 interface.  Time for a major redesign. :-)
 *
 * Revision 1.42  1994/03/11  23:09:20  root
 * Made default at 2400 baud 8N1....without compromising stability so far.
 *
 * Revision 1.41  1994/03/11  23:00:16  root
 * 1.4 didn't work.  Bleaugh.   Starting over from 1.3, with some corrections.
 *
 * Revision 1.3  1994/03/11  22:13:05  root
 * Ooops. Removed two garb chars in 1.2
 *
 * Revision 1.2  1994/03/11  22:09:20  root
 * Fixed infamous kludge / added some dbg messages to get_msr
 *
 * Revision 1.1  1994/03/11  22:03:40  root
 * Initial revision
 *
 * Revision 1.17  1994/03/10  02:49:27  root
 * Back to 1 process.
 *
 * Revision 1.16  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.15  1994/03/04  00:01:58  root
 * Ronnie's SERIAL enhancements.
 *
 * Revision 1.14  1994/02/21  20:28:19  root
 * Serial patches of Ronnie's
 *
 * Revision 1.11  1994/02/13  21:46:40  root
 * Ronnie's first set of serial enhancements.
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.3  1993/07/19  16:37:37  rsanders
 * minor change
 *
 * Revision 1.2  1993/07/14  04:27:33  rsanders
 * removed erroneous error message
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.1  1993/05/04  05:29:22  root
 * Initial revision
 *
 *
 */

#define NO_PAUSE 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>

#include "config.h"
#include "cpu.h"
#include "emu.h"
#include "dosio.h"
#include "serial.h"

extern int_count[8];

extern void queue_hard_int(int i, void (*), void (*));
extern struct CPU cpu;
extern config_t config;
extern pid_t parent_pid, ser_pid;

inline int get_msr(int num);

serial_t com[MAX_SER];

#define QUEUE  (param->ser[num].queue)
#define QSTART (param->ser[num].start)
#define QEND   (param->ser[num].end)

inline int
ser_avail(int num) 
{
  return (QSTART != QEND);
}

/* This routine is not used right now, but may be useful in the future
inline int
ser_queue_free(int num) 
{
  return ((QSTART <= QEND) ? (QEND - QSTART) : (QEND + SER_QUEUE_LEN - QSTART));
}
*/

inline void
ser_clear_queue(int num)
{  
  QSTART = 0;  QEND = 0;
}

inline int
ser_enqueue(int num, int byte)
{
  if (QSTART == (QEND + 1) & SER_QUEUE_LOOP) {
    com[num].LSR |= UART_LSR_OE;     /* Overrun error added by Mark Rejhon */
    s_printf("COM%d: Virtual queue OVERRUN!\n",num);
    return 0;
  }
  QUEUE[QEND] = byte;
  QEND = (QEND + 1) & SER_QUEUE_LOOP;
  return 1;
}

inline int
ser_dequeue(int num)
{
  int byte;

  if (!ser_avail(num)) {
    error("ERROR: COM: in ser_dequeue(), QEND == QSTART!\n");
    return -1;
  }
  byte = QUEUE[QSTART];
  QSTART = (QSTART + 1) & SER_QUEUE_LOOP;
  return byte;
}

int
ser_open(int num)
{
  s_printf("COM%d: Running ser_open\n",num);
  ser_clear_queue(num);
  if (com[num].fd != -1) {
    return (com[num].fd);
  }
  com[num].fd = DOS_SYSCALL(open(com[num].dev, O_RDWR | O_NONBLOCK));
  DOS_SYSCALL(tcgetattr(com[num].fd, &com[num].oldsettings));
  DOS_SYSCALL(tcsetattr(com[num].fd, TCSANOW, &com[num].newsettings));
  return (com[num].fd);
}

int
ser_close(int num)
{
  int i;
  s_printf("COM%d: Running ser_close\n",num);

  ser_clear_queue(num);
  if (com[num].fd == -1) {
    return (0);
  }
  /* save current dosemu settings of the file and restore the old settings
     before closing the file down. */
  DOS_SYSCALL(tcgetattr(com[num].fd, &com[num].newsettings));
  DOS_SYSCALL(tcsetattr(com[num].fd, TCSANOW, &com[num].oldsettings));
  i = DOS_SYSCALL(close(com[num].fd));
  com[num].fd = -1;
  return (i);
}

void
ser_termios(int num)
{
  speed_t baud;
  int rounddiv;

  /* The following is the same as (com[num].dlm * 256) + com[num].dll */
#define DIVISOR ((com[num].dlm<<8)|com[num].dll)

  /* return if not a tty */
  if (tcgetattr(com[num].fd, &com[num].newsettings) == -1) {
    s_printf("COM%d: Not a TTY!\n",num);
    return;
  }
  s_printf("COM%d: LCR = 0x%x, ",num,com[num].LCR);

  /* set word size */
  com[num].newsettings.c_cflag &= ~CSIZE;
  switch (com[num].LCR & UART_LCR_WLEN8) {
  case UART_LCR_WLEN5:
    com[num].newsettings.c_cflag |= CS5;
    s_printf("5");
    break;
  case UART_LCR_WLEN6:
    com[num].newsettings.c_cflag |= CS6;
    s_printf("6");
    break;
  case UART_LCR_WLEN7:
    com[num].newsettings.c_cflag |= CS7;
    s_printf("7");
    break;
  case UART_LCR_WLEN8:
    com[num].newsettings.c_cflag |= CS8;
    s_printf("8");
    break;
  }

  /* set parity */
  if (com[num].LCR & UART_LCR_PARITY) {
    com[num].newsettings.c_cflag |= PARENB;
    if (com[num].LCR & UART_LCR_EPAR) {
      com[num].newsettings.c_cflag &= ~PARODD;
      s_printf("E");
    }
    else {
      com[num].newsettings.c_cflag |= PARODD;
      s_printf("O");
    }
  }
  else {
    com[num].newsettings.c_cflag &= ~PARENB;
    s_printf("N");
  }

  /* stop bits: UART_LCR_STOP set means 2 stop bits, 1 otherwise */
  if (com[num].LCR & UART_LCR_STOP) {
    com[num].newsettings.c_cflag |= CSTOPB;
    s_printf("2, ");
  }
  else {
    com[num].newsettings.c_cflag &= ~CSTOPB;
    s_printf("1, ");
  }

  /* The following sets the baudrate.  These nested IF statements rounds 
  ** upwards to the next higher baudrate. (ie, rounds downwards to the next 
  ** valid divisor value) The formula is:  bps = 1843200 / (divisor * 16)
  */
  if (DIVISOR < DIV_38400) {		/* above 38400, use 38400 baud */
    s_printf("bps = %d, using 38400, ", 1843200 / (DIVISOR * 16));
    rounddiv = DIV_38400;
    baud = B38400;
  } else if (DIVISOR < DIV_19200) {			/* 38400 baud */
    s_printf("bps = 38400, ");
    rounddiv = DIV_38400;
    baud = B38400;
  } else if (DIVISOR < DIV_9600) {			/* 19200 baud */
    s_printf("bps = 19200, ");
    rounddiv = DIV_19200;
    baud = B19200;
  } else if (DIVISOR < DIV_4800) {			/* 9600 baud */
    s_printf("bps = 9600, ");
    rounddiv = DIV_9600;
    baud = B9600;
  } else if (DIVISOR < DIV_2400) {			/* 4800 baud */
    s_printf("bps = 4800, ");
    rounddiv = DIV_4800;
    baud = B4800;
  } else if (DIVISOR < DIV_1800) {			/* 2400 baud */
    s_printf("bps = 2400, ");
    rounddiv = DIV_2400;
    baud = B2400;
  } else if (DIVISOR < DIV_1200) {			/* 1800 baud */
    s_printf("bps = 1800, ");
    rounddiv = DIV_1800;
    baud = B1800;
  } else if (DIVISOR < DIV_600) {			/* 1200 baud */
    s_printf("bps = 1200, ");
    rounddiv = DIV_1200;
    baud = B1200;
  } else if (DIVISOR < DIV_300) {			/* 600 baud */
    s_printf("bps = 600, ");
    rounddiv = DIV_600;
    baud = B600;
  } else if (DIVISOR < DIV_150) {			/* 300 baud */
    s_printf("bps = 300, ");
    rounddiv = DIV_300;
    baud = B300;
  } else if (DIVISOR < DIV_110) {			/* 150 baud */
    s_printf("bps = 150, ");
    rounddiv = DIV_150;
    baud = B150;
  } else if (DIVISOR < DIV_50) {			/* 110 baud */
    s_printf("bps = 110, ");
    rounddiv = DIV_110;
    baud = B110;
  } else {						/* 50 baud */
    s_printf("bps = 50, ");
    rounddiv = DIV_50;
    baud = B50;
  }
  s_printf("divisor 0x%x -> 0x%x\n", DIVISOR, rounddiv);

  DOS_SYSCALL(cfsetispeed(&com[num].newsettings, baud));
  DOS_SYSCALL(cfsetospeed(&com[num].newsettings, baud));
  com[num].newbaud = baud;
  DOS_SYSCALL(tcsetattr(com[num].fd, TCSANOW, &com[num].newsettings));
}

void
do_ser_init(int num)
{
  com[num].in_rxinterrupt = 0;
  com[num].fd = DOS_SYSCALL(open(com[num].dev, O_RDWR | O_NONBLOCK));

  param->ser[num].start = 0;
  param->ser[num].end = 0;

#if 0
  /* set both up for 1200 baud.  no workee yet */
  com[num].dll = 0x60;
  com[num].dlm = 0;
#else /* Mark Rejhon's modifiction for default 2400 baud, 8N1 parameters */
  com[num].dll = 0x30;
  com[num].dlm = 0;
  com[num].LCR = UART_LCR_WLEN8;
#endif

  DOS_SYSCALL(tcgetattr(com[num].fd, &com[num].oldsettings));
  s_printf("COM%d: ser_termios in do_ser_init\n",num);
  ser_termios(num);

  if (com[num].modem) {
    com[num].newsettings.c_cflag |= (CLOCAL);
    com[num].newsettings.c_cflag &= (~HUPCL);
    DOS_SYSCALL(tcsetattr(com[num].fd, TCSANOW, &com[num].newsettings));
  }

  /* reopen file and set attributes. */
  ser_close(num);
  ser_open(num);

  *((u_short *) (0x400) + num) = com[num].base_port;
  g_printf("SERIAL: num %d, port 0x%x, address 0x%x, mem 0x%x\n", num,
         com[num].base_port, (int)((u_short *) (0x400) + num), *((u_short *) (0x400) + num));

  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  com[num].IIR = UART_IIR_NO_INT;
  com[num].LCR = UART_LCR_WLEN8;
  com[num].IER = 0;
  com[num].MSR = get_msr(num);
  com[num].FCReg = 0; 
  com[num].MCR = 0;
  com[num].DLAB = 0;
}

void
serial_init(void)
{
  int i;

  *(u_short *) 0x400 = 0;
  *(u_short *) 0x402 = 0;
  *(u_short *) 0x404 = 0;
  *(u_short *) 0x406 = 0;
  param=&serial_parm;

  /* do UART init here - need to setup registers*/
  for (i = 0; i < config.num_ser; i++) {
    com[i].fd = -1;
    do_ser_init(i);
  }
  warn("COM: serial init, %d serial ports\n", config.num_ser);
}

void
child_close_mouse()
{
  u_char i, rtrn;

  s_printf("MOUSE: About to close\n");
  for (i = 0; i < config.num_ser; i++) {
    s_printf("MOUSE: is %d, i=%d, dev=%s\n", com[i].mouse, i, com[i].dev);
    if (com[i].mouse && (com[i].fd > 0)) {
      s_printf("MOUSE: legal %d fd=%d\n", com[i].mouse, com[i].fd);
      rtrn = ser_close(i);
      if (rtrn)
	s_printf("MOUSE SERIAL ERROR - %s\n", strerror(errno));
      s_printf("MOUSE: close %d\n", i);
    }
  }
}

void
child_open_mouse()
{
  u_char i;

  s_printf("MOUSE: About to open\n");
  for (i = 0; i < config.num_ser; i++) {
    if (com[i].mouse) {
      s_printf("MOUSE: legal %d, i=%d, dev=%s\n", com[i].mouse, i, com[i].dev);
      ser_open(i);
      s_printf("MOUSE: opened %d\n", i);
#if 0
      {
	struct termios mouse;
	static const unsigned short cflag[] =
	{
	  (CS7 | CREAD | CLOCAL | HUPCL),		/* MicroSoft */
	  (CS8 | CSTOPB | CREAD | CLOCAL | HUPCL),	/* MouseSystems 3 */
	  (CS8 | CSTOPB | CREAD | CLOCAL | HUPCL),	/* MouseSystems 5 */
	  (CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL),	/* MMSeries */
	  (CS8 | CSTOPB | CREAD | CLOCAL | HUPCL)	/* Logitech */
	};

	tcgetattr(com[i].fd, &mouse);
	mouse.c_cflag = cflag[com[i].mtype] | com[i].newbaud;
	mouse.c_iflag = IGNBRK | IGNPAR;
	mouse.c_oflag = 0;
	mouse.c_lflag = 0;
	mouse.c_line = 0;
	mouse.c_cc[VTIME] = 0;
	mouse.c_cc[VMIN] = 1;
	tcsetattr(com[i].fd, TCSANOW, &mouse);
	write(com[i].fd, "*n", 2);
	usleep(100000);
	tcsetattr(com[i].fd, TCSANOW, &mouse);
	tcgetattr(com[i].fd, &com[i].newsettings);
	{
	  /* discard any trash that might bee sent by the mouse */
	  char dummy[25];

	  usleep(2500000);
	  while (read(com[i].fd, dummy, 20) == 20) ;
	}
      }
#endif
    }
  }
}

void
serial_close(void)
{
  int i;

  s_printf("COM: serial_close\n");
  for (i = 0; i < config.num_ser; i++) {
    DOS_SYSCALL(tcsetattr(com[i].fd, TCSANOW, &com[i].oldsettings));
    ser_close(i);
  }
}

inline void
check_serial_ready()
{
#define BYTES_SIZE 20
  int i, x, size;
  u_char bytes[BYTES_SIZE];

  /* This is the where the serial port is _actually_ read */
  for (x = 0; x < config.num_ser; x++)
    if (com[x].fd != -1)
      if ((size = read(com[x].fd, bytes, BYTES_SIZE)) > 0)
	for (i = 0; 
            (i < size) && ser_enqueue(x, bytes[i]); 
            i++);
}

int
beg_rxint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  if (com[num].FCReg & UART_FCR_ENABLE_FIFO) {
    /* fifos are enabled, fill as much as possible before interrupt is made.
    */
    while ((com[num].RX_BYTES_IN_FIFO < 16) && (ser_avail(num))) {
      com[num].RX_BYTES_IN_FIFO++;
      com[num].RX_FIFO[com[num].RX_FIFO_END] = ser_dequeue(num);
      com[num].RX_FIFO_END = (com[num].RX_FIFO_END + 1) & 15;
    }

    /* if trigger is exceeded we issue a ReceivedDataAvailable intr.
    ** othervise we do a CharacterTimeoutIndication.
    */
    if (com[num].RX_BYTES_IN_FIFO < com[num].RX_FIFO_TRIGGER_VAL)
      com[num].IIR = UART_IIR_RDI | UART_IIR_FIFO;
    else
      com[num].IIR = UART_IIR_CTI | UART_IIR_FIFO;
  }
  else {
    /* normal mode, copy one byte to RX register before issuing interrupt. */
    com[num].RX = ser_dequeue(num);
    com[num].IIR = UART_IIR_RDI;
  }

  com[num].LSR |= UART_LSR_DR;
  com[num].in_rxinterrupt = 1;
  return 0;
}

int
end_rxint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  com[num].in_rxinterrupt = 0;
  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
  serial_run();			/* check if new data has arrived. */
  return 0;
}

/* Now get_rx contains the merged original get_rx and get_fiforx.
** This function runs when the user program reads from the fifo data register.
*/
inline int
get_rx(int num)
{
  if (com[num].FCReg & UART_FCR_ENABLE_FIFO) {
    int val;
    /* if there are data in fifo then read from from the fifo.
    ** if the fifo is empty, read 0. (is this correct ??? XXXXXXXXXXX)
    */
    if (com[num].RX_BYTES_IN_FIFO) {
      val = com[num].RX_FIFO[com[num].RX_FIFO_START];
      com[num].RX_FIFO_START = (com[num].RX_FIFO_START + 1) & 15;
      com[num].RX_BYTES_IN_FIFO--;
    }
    else
      val = 0;

    /* if fifo is empty, we will no longer flag data-ready. */
    if (!com[num].RX_BYTES_IN_FIFO) {
      com[num].dready = 0;
      com[num].LSR &= ~UART_LSR_DR;
    }

    /* if less data in fifo than trigger, dont indicate interrupt anymore */
    if (com[num].RX_BYTES_IN_FIFO < com[num].RX_FIFO_TRIGGER_VAL) {
      com[num].IIR = UART_IIR_FIFO | UART_IIR_NO_INT;
    }
    return (val);
  }
  else {		/* not in FIFO mode */
    com[num].dready = 0;
    com[num].LSR &= ~UART_LSR_DR;
    com[num].IIR = UART_IIR_NO_INT;
    return com[num].RX;
  }
}

int
beg_msi(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;   /* XXXX Needs change */

  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_MSI;
  return 0;
}

int
end_msi(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;   /* XXXX Needs change */

  com[num].in_msinterrupt = 0;		/* no longer pending interrupt */
  return 0;
}

inline int
msr_bits(int num)			/* Function returns modem status */
{
#define MSR_BIT(c,s,d) ((c & s) ? d : 0)
  int control;
  ioctl(com[num].fd, TIOCMGET, &control);
  return MSR_BIT(control, TIOCM_CTS, UART_MSR_CTS) |
         MSR_BIT(control, TIOCM_DSR, UART_MSR_DSR) |
         MSR_BIT(control, TIOCM_RNG, UART_MSR_RI) |
         MSR_BIT(control, TIOCM_CAR, UART_MSR_DCD);
}

inline int
get_msr(int num)
{
#define DELTA_BIT(o,n,b) ((o ^ n) & b)

  int oldmsr = com[num].MSR;

  /* Clear the Modem Status Interrupt flag */
  if ((com[num].IIR & UART_IIR_ID) == UART_IIR_MSI)
    com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;

  com[num].MSR = msr_bits(num);		/* Get status bits */

  /* This sets the delta CTS bit */
  if (DELTA_BIT(oldmsr, com[num].MSR, UART_MSR_CTS)) {
    com[num].MSR |= UART_MSR_DCTS;
    s_printf("COM%d: CTS change to %d\n", num, com[num].MSR & UART_MSR_CTS);
  }

  /* This sets the delta DSR bit */
  if (DELTA_BIT(oldmsr, com[num].MSR, UART_MSR_DSR))
    com[num].MSR |= UART_MSR_DDSR;

  /* This sets the trailing edge RI bit (when RI goes from low to high) */
  if ((oldmsr & UART_MSR_RI) < (com[num].MSR & UART_MSR_RI)) {
    com[num].MSR |= UART_MSR_TERI;
    s_printf("COM%d: RI detected %x!\n", num, com[num].MSR & UART_MSR_RI);
  }

  /* This sets the delta DCD bit */
  if (DELTA_BIT(oldmsr, com[num].MSR, UART_MSR_DCD)) {
    com[num].MSR |= UART_MSR_DDCD;
    s_printf("COM%d: DCD change to %d\n", num, com[num].MSR & UART_MSR_DCD);
  }

  return com[num].MSR;
}

inline void
serial_int_msr(int num)
{
  int newmsr = msr_bits(num);

  if (com[num].MSR & UART_MSR_RI)  /* Don't interrupt if RI goes off, not on */
    newmsr |= UART_MSR_RI;

  if (newmsr != (com[num].MSR & UART_MSR_STAT)) {
    com[num].in_msinterrupt = 1;
    s_printf("COM%d: Modem status interrupt!\n",num);
    queue_hard_int(com[num].interrupt, beg_msi, end_msi);
  }
  /* XXXXXXX Maybe I should update part of MSR here? */
}

int
beg_txint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_THRI;
  return 0;
}

int
end_txint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
  com[num].in_txinterrupt = 0;		/* no longer pending interrupt. */
  serial_run();                         /* check if new data has arrived. */
  return 0;
}

inline void
put_tx(int num, int val)
{
  com[num].TX = val;

  /* loopback */
  if (com[num].MCR & UART_MCR_LOOP) {
    s_printf("COM: loopback write!\n");
    /* XXXXXXXXXXXXX should really be changed */
    ser_enqueue(num, val);
    return;
  }
  else {
    RPT_SYSCALL(write(com[num].fd, &val, 1));
    com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;
  }

  if ((com[num].IER & UART_IER_THRI) && (com[num].MCR & UART_MCR_OUT2)) {
    com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_THRI;

    /* queue tx interrupt, if non pending. */
    if (!com[num].in_txinterrupt) {
      com[num].in_txinterrupt = 1;
      queue_hard_int(com[num].interrupt, beg_txint, end_txint);
    }
  } 
  else {
    com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
  }
}

/* XMIT and RCVR fifos are cleared when changing from fifo mode to
** 16540 modes and vice versa.
** This function will clear the specified fifo.
*/
void
uart_clear_fifo(int num, int fifo)
{
  if (fifo & UART_FCR_CLEAR_RCVR) {
    com[num].RX_FIFO_START = 0;
    com[num].RX_FIFO_END = 0;
    com[num].RX_BYTES_IN_FIFO = 0;
    com[num].dready = 1;
    com[num].LSR |= UART_LSR_DR;
    ser_clear_queue(num);
    /* XXXXXXXXX - is the above 3 lines correct? */
  }
  if (fifo & UART_FCR_CLEAR_XMIT) {
    com[num].TX_FIFO_START = 0;
    com[num].TX_FIFO_END = 0;
    com[num].LSR |= UART_LSR_TEMT | UART_LSR_THRE;
  }
}

inline int
get_lsr(int num)
{
  int val = com[num].LSR;

  /* Clear the Reciever Line Status Interrupt flag: RLSI not yet implemented
  if ((com[num].IIR & UART_IIR_ID) == UART_IIR_RLSI)
    com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
  */

  com[num].LSR &= ~UART_LSR_ERR;
  return (val);
}

/* this function handles when the user writes to the FifoControl
** Register.
*/
inline void
put_fcr(int num, int val)
{
  s_printf("COM%d: FCR = 0x%x\n",num,val);

  val &= 0xcf;			/* bits 4,5 are reserved. */

  /* Bits 1-6 are only programmed when bit 0 is set.
  ** i.e. when fifo is enabled.
  */
  if (val & UART_FCR_ENABLE_FIFO) {
    /* fifos are reset when we change from 16450 to 16550 mode.*/
    if (!(com[num].FCReg & UART_FCR_ENABLE_FIFO))
      uart_clear_fifo(num, UART_FCR_CLEAR_CMD);

    /* various flags to indicate that fifos are enabled */
    com[num].FCReg |= UART_FCR_ENABLE_FIFO;
    com[num].IIR |= UART_IIR_FIFO;

    /* commands to reset any of the two fifos.
    ** these bits are cleared by hardware when the fifos are cleared,
    ** i.e. immediately. :-) , and thus not saved.
    */
    if (val & UART_FCR_CLEAR_CMD)
      uart_clear_fifo(num, val & UART_FCR_CLEAR_CMD);

    /* dont need to emulate any RXRDY,TXRDY pins */
    if (val & UART_FCR_DMA_SELECT) {
      /* nothing */
      s_printf("COM%d: Attempt to change RXRDY & TXRDY pin modes",num);
    }

    /* set trigger values for receive fifo in uart register */
    if (val & UART_FCR_TRIGGER_4)
      com[num].FCReg |= UART_FCR_TRIGGER_4;

    if (val & UART_FCR_TRIGGER_8)
      com[num].FCReg |= UART_FCR_TRIGGER_8;

    /* assign special variable for easy access to trigger values */
    switch (val & UART_FCR_TRIGGER_14) {
    case UART_FCR_TRIGGER_1:   com[num].RX_FIFO_TRIGGER_VAL = 1;
    case UART_FCR_TRIGGER_4:   com[num].RX_FIFO_TRIGGER_VAL = 4;
    case UART_FCR_TRIGGER_8:   com[num].RX_FIFO_TRIGGER_VAL = 8;
    case UART_FCR_TRIGGER_14:  com[num].RX_FIFO_TRIGGER_VAL = 14;
    }

  }
  else {
    /* if uart was set in 16550 mode, this will reset it back to
    ** 16450 mode.  If it already was in 16450 mode, it doesnt matter.
    */
    com[num].FCReg &= (~UART_FCR_ENABLE_FIFO);
    com[num].IIR &= (~UART_IIR_FIFO);
    uart_clear_fifo(num, UART_FCR_CLEAR_CMD);
  }
}

inline void
put_lcr(int num, int val)
{
#if 0  /* This is directly related to the below "#if" */
  int changed = com[num].LCR ^ val;
#endif

  com[num].LCR = val;
  if (val & UART_LCR_DLAB)
    com[num].DLAB = 1;
  else
    com[num].DLAB = 0;

  s_printf("COM%d: ser_termios in put_lcr\n",num);
  ser_termios(num);

#if 0
   /* Mark Rejhon's comment: What a silly kludge. Breaks some other programs.
   **
   ** to make modems happy with unfriendly programs.
   ** i.e. make sure serial line is reset.
   ** Do this stuff whenever DLAB goes low. i.e. speed is specified.
   */
  if ((com[num].modem) && (changed & UART_LCR_DLAB) && (!(val & UART_LCR_DLAB))) {
    char dummy[20];
    ser_close(num);
    ser_open(num);
    write(com[num].fd, "ATZ\r\n", 5);
    usleep(2000000);
    read(com[num].fd, dummy, 20);
    write(com[num].fd, "ATZ\r\n", 5);
    usleep(2000000);
    read(com[num].fd, dummy, 20);
  }
#endif
}

void
put_mcr(int num, int val)
{
  int changed = com[num].MCR ^ val;
  int control = 0;
  com[num].MCR = val & 0x1F;

#if 0  /* This is already done at the end of put_mcr */
  if (changed & UART_MCR_DTR) && (com[num].modem)) {  
    int data = TIOCM_DTR;  
    if (val & UART_MCR_DTR) {
      ioctl(com[num].fd, TIOCMBIS, &data);		/* Raise DTR of port */
    } else {
      ioctl(com[num].fd, TIOCMBIC, &data);		/* Drop DTR of port */
      /* usleep(100000);  XXXX Is this really necessary???? */
    }
  }
#endif

  /* stuff stolen from selector code */
  if ((changed & UART_MCR_DTR) && (val & UART_MCR_DTR) && (com[num].mouse)) {
    struct termios mouse;
    static const unsigned short cflag[] =
    {
      (CS7 | CREAD | CLOCAL | HUPCL),		/* MicroSoft */
      (CS8 | CSTOPB | CREAD | CLOCAL | HUPCL),	/* MouseSystems 3 */
      (CS8 | CSTOPB | CREAD | CLOCAL | HUPCL),	/* MouseSystems 5 */
      (CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL),	/* MMSeries */
      (CS8 | CSTOPB | CREAD | CLOCAL | HUPCL)	/* Logitech */
    };
    int data = TIOCM_DTR;

    /* pull down DTR line on serial port, to make sure DTR rises during
       initialization of mouse */
    ioctl(com[num].fd, TIOCMBIC, &data);

    tcgetattr(com[num].fd, &mouse);
    mouse.c_cflag = cflag[com[num].mtype] | B1200;
    mouse.c_iflag = IGNBRK | IGNPAR;
    mouse.c_oflag = 0;
    mouse.c_lflag = 0;
    mouse.c_line = 0;
    mouse.c_cc[VTIME] = 0;
    mouse.c_cc[VMIN] = 1;
    tcsetattr(com[num].fd, TCSANOW, &mouse);
    write(com[num].fd, "*n", 2);
    usleep(100000);
    tcsetattr(com[num].fd, TCSANOW, &mouse);
    tcgetattr(com[num].fd, &com[num].newsettings);
  }

  /* Set the RTS and DTR bits of the serial line */
  if (changed & (UART_MCR_RTS | UART_MCR_DTR)) {
    control = ((com[num].MCR & UART_MCR_RTS) ? TIOCM_RTS : 0)
            | ((com[num].MCR & UART_MCR_DTR) ? TIOCM_DTR : 0);
    ioctl(com[num].fd, TIOCMSET, &control);
    if (changed & UART_MCR_DTR) {
      s_printf("COM%d: DTR change to %d\n",num,(val & UART_MCR_DTR));
    }
  }
  s_printf("COM%d: MCR = 0x%x\n",num,val);
}

int
do_serial_in(int num, int port)
{
  int val;

  switch (port - com[num].base_port) {
  case UART_RX:		/* case UART_DLL: */
    if (com[num].DLAB)
      val = com[num].dll;
    else
      val = get_rx(num);
    break;

  case UART_IER:	/* case UART_DLM: */
    if (com[num].DLAB)
      val = com[num].dlm;
    else
      val = com[num].IER;
    break;

  case UART_IIR:
    val = com[num].IIR;
    if ((com[num].IIR & UART_IIR_ID) == UART_IIR_THRI)
      com[num].IIR = (com[num].IIR & UART_IIR_FIFO) | UART_IIR_NO_INT;
    break;

  case UART_LCR:
    val = com[num].LCR;
    break;

  case UART_MCR:
    val = com[num].MCR;
    break;

  case UART_LSR:
    val = get_lsr(num);
    break;

  case UART_MSR:
    val = get_msr(num);
    break;

  case UART_SCR:   	/* scratch: in/out */
    val = com[num].SCR;
    break;

  default:
    val = 0;
    break;
  }
  return val;
}

int
do_serial_out(int num, int port, int val)
{
  switch (port - com[num].base_port) {

  case UART_TX:		/*  case UART_DLL: */
    if (com[num].DLAB)
      com[num].dll = val;
    else
      put_tx(num, val);
    break;

  case UART_IER:	/* case UART_DLM: */
    if (com[num].DLAB) {
      com[num].dlm = val;
      /* ser_termios(num);  Unnecessary. Done when DLAB goes off again */
    }
    else {
      com[num].IER = val;
      s_printf("COM%d: IER = 0x%02x\n", num, val);
    }
    break;

  case UART_FCR:	/* case UART_IIR: */
    put_fcr(num, val);
    break;

  case UART_LCR:
    put_lcr(num, val);
    break;

  case UART_MCR:
    put_mcr(num, val);
    break;

  case UART_LSR:	/* read-only port */
    break;

  case UART_MSR:	/* read-only port */
    break;

  case UART_SCR:	/* scratch: in/out */
    com[num].SCR = val;
    break;
  }

  return 0;
}

#if 1
/* New int14 thanks to Mark Rejhon <mdrejhon@undergrad.math.uwaterloo.ca> */
void 
int14(void)
{
  int num = LO(dx);

  switch (HI(ax)) {
  case 0:			/* init serial port (raise DTR?) */
    if (num >= config.num_ser) return;
    s_printf("INT14, 0x0: Init port %d, ", num);

    /* The following sets character size, parity, and stopbits at once */
    com[num].LCR = (com[num].LCR & ~UART_LCR_PARA) | (LO(ax) & UART_LCR_PARA);
    s_printf("al = 0x%x, ",LO(ax));

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
    s_printf("\n");
    s_printf("COM%d: ser_termios in INT14 init\n",num);
    ser_termios(num);
    HI(ax) = get_lsr(num);
    LO(ax) = get_msr(num);
    s_printf("INT14, 0x0: Returning with AL=0x%x AH=0x%x\n", LO(ax), HI(ax));
    break;
  case 1:			/* write char (fix bit 7?) */
    if (num >= config.num_ser) return;
    /* s_printf("INT14: 0x1: Write char on %d\n",num); */
    do_serial_out(num, com[num].base_port, LO(ax));
    HI(ax) = get_lsr(num);
    LO(ax) = get_msr(num); 
    break;
  case 2:			/* read char ... */
    if (num >= config.num_ser) return;
    s_printf("INT14: 0x2: Read char on %d from %x\n",num,com[num].base_port);
    if (com[num].LSR | UART_LSR_DR) {
      LO(ax) = do_serial_in(num, com[num].base_port);
      HI(ax) = get_lsr(num) & ~0x80;
    }
    else
      HI(ax) = get_lsr(num) | 0x80;
    break;
  case 3:			/* port status ... */
    if (num >= config.num_ser) return;
    /* s_printf("INT14: 0x3: Port Status on %d\n",num); */
    HI(ax) = get_lsr(num);
    LO(ax) = get_msr(num);
    break;
  case 4:			/* extended initialize. Not supported. */
    s_printf("INT14, 0x4: Unsupported Extended serial initialize on %d\n",num);
    return;
  default:			/* This runs if nothing handles the func? */
    error("INT14, 0x%x: Unsupported interrupt on %d\n",HI(ax),num);	
    /* show_regs(); */
    /* fatalerr = 5; */
    return;
  }
}
#else /* The following is the old crappy int14 interface. */
void
int14(void)
{
  int num;

  switch (HI(ax)) {
  case 0:			/* init */
    LWORD(eax) = 0;
    num = LWORD(edx);
    s_printf("init serial %d\n", num);
    break;
  case 1:			/* write char */
    put_tx(LO(dx), LO(ax));
    HI(ax) = 96;
    break;
  case 2:			/* read char */
    HI(ax) = 96;
    if (com[LO(dx)].dready) {
      HI(ax) |= 1;
      HI(ax) &= ~128;
    }
    else {
      HI(ax) |= 128;
      HI(ax) &= ~1;
    }
    LO(ax) = get_rx(LO(dx));    /* This should compensate for FIFO mode!! */

    break;
  case 3:			/* port status */

    if (LO(dx) > 3)
      LO(dx) = 3;
    HI(ax) = 0x60;
    LO(ax) = 0xb0;		/* CD, DSR, CTS */
    if (ser_avail(LO(dx))) {
      HI(ax) |= 1;		/* receiver data ready */
    }
    break;
  case 4:			/* extended initialize */
    s_printf("extended serial initialize\n");
    return;
  default:
    error("ERROR: serial error (unknown interrupt)\n");
    show_regs();
    /* fatalerr = 5; */
    return;
  }
}
#endif

inline void
do_ser(int num)
{
  com[num].dready = 1;
  if (com[num].IER & UART_IER_RDI) {
    com[num].in_rxinterrupt = 1;
    /* must dequeue RX/RX-FIFO in beg_rxint() */
    queue_hard_int(com[num].interrupt, beg_rxint, end_rxint);
  }
  else {
    com[num].RX = ser_dequeue(num);
  }
}

void
serial_run(void)
{
  int i;

  check_serial_ready();
  for (i = 0; i < config.num_ser; i++) {
    /* (!com[i].in_msinterrupt) */
    if ((!com[i].in_rxinterrupt) && ser_avail(i)) 
      do_ser(i);
    if ((com[i].IER & UART_IER_MSI) && !com[i].in_msinterrupt) 
      serial_int_msr(i);
  }
  return;
}
