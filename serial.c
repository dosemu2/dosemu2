/* serial.c for the DOS emulator 
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * Basic 16450 UART emulation.  16550 FIFOs to come later?
 *
 * $Date: 1993/05/04 05:29:22 $
 * $Source: /usr/src/dos/RCS/serial.c,v $
 * $Revision: 1.1 $
 * $State: Exp $
 *
 * $Log: serial.c,v $
 * Revision 1.1  1993/05/04  05:29:22  root
 * Initial revision
 *
 *
 */

#define NO_PAUSE 1

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>

#include "config.h"
#include "cpu.h"
#include "emu.h"
#include "dosipc.h"
#include "mutex.h"
#include "serial.h"


extern struct CPU cpu;
extern config_t config;
extern pid_t parent_pid, ser_pid;

/* dosemu only supports 2 serial ports */
serial_t com[2];

int
ser_open(char *name)
{
  return ( DOS_SYSCALL( open(name, O_RDWR | O_NONBLOCK) ) );
}


#define QUEUE param->ser[num].queue
#define QSTART param->ser[num].start
#define QEND param->ser[num].end


inline int
ser_avail(int num)
{
  return (QSTART != QEND);
}


void
dump_ser_queue(int num)
{
  int x;
  int pos=QSTART;

  while (pos != QEND)
    {
      pos = (pos + 1) % SER_QUEUE_LEN;
    }
}


inline int
ser_enqueue(int num, int byte)
{
  mutex_get(com[num].sem);

  QUEUE[QEND]= byte;
  QEND = (QEND+1) % SER_QUEUE_LEN;

  /* this line is optional, and may not work. it's here to make
   * sure the queue will eat itself instead of seeming full
   * when empty
   */
  if (QEND == QSTART) QSTART = (QSTART+1) % SER_QUEUE_LEN; 

/*  dump_ser_queue(num); */

  mutex_release(com[num].sem);
}


inline int
ser_dequeue(int num)
{
  int byte;

  if (QEND == QSTART) {
    error("ERROR: COM: in ser_dequeue(), QEND == QSTART!\n");
    return -1;
  }

  mutex_get(com[num].sem);

  byte = QUEUE[QSTART];
  QSTART = (QSTART+1) % SER_QUEUE_LEN;

/*  dump_ser_queue(num); */

  mutex_release(com[num].sem);

  return byte;
}


int
ser_termios(int num)
{
  struct termios sertty;
  speed_t baud;

#define DIVISOR ((com[num].dlm<<8)|com[num].dll)

  /* return if not a tty */
  if ( tcgetattr(com[num].fd, &sertty) == -1)
    return;

  /* set word size */
  sertty.c_cflag &= ~CSIZE;
  switch(com[num].LCR & 3) 
    {
    case UART_LCR_WLEN5:
      sertty.c_cflag |= CS5;
      s_printf("COM: 5 bits\n");
      break;
    case UART_LCR_WLEN6:
      sertty.c_cflag |= CS6;
      s_printf("COM: 6 bits\n");
      break;
    case UART_LCR_WLEN7:
      sertty.c_cflag |= CS7;
      s_printf("COM: 7 bits\n");
      break;
    case UART_LCR_WLEN8:
      sertty.c_cflag |= CS8;
      s_printf("COM: 8 bits\n");
      break;
    }

  /* set parity */
  if (com[num].LCR & UART_LCR_PARITY) 
    {
      s_printf("COM: parity ");
      sertty.c_cflag |= PARENB;
      if (com[num].LCR & UART_LCR_EPAR) { sertty.c_cflag &= ~PARODD; s_printf("even\n"); }
      else { sertty.c_cflag |= PARODD; s_printf("odd\n"); }
    }
  else { sertty.c_cflag &= ~PARENB; s_printf("COM: no parity\n"); }


  /* stop bits: UART_LCR_STOP set means 2 stop bits, 1 otherwise */
  if (com[num].LCR & UART_LCR_STOP) sertty.c_cflag |= CSTOPB;
  else sertty.c_cflag &= ~CSTOPB;


  /* XXX - baud rate. ought to calculate here... 
   *  divisor = 1843200 / (BaudRate * 16)
   */
  switch (DIVISOR)
    {
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
    default: {
      int tmpbaud = 1843200 / (DIVISOR * 16);
      error("ERROR: COM: BAUD divisor not known, 0x%04x, = baud %d\n", 
	    DIVISOR, tmpbaud);
      return;
    }
    }

  s_printf("COM%d: BAUD divisor = 0x%x\n", num, DIVISOR);
  DOS_SYSCALL( cfsetispeed(&sertty, baud) );
  DOS_SYSCALL( cfsetospeed(&sertty, baud) );

  DOS_SYSCALL( tcsetattr(com[num].fd, TCSANOW, &sertty) );
}


