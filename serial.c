/* serial.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * Basic 16550 UART emulation.
 *
 * $Date: 1994/03/04 15:23:54 $
 * $Source: /home/src/dosemu0.50/RCS/serial.c,v $
 * $Revision: 1.16 $
 * $State: Exp $
 *
 * $Log: serial.c,v $
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
#include "dosipc.h"
#include "serial.h"

extern int_count[8];

extern struct CPU cpu;
extern config_t config;
extern pid_t parent_pid, ser_pid;

serial_t com[MAX_SER];

#define QUEUE  (param->ser[num].queue)
#define QSTART (param->ser[num].start)
#define QEND   (param->ser[num].end)

int
ser_open(int num)
{
  QSTART = 0;
  QEND = 0;
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

  QSTART = 0;
  QEND = 0;
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

inline int
ser_avail(int num)
{
  return (QSTART != QEND);
}

void
dump_ser_queue(int num)
{
  int x;
  int pos = QSTART;

  while (pos != QEND) {
    pos = (pos + 1) & (SER_QUEUE_LEN - 1);
  }
}

inline int
ser_enqueue(int num, int byte)
{
  QUEUE[QEND] = byte;
  QEND = (QEND + 1) & (SER_QUEUE_LEN - 1);

  /* this line is optional, and may not work. it's here to make
   * sure the queue will eat itself instead of seeming full
   * when empty
   */
  if (QEND == QSTART)
    QSTART = (QSTART + 1) & (SER_QUEUE_LEN - 1);
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
  QSTART = (QSTART + 1) & (SER_QUEUE_LEN - 1);

  return byte;
}

int
ser_termios(int num)
{
  speed_t baud;

#define DIVISOR ((com[num].dlm<<8)|com[num].dll)

  /* return if not a tty */
  if (tcgetattr(com[num].fd, &com[num].newsettings) == -1)
    return;

  /* set word size */
  com[num].newsettings.c_cflag &= ~CSIZE;
  switch (com[num].LCR & 3) {
  case UART_LCR_WLEN5:
    com[num].newsettings.c_cflag |= CS5;
    s_printf("COM: 5 bits\n");
    break;
  case UART_LCR_WLEN6:
    com[num].newsettings.c_cflag |= CS6;
    s_printf("COM: 6 bits\n");
    break;
  case UART_LCR_WLEN7:
    com[num].newsettings.c_cflag |= CS7;
    s_printf("COM: 7 bits\n");
    break;
  case UART_LCR_WLEN8:
    com[num].newsettings.c_cflag |= CS8;
    s_printf("COM: 8 bits\n");
    break;
  }

  /* set parity */
  if (com[num].LCR & UART_LCR_PARITY) {
    s_printf("COM: parity ");
    com[num].newsettings.c_cflag |= PARENB;
    if (com[num].LCR & UART_LCR_EPAR) {
      com[num].newsettings.c_cflag &= ~PARODD;
      s_printf("even\n");
    }
    else {
      com[num].newsettings.c_cflag |= PARODD;
      s_printf("odd\n");
    }
  }
  else {
    com[num].newsettings.c_cflag &= ~PARENB;
    s_printf("COM: no parity\n");
  }

  /* stop bits: UART_LCR_STOP set means 2 stop bits, 1 otherwise */
  if (com[num].LCR & UART_LCR_STOP)
    com[num].newsettings.c_cflag |= CSTOPB;
  else
    com[num].newsettings.c_cflag &= ~CSTOPB;

  /* XXX - baud rate. ought to calculate here...
   *  divisor = 1843200 / (BaudRate * 16)
   */
  switch (DIVISOR) {
  case 0x900:
    baud = B50;
    s_printf("COM: 50 baud!\n");
    break;
  case 0x417:
    baud = B110;
    s_printf("COM: 110 baud!\n");
    break;
  case 0x300:
    baud = B150;
    s_printf("COM: 150 baud!\n");
    break;
  case 0x180:
    baud = B300;
    s_printf("COM: 300 baud!\n");
    break;
  case 0xc0:
    baud = B600;
    s_printf("COM: 600 baud!\n");
    break;
  case 0x60:
    baud = B1200;
    s_printf("COM: 1200 baud!\n");
    break;
  case 0x40:
    baud = B1800;
    s_printf("COM: 1800 baud!\n");
    break;
  case 0x30:
    baud = B2400;
    s_printf("COM: 2400 baud!\n");
    break;
  case 0x18:
    baud = B4800;
    s_printf("COM: 4800 baud!\n");
    break;
  case 0xc:
    baud = B9600;
    s_printf("COM: 9600 baud!\n");
    break;
  case 0x6:
    baud = B19200;
    s_printf("COM: 19200 baud!\n");
    break;
  case 0x3:
    baud = B38400;
    s_printf("COM: 38400 baud!\n");
    break;
  default:
    {
      int tmpbaud = 1843200 / (DIVISOR * 16);

      error("ERROR: COM: BAUD divisor not known, 0x%04x, = baud %d\n",
	    DIVISOR, tmpbaud);
      return;
    }
  }

  s_printf("COM%d: BAUD divisor = 0x%x\n", num, DIVISOR);
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

  /* set both up for 1200 baud.  no workee yet */
  com[num].dll = 0x60;
  com[num].dlm = 0;

  DOS_SYSCALL(tcgetattr(com[num].fd, &com[num].oldsettings));
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
	   com[num].base_port, ((u_short *) (0x400) + num), *((u_short *) (0x400) + num));

  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  com[num].IIR = UART_IIR_NO_INT;
  com[num].LCR = UART_LCR_WLEN8;
  com[num].IER = 0;
  com[num].MSR = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS;
  com[num].FCtrlR = 0;
  com[num].MCR = 0;
  com[num].DLAB = 0;
}

int
serial_init(void)
{
  int i;

  *(u_short *) 0x400 = 0;
  *(u_short *) 0x402 = 0;
  *(u_short *) 0x404 = 0;
  *(u_short *) 0x406 = 0;

  param->ser_locked = mutex_init();

  /* do UART init here - need to setup registers*/
  for (i = 0; i < config.num_ser; i++) {
    com[i].fd = -1;
    do_ser_init(i);
  }

  warn("COM: serial init, %d serial ports\n", config.num_ser);
}

/* these won't work unless I pass the fd to the child
 * process, too.  grumble.
 */

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
	s_printf("SERIAL: ERROR - %s\n", strerror(errno));
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
      {
	struct termios mouse;
	static const unsigned short cflag[] =
	{
	  (CS7 | CREAD | CLOCAL | HUPCL),	/* MicroSoft */
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
    }
  }
}

int
serial_close(void)
{
  int i;

  s_printf("COM: serial_close\n");
  for (i = 0; i < config.num_ser; i++) {
    DOS_SYSCALL(tcsetattr(com[i].fd, TCSANOW, &com[i].oldsettings));
    ser_close(i);
  }
}

int
check_serial_ready()
{
#define BYTES_SIZE 50
  int x, size;
  u_char bytes[BYTES_SIZE];

  for (x = 0; x < config.num_ser; x++) {
    if (com[x].fd != -1) {
      if ((size = read(com[x].fd, bytes, BYTES_SIZE)) > 0) {
	int i;

	for (i = 0; i < size; i++) {
	  ser_enqueue(x, bytes[i]);
	}
      }
    }
  }

}

inline int
get_rx(int num)
{
  com[num].dready = 0;
  com[num].LSR &= ~UART_LSR_DR;
  com[num].IIR = UART_IIR_NO_INT;
  return com[num].RX;
}

/* this function will take care of when the user program reads from
** the fifo data register.
*/
inline int
get_fiforx(int num)
{
  int val;

  /* if there are data in fifo then read from from the fifo.
  ** if the fifo is empty, read 0. (is this correct ???)
  */
  if (com[num].RX_BYTES_IN_FIFO) {
    val = com[num].RX_FIFO[com[num].RX_FIFO_START];
    com[num].RX_FIFO_START = (com[num].RX_FIFO_START + 1) & 15;
    com[num].RX_BYTES_IN_FIFO--;
  }
  else {
    val = 0;
  }

  /* if fifo is empty, we will no longer flag data-ready. */
  if (!com[num].RX_BYTES_IN_FIFO) {
    com[num].dready = 0;
    com[num].LSR &= ~UART_LSR_DR;
  }

  /* if less data in fifo than trigger, dont indicate interrupt anymore*/
  if (com[num].RX_BYTES_IN_FIFO < com[num].RX_FIFO_TRIGGER_VAL) {
    com[num].IIR = UART_IIR_NO_INT | UART_IIR_FIFO;
  }

  return (val);
}

inline int
get_lsr(int num)
{
  return com[num].LSR;
}