void
do_ser_init(num)
{
  com[num].in_interrupt = 0;
  com[num].fd = ser_open(com[num].dev);

  /* initialize the semaphore */
  com[num].sem = mutex_init();

  param->ser[num].start=0;     
  param->ser[num].end=0;

  /* no interrupts so far */
  param->ser[num].num_ints = 0;

  /* set both up for 1200 baud.  no workee yet */
  com[num].dll=0x60;   
  com[num].dlm=0;
  ser_termios(num);

  *((u_short *)(0x400) + num) = com[num].base_port;
  error("ERROR: num %d, port 0x%x, address 0x%x, mem 0x%x\n", num,
	com[num].base_port, ((u_short *)(0x400) + num), *((u_short *)(0x400) + num));
  
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  com[num].IIR = UART_IIR_NO_INT;
  com[num].LCR = UART_LCR_WLEN8;
  com[num].IER = 0;
  com[num].MSR = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS ;
}


int 
serial_init(void)
{
  int i;

  *(u_short *)0x400 = 0;
  *(u_short *)0x402 = 0;
  *(u_short *)0x404 = 0;
  *(u_short *)0x406 = 0;

  param->ser_locked = mutex_init();

  /* do UART init here - need to setup registers*/
  for (i = 0; i < config.num_ser; i++) 
    {
      com[i].fd = -1;
      param->ser[i].num_ints = 0;
      do_ser_init(i);
    }

  warn("COM: serial init, %d serial ports\n", config.num_ser);
}


/* these won't work unless I pass the fd to the child
 * process, too.  grumble. 
 */
int
close_mouse()
{
  int i;
  return;
  for (i=0; i<config.num_ser; i++)
      if (com[i].mouse) { 
	close(com[i].fd); 
	com[i].fd = -1;
	warn("MOUSE: close %d\n", i); 
      }
}

int
open_mouse()
{
  int i;
  return;
  for (i=0; i<config.num_ser; i++)
      if (com[i].mouse) { 
	com[i].fd = ser_open(com[i].dev); 
	warn("MOUSE: open %d\n", i); 
      }
}


int
serial_close(void)
{
  int i;
  s_printf("COM: serial_close\n");
  for (i=0; i<config.num_ser; i++)
    mutex_deinit(com[i].sem);
  mutex_deinit(param->ser_locked);
}


void
serial_fdset(fd_set *set)
{
  int x;
  for (x=0; x<config.num_ser; x++)
    if (com[x].fd != -1) FD_SET(com[x].fd, set);
}



inline void
ser_interrupt(int num)
{
  mutex_get(param->ser_locked);
  param->ser[num].num_ints++;
  mutex_release(param->ser_locked);
  kill(parent_pid, SIG_SER); 
}
  

int
check_serial_ready(fd_set *set)
{
#define BYTES_SIZE 50
  int x, size;
  u_char bytes[BYTES_SIZE];

  for (x=0; x<config.num_ser; x++)
    if (com[x].fd != -1 && FD_ISSET(com[x].fd, set))
      {
	if ( (size=RPT_SYSCALL(read(com[x].fd, bytes, BYTES_SIZE))) > 0)
	  {
	    int i;

	    for (i=0; i<size; i++) {
	      ser_enqueue(x, bytes[i]);
	      ser_interrupt(x); 
	    }
	    
	  }
	else error("ERROR: select() said serial data%d, but none!\n", x);
      }
}


/* determine if I/O address is for serial port, return which port or 0 */
int
is_serial_io(int port)
{
  int x;

  for (x=0; x<config.num_ser; x++) {
    if ((port-com[x].base_port) >= 0 && (port-com[x].base_port) <= 7) {
      return x;
    }
  }

  return -1;
}


inline int
get_rx(int num)
{
  com[num].dready = 0;
  com[num].LSR &= ~UART_LSR_DR;
  return com[num].RX;
}


inline int
get_lsr(int num)
{
  return com[num].LSR;
}


inline int
get_msr(int num)
{
  return com[num].MSR;
}


int
beg_txint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  com[num].IIR = UART_IIR_THRI;
  return 0;
}

int
end_txint(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  com[num].IIR = UART_IIR_NO_INT;
  return 0;
}

inline void
put_tx(int num, int val)
{
  com[num].TX = val;

  /* loopback */
  if (com[num].MCR & UART_MCR_LOOP)
    {
      s_printf("COM: loopback write!\n");
      ser_enqueue(num, val);
      ser_interrupt(num);
      return;
    }
  else RPT_SYSCALL(write(com[num].fd, &val, 1));

  if ((com[num].IER & UART_IER_THRI) && (com[num].MCR & UART_MCR_OUT2)) {
    com[num].IIR = UART_IIR_THRI;
    queue_hard_int(com[num].interrupt, beg_txint, end_txint);
  }
}


inline void
put_lcr(int num, int val)
{
  com[num].LCR = val;
  ser_termios(num);
}


int
beg_msi(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  com[num].IIR = UART_IIR_MSI;
  return 0;
}

int
end_msi(int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  com[num].IIR = UART_IIR_NO_INT;
  return 0;
}