inline int
get_msr(int num)
{
  int control;
  int oldmsr = com[num].MSR;

#define MSR_BIT(c,s,d) ((c & s) ? d : 0)
#define DELTA_BIT(o,n,b) ((o ^ n) & b)

  ioctl(com[num].fd, TIOCMGET, &control);

  com[num].MSR = 0;

  com[num].MSR |= MSR_BIT(control, TIOCM_CTS, UART_MSR_CTS);
  com[num].MSR |= MSR_BIT(control, TIOCM_DSR, UART_MSR_DSR);
  com[num].MSR |= MSR_BIT(control, TIOCM_CAR, UART_MSR_DCD);
  com[num].MSR |= MSR_BIT(control, TIOCM_RNG, UART_MSR_RI);

  if (DELTA_BIT(oldmsr, com[num].MSR, UART_MSR_DCD))
    com[num].MSR |= UART_MSR_DDCD;

  if (DELTA_BIT(oldmsr, com[num].MSR, UART_MSR_DSR))
    com[num].MSR |= UART_MSR_DDSR;

  if (DELTA_BIT(oldmsr, com[num].MSR, UART_MSR_CTS))
    com[num].MSR |= UART_MSR_DCTS;

  return com[num].MSR;
}

int
beg_txint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;

  if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
    com[num].IIR = UART_IIR_THRI | UART_IIR_FIFO;
  }
  else {
    com[num].IIR = UART_IIR_THRI;
  }
  return 0;
}

int
end_txint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;

  if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
    com[num].IIR = UART_IIR_NO_INT | UART_IIR_FIFO;
  }
  else {
    com[num].IIR = UART_IIR_NO_INT;
  }

  /* no longer pending interrupt. */
  com[num].in_txinterrupt = 0;
  /* check if new data has arrived. */
  serial_run();

  return 0;
}

inline void
put_tx(int num, int val)
{
  com[num].TX = val;

  /* loopback */
  if (com[num].MCR & UART_MCR_LOOP) {
    s_printf("COM: loopback write!\n");
    /* XXXXXXXXXXXXXXX should really be changed */
    ser_enqueue(num, val);
    return;
  }
  else
    RPT_SYSCALL(write(com[num].fd, &val, 1));

  if ((com[num].IER & UART_IER_THRI) && (com[num].MCR & UART_MCR_OUT2)) {
    if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
      com[num].IIR = UART_IIR_THRI | UART_IIR_FIFO;
    }
    else {
      com[num].IIR = UART_IIR_THRI;
    }
    /* queue tx interrupt, if non pending. */
    if (!com[num].in_txinterrupt) {
      com[num].in_txinterrupt = 1;
      queue_hard_int(com[num].interrupt, beg_txint, end_txint);
    }
  }
}

inline void
put_fifotx(int num, int val)
{
  com[num].TX = val;

  /* loopback */
  if (com[num].MCR & UART_MCR_LOOP) {
    s_printf("COM: loopback write!\n");
    /* should really be changed */
    ser_enqueue(num, val);
    return;
  }
  else
    RPT_SYSCALL(write(com[num].fd, &val, 1));

  if ((com[num].IER & UART_IER_THRI) && (com[num].MCR & UART_MCR_OUT2)) {
    if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
      com[num].IIR = UART_IIR_THRI | UART_IIR_FIFO;
    }
    else {
      com[num].IIR = UART_IIR_THRI;
    }
    /* queue tx interrupt, if non pending. */
    if (!com[num].in_txinterrupt) {
      com[num].in_txinterrupt = 1;
      queue_hard_int(com[num].interrupt, beg_txint, end_txint);
    }
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
  }
  if (fifo & UART_FCR_CLEAR_XMIT) {
    com[num].TX_FIFO_START = 0;
    com[num].TX_FIFO_END = 0;
  }
}

/* this function handles when the user writes to the FifoControl
** Register.
*/
inline void
put_fcr(int num, int val)
{
  val &= 0xcf;			/* bits 4,5 are reserved. */
  /* Bits 1-6 are only programmed when bit 0 is set
  ** i.e. when fifo is enabled.
  */
  if (val & UART_FCR_ENABLE_FIFO) {
    /* fifos are reset when we change from 16450 to 16550 mode.*/
    if (!(com[num].FCtrlR & UART_FCR_ENABLE_FIFO))
      uart_clear_fifo(num, UART_FCR_CLEAR_CMD);

    /* various flags to indicate that fifos are enabled */
    com[num].FCtrlR |= UART_FCR_ENABLE_FIFO;
    com[num].IIR |= UART_IIR_FIFO;

    /* commands to reset any of the two fifos.
    ** these bits are cleared by hardware when the fifos are cleared,
    ** i.e. immediately. :-) , and thus not saved.
    */
    if (val & UART_FCR_CLEAR_RCVR) {
      uart_clear_fifo(num, UART_FCR_CLEAR_RCVR);
    }
    if (val & UART_FCR_CLEAR_XMIT) {
      uart_clear_fifo(num, UART_FCR_CLEAR_XMIT);
    }

    /* dont need to emulate any RXRDY,TXRDY pins */
    if (val & UART_FCR_DMA_SELECT) {
      /* nothing */
    }

    /* set trigger values for receive fifo in uart register */
    if (val & UART_FCR_TRIGGER_4) {
      com[num].FCtrlR |= UART_FCR_TRIGGER_4;
    }
    if (val & UART_FCR_TRIGGER_8) {
      com[num].FCtrlR |= UART_FCR_TRIGGER_8;
    }
    /* assign special variable for easy access to trigger values */
    switch (val & (UART_FCR_TRIGGER_8 | UART_FCR_TRIGGER_4)) {
    case UART_FCR_TRIGGER_1:
      com[num].RX_FIFO_TRIGGER_VAL = 1;
    case UART_FCR_TRIGGER_4:
      com[num].RX_FIFO_TRIGGER_VAL = 4;
    case UART_FCR_TRIGGER_8:
      com[num].RX_FIFO_TRIGGER_VAL = 8;
    case UART_FCR_TRIGGER_14:
      com[num].RX_FIFO_TRIGGER_VAL = 14;
    }

  }
  else {
    /* if uart was set in 16550 mode, this will reset it back to
    ** 16450 mode.
    ** if it already was in 16450 mode, it doesnt matter.
    */
    com[num].FCtrlR &= (~UART_FCR_ENABLE_FIFO);
    com[num].IIR &= (~(UART_IIR_FIFO));
    uart_clear_fifo(num, UART_FCR_CLEAR_CMD);
  }
}

inline void
put_lcr(int num, int val)
{
  int changed = com[num].LCR ^ val;

  com[num].LCR = val;
  if (val & UART_LCR_DLAB) {
    com[num].DLAB = 1;
  }
  else {
    com[num].DLAB = 0;
  }
  ser_termios(num);

  /* to make modems happy with unfriendly programs.
   ** i.e. make sure serial line is reset.
   ** Do this suff whenever DLAB goes low. i.e. speed is specified.
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
}

int
beg_msi(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
    com[num].IIR = UART_IIR_MSI | UART_IIR_FIFO;
  }
  else {
    com[num].IIR = UART_IIR_MSI;
  }
  return 0;
}

int
end_msi(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
    com[num].IIR = UART_IIR_NO_INT | UART_IIR_FIFO;
  }
  else {
    com[num].IIR = UART_IIR_NO_INT;
  }
  return 0;
}

void
put_mcr(int num, int val)
{
  int changed = com[num].MCR ^ (u_char) val;
  int intr = 0;
  int control = 0;

  com[num].MCR = val;

  /* take care of when DTR goes low and modem is configured */
  if ((changed & 0x01) && (!(val & 0x01)) && (com[num].modem)) {
    int data = TIOCM_DTR;

    /* pull down DTR line on serial port*/
    ioctl(com[num].fd, TIOCMBIC, &data);
    usleep(100000);
  }

  /* take care of when DTR goes high and modem is configured */
  if ((changed & 0x01) && (val & 0x01) && (com[num].modem)) {
    int data = TIOCM_DTR;

    /* DTR line on serial port should also go high*/
    ioctl(com[num].fd, TIOCMBIS, &data);
  }

  /* stuff stolen from selector code */
  if ((changed & 0x01) && (val & 0x01) && (com[num].mouse)) {
    struct termios mouse;
    static const unsigned short cflag[] =
    {
      (CS7 | CREAD | CLOCAL | HUPCL),	/* MicroSoft */
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

  if (intr && (com[num].IER & UART_IER_MSI))
    queue_hard_int(com[num].interrupt, beg_msi, end_msi);

  if (!(com[num].MCR & UART_MCR_DTR)) {
    s_printf("COM: dropped DTR!\n");
  }
  control = ((com[num].MCR & UART_MCR_RTS) ? TIOCM_RTS : 0)
    | ((com[num].MCR & UART_MCR_DTR) ? TIOCM_DTR : 0);
  ioctl(com[num].fd, TIOCMSET, &control);
}

int
do_serial_in(int num, int port)
{
  int val;

  switch (port - com[num].base_port) {
  case UART_RX:
    /*  case UART_DLL: */
    if (com[num].DLAB) {
      val = com[num].dll;
    }
    else {
      if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
	val = get_fiforx(num);
      }
      else {
	val = get_rx(num);
      }
    }
    break;

  case UART_IER:
    /*  case UART_DLM: */
    if (com[num].DLAB) {
      val = com[num].dlm;
    }
    else {
      val = com[num].IER;
    }
    break;

  case UART_IIR:
    val = com[num].IIR;
    if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
      com[num].IIR = UART_IIR_NO_INT | UART_IIR_FIFO;
    }
    else {
      com[num].IIR = UART_IIR_NO_INT;
    }
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

    /* scratch: in/out */
  case UART_SCR:
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
  case UART_RX:
    /*  case UART_DLL: */
    if (com[num].DLAB) {
      com[num].dll = val;
      ser_termios(num);
    }
    else {
      if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
	put_fifotx(num, val);
      }
      else {
	put_tx(num, val);
      }
    }
    break;

  case UART_IER:
    /*  case UART_DLM: */
    if (com[num].DLAB) {
      com[num].dlm = val;
      ser_termios(num);
    }
    else {
      com[num].IER = val;
      s_printf("IER%d = 0x%02x\n", num, val);
    }
    break;

  case UART_FCR:
    /*  case UART_IIR: */
    put_fcr(num, val);
    break;
  case UART_LCR:
    put_lcr(num, val);
    break;

  case UART_MCR:
    put_mcr(num, val);
    break;

  case UART_LSR:
    /* read-only port */
    break;

  case UART_MSR:
    /* read-only port */
    break;

    /* scratch: in/out */
  case UART_SCR:
    com[num].SCR = val;
    break;
  }

  return 0;
}

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
    LO(ax) = get_rx(LO(dx));
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