void
put_mcr(int num, int val)
{
  int changed = com[num].MCR ^ (u_char)val;
  int intr=0;

  com[num].MCR = val;

  if ((changed & UART_MCR_DTR) && (com[num].MCR & UART_MCR_DTR)) {
    com[num].MSR |= UART_MSR_DSR;
    com[num].MSR |= UART_MSR_DDSR;
    intr = 1;
  }
  if ((changed & UART_MCR_RTS) && (com[num].MCR & UART_MCR_RTS)) {
    com[num].MSR |= UART_MSR_CTS;
    com[num].MSR |= UART_MSR_DCTS;
    intr = 1;
  }

  if (intr && (com[num].IER & UART_IER_MSI)) 
    queue_hard_int(com[num].interrupt, beg_msi, end_msi);

  if (! (com[num].MCR & UART_MCR_DTR)) {
    s_printf("COM: dropped DTR!\n");
  }
}


int 
do_serial_in(int num, int port)
{
  int val;

  switch (port - com[num].base_port)
    {
    case UART_RX:
/*  case UART_DLL: */
      if (DLAB(num)) { 
	val = com[num].dll;
	s_printf("DLABL%d = 0x%x\n", num, val);
      }
      else {
	val = get_rx(num);
	com[num].IIR = UART_IIR_NO_INT;
      }
      break;

    case UART_IER:
/*  case UART_DLM: */
      if (DLAB(num)) {
	val = com[num].dlm;
	s_printf("DLABM%d = 0x%x\n", num, val);
      }
      else {
	val = com[num].IER;
      }
      break;

    case UART_IIR:
      val = com[num].IIR;
      com[num].IIR = UART_IIR_NO_INT;
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
    }

  return val;
}



int 
do_serial_out(int num, int port, int val)
{
  switch (port - com[num].base_port)
    {
    case UART_RX:
/*  case UART_DLL: */
      if (DLAB(num)) { com[num].dll = val; ser_termios(num); }
      else put_tx(num, val);
      break;

    case UART_IER:
/*  case UART_DLM: */
      if (DLAB(num)) { com[num].dlm = val; ser_termios(num); }
      else com[num].IER = val;
      break;

    case UART_IIR:
      /* read-only */
      break;

    case UART_LCR:
      put_lcr(num, val);
      break;

    case UART_MCR:
      com[num].MCR = val;
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

	switch(HI(ax)) {
		case 0: /* init */
			LWORD(eax) = 0;
			num = LWORD(edx);
			s_printf("init serial %d\n", num);
			break;
 	        case 1: /* write char */
			/* dbug_printf("send serial char '%c' com%d (0x%x)\n",
			   LO(ax), LO(dx)+1, LO(ax)); */
			put_tx(LO(dx), LO(ax));
			HI(ax) = 96;
			break;
 	        case 2: /* read char */
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
 	        case 3: /* port status */

			if (LO(dx) > 3) LO(dx) = 3;
			HI(ax) = 0x60;
			LO(ax) = 0xb0;   /* CD, DSR, CTS */
			if ( ser_avail(LO(dx)) )
			  {
			    HI(ax) |= 1; /* receiver data ready */
			    /* dbug_printf("BIOS serial port status com%d = 0x%x\n",
				   LO(dx), LWORD(eax)); */
			  }
			break;
	        case 4: /* extended initialize */
			s_printf("extended serial initialize\n");
			return;
		default:
			error("ERROR: serial error\n");
			show_regs();
			/* fatalerr = 5; */
			return;
	}
}


int
begin_serint (int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;
  com[num].RX = ser_dequeue(num);
  com[num].IIR = UART_IIR_RDI;
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE | UART_LSR_DR;
  com[num].in_interrupt=1; 
  return 0;
}



int
end_serint (int intnum)
{
  u_char num = (intnum == com[0].interrupt) ? 0 : 1;
  com[num].in_interrupt = 0;
  com[num].IIR = UART_IIR_NO_INT;
  com[num].LSR = UART_LSR_TEMT | UART_LSR_THRE;
  return 0;
}


inline void
do_ser(int num)
{
  com[num].dready = 1;
  if ((com[num].IER & UART_IER_RDI) && (com[num].MCR & UART_MCR_OUT2))
    {
      com[num].in_interrupt=1;
      /* must dequeue RX in begin_serint() */
      queue_hard_int(com[num].interrupt,begin_serint,end_serint);
      /* s_printf("COM: queuing interrupt!\n"); */
    }
  else {
    com[num].RX = ser_dequeue(num);
    s_printf("COM: no interrupt on data!\n");
  }

  mutex_get(param->ser_locked);
  param->ser[num].num_ints--;
  if (param->ser[num].num_ints < 0) 
    error("ERROR: num_ints (%d) < 0\n", num);
  mutex_release(param->ser_locked);
}


/* I do the business with first/OTHER to make sure each gets a chance 
 * to be serviced.  Ugly.
 */
int
serial_run(void)
{
  static int first=0;
  int rtn;
#define OTHER (first ? 0 : 1)

  rtn=1;
  if (!com[first].in_interrupt && param->ser[first].num_ints) do_ser(first);
  else if (!com[OTHER].in_interrupt && param->ser[OTHER].num_ints) do_ser(OTHER);
  else rtn=0;

  first = OTHER;
  return rtn;
}


void
sigser(int sig)
{
}