int
begin_serint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
    /* fifos are enabled, fill as much as possible before interrupt
    ** is made.
    */
    while ((com[num].RX_BYTES_IN_FIFO < 16)
	   && (ser_avail(num))) {
      com[num].RX_BYTES_IN_FIFO++;
      com[num].RX_FIFO[com[num].RX_FIFO_END] = ser_dequeue(num);
      com[num].RX_FIFO_END = (com[num].RX_FIFO_END + 1) & 15;
    }
    /* if trigger is exceeded we issue a ReceivedDataAvailable intr.
    ** othervise we do a CharacterTimeoutIndication.
    */
    if (com[num].RX_BYTES_IN_FIFO < com[num].RX_FIFO_TRIGGER_VAL) {
      com[num].IIR = UART_IIR_RDI | UART_IIR_FIFO;
    }
    else {
      com[num].IIR = UART_IIR_CTI | UART_IIR_FIFO;
    }

    com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE | UART_LSR_DR;
    com[num].in_rxinterrupt = 1;
  }
  else {
    /* normal mode, copy one byte to RX register before issuing interrupt.
    */
    com[num].RX = ser_dequeue(num);
    com[num].IIR = UART_IIR_RDI;
    com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE | UART_LSR_DR;
    com[num].in_rxinterrupt = 1;
  }

  return 0;
}

int
end_serint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;

  com[num].in_rxinterrupt = 0;
  if (com[num].FCtrlR & UART_FCR_ENABLE_FIFO) {
    com[num].IIR = UART_IIR_NO_INT | UART_IIR_FIFO;
  }
  else {
    com[num].IIR = UART_IIR_NO_INT;
  }
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;

  /* check if new data has arrived. */
  serial_run();

  return 0;
}

inline void
do_ser(int num)
{
  com[num].dready = 1;
  if (com[num].IER & UART_IER_RDI) {
    com[num].in_rxinterrupt = 1;
    /* must dequeue RX/RX-FIFO in begin_serint() */
    queue_hard_int(com[num].interrupt, begin_serint, end_serint);
  }
  else {
    com[num].RX = ser_dequeue(num);
    com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE | UART_LSR_DR;
  }
}

int
serial_run(void)
{
  int rtn, i;

  check_serial_ready();

  rtn = 0;
  for (i = 0; i < config.num_ser; i++) {
    if ((!com[i].in_rxinterrupt) && ser_avail(i)) {
      do_ser(i);
      rtn = 1;
    }
  }

  return rtn;
}
