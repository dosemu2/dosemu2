/* dos emulator, Matthias Lautner */
#define EMU_C 1
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/02/16 00:21:29 $
 * $Source: /usr/src/dos/RCS/emu.c,v $
 * $Revision: 1.18 $
 * $State: Exp $
 *
 * Revision 1.11  1993/01/25  22:59:38  root
 * added the -H and -F options to choose # of hard/flpy disks.
 * LINUX.EXE won't work with more than 2 floppies defined, so I changed
 * default number of floppy disks to 2.
 *
 * Revision 1.7  1993/01/16  01:15:32  root
 * fixed various things, like the BDA base port address of the CRTC.
 * now telix "port checked" writes work, as does tv.com (partly).
 * fixed the int 10h, ah=09 problem (the func wasn't there :-).
 *
 */


/* make sure that this line ist the first of emu.c
   and link emu.o as the first object file to the lib */
	__asm__("___START___: jmp _emulate\n");

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <termio.h>
#include <termcap.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>
#include <getopt.h>
#include <linux/fd.h>
#include "emu.h"
#include "dosvga.h"
#include "timers.h"
#include "cmos.h"

#ifdef XMS
#include "xms.h"
#endif

/* #define CO	80
   #define LI	25 */

/* would use the info termio.c nicely got for us, but it works badly now */
#define CO	co2
#define LI	li2

#define SCREEN_ADR(s)	(us *)(0xb8000 + s * 0x1000)

#define OUTBUFSIZE	1500    /* was 3000 */

/* thanks to Andrew Haylett (ajh@gec-mrc.co.uk) for catching character loss
 * in CHOUT, and other things */
#define CHOUT(c)   if (outp == &outbuf[OUTBUFSIZE]) { CHFLUSH } \
			*outp++ = (c);
#define CHFLUSH    if (outp > outbuf) { v_write(2, outbuf, outp - outbuf); \
						outp = outbuf; }
#define SETIVEC(i, seg, ofs)	((us *)0)[ (i<<1) +1] = (us)seg; \
				((us *)0)[  i<<1    ] = (us)ofs

extern struct disk disktab[DISKS],
                  hdisktab[HDISKS];

/* the default configuration is two floppies and two hard disks. You
 * can change it with the -H and -F flags, or by editing the numbers
 * below.  For example, you may wish to have only the hdimage hard drive,
 * plus your LINUX.EXE drive (i.e. no direct-access to your DOS
 * partition.
 * !!!!! You do this in the Makefile now! Don't tamper with it here! !!!!
 */
int fdisks=DEF_FDISKS,
    hdisks=DEF_HDISKS;

int exitearly=0;

struct vm86_struct vm86s;
int error;
struct timeval scr_tv;
struct itimerval itv;
unsigned char outbuf[OUTBUFSIZE], *outp = outbuf;
int iflag;
int hdiskboot =0;
int scrtest_bitmap;
long start_time;
unsigned long last_ticks;
int screen, xpos[8], ypos[8];
int mem_size=640, extmem_size=MAX_XMS;

int card_init=0;
unsigned long precard_eip, precard_cs;

/* this is the array of interrupt vectors */
struct vec_t {
  unsigned short offset;
  unsigned short segment;
} *ivecs=0;

struct vec_t orig[256];		/* "original" interrupt vectors */
struct vec_t snapshot[256];	/* vectors from last snapshot */

#define MEM_SIZE	mem_size   /* 1k blocks of normal RAM */

#ifdef EXPERIMENTAL_GFX
#define MAX_MEM_SIZE    640
#else
#define MAX_MEM_SIZE	734	   /* up close to the 0xB8000 mark */
#endif

/* initial reported video mode */
#ifdef MDA_VIDEO
#define INIT_SCREEN_MODE	2  /* 80x25 MDA monochrome */ 
#define VID_COMBO		1
#define VID_SUBSYS		1  /* 1=mono */
#else
#define INIT_SCREEN_MODE	3  /* 80x25 VGA color */
#define VID_COMBO		4  /* 4=EGA (ok), 8=VGA (not ok) */
#define VID_SUBSYS		0  /* 0=color */
#endif

/* without disk information, w/3 printers, 2 serial ports */
#ifdef MATHCO
#define CONFIGURATION (0xc90e | INIT_SCREEN_MODE)
#else
#define CONFIGURATION (0xc90c | INIT_SCREEN_MODE)
#endif

void boot(struct disk *);

extern void map_bios(void);
extern int open_kmem();

void leavedos(int sig),
  usage(void),
  robert_irq(int sig),
  hardware_init(void),
  show_ints(int),
  do_int(int);

int printer_print(int, char), serial_write(int, char), serial_read(int);

FILE *lpt[3]={ NULL, NULL, NULL };

/************************************************************/
/* DEVICE file names....  USER! change these to your liking */
/************************************************************/
char *prtnames[] =
{
  "dosemulpt1",  "dosemulpt2", "dosemulpt3"
};
char *comnames[] =
{
  "/dev/modem", "doscom2", "doscom3", "doscom4"
};
/************************************************************/

/* Universal Asynchronous Receiver/Transmitter, 8250/16450 */
struct uart
{
  int base;	/* base port, i.e. com2=0x2f8 */
  unsigned char
    RX,		/* receive */
    TX,		/* transmit */
    IER, 	/* interrupt enable */
    IIR,	/* interrupt ID */
    FCR,	/* FIFO control */
    LCR, 	/* line control */
    MCR,	/* modem control */
    LSR,	/* line status */
    MSR,	/* modem status */
    SCR;	/* scratch */
  int fd;       /* file descriptor */
} uarts[4];

/* Programmable Interrupt Controller, 8259 */
struct pic
{
  int stage;	/* where in init. , 0=ICW1 */
  	/* the seq. is ICW1 to 0x20, ICW2 to 0x21
         * if ICW1:D1=0, ICW3 to 0x20h, 
	 * ICW4 to 0x21, OCWs any order
	 */
  unsigned char
    ICW1,	/* Input Control Words */
    ICW2,
    ICW3,
    ICW4,
    OCW1,	/* Output Control Words */
    OCW2,
    OCW3;
} pics[2];

int port61=0xd0;  /* the pseudo-8255 device on AT's */

/* apparently, DOS 5.0 issues some nasty instructions
 * when it's booting.  We want it to boot, and there
 * seems to be no harm done, so we just let the SIGSEGV's
 * dribble onto the floor for a while.
 */
int boot_in_progress=0;		/* 1 if booting */
int console_video=0,
    console_keyb=0,		/* 1 if uses PC console */
    vga=0,			/* use VGA controls on sonsole */
    graphics=0;                 /* can do graphics */
int gfx_mode = TEXT;
int max_page = 4;		/* number of highest vid page - 1*/

/* keybint should really be 1 all the time, but until I get it
 * stable...the default value is set in emulate()
 */
int keybint;
int xms=0;
int mapped_bios=0, special_nowait=0;

struct ioctlq iq = {0,0,0,0};	/* one-entry queue :-( for ioctl's */
int in_sighandler=0;		/* so I know to not use non-reentrant
				 * syscalls like ioctl() :-( */
int in_ioctl=0;
struct ioctlq curi = {0,0,0,0};				 

/* this is DEBUGGING code! */
int timers=0;			/* no timer emulation */
int sizes=0;

int screen_mode=INIT_SCREEN_MODE;

struct debug_flags d = {0,0,0,0,1,0,1,1,0,0,0,1,1,1,0,1};

int poll_io=1;			/* polling io, default on */
int ignore_segv;		/* ignore sigsegv's */
int terminal_pipe;
FILE *terminal;

extern unsigned int kbd_flags,
  		    key_flags;
int in_interrupt=0;		/* for unimplemented interrupt code */


unsigned char trans[] = /* LATIN CHAR SET */
{
	"\0\0\0\0\0\0\0\0\00\00\00\00\00\00\00\00"
	"\0\0\0\0\266\247\0\0\0\0\0\0\0\0\0\0"
	" !\"#$%&'()*+,-./"
	"0123456789:;<=>?"
	"@ABCDEFGHIJKLMNO"
	"PQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmno"
	"pqrstuvwxyz{|}~ "
	"\307\374\351\342\344\340\345\347\352\353\350\357\356\354\304\305"
	"\311\346\306\364\366\362\373\371\377\326\334\242\243\245\0\0"
	"\341\355\363\372\361\321\252\272\277\0\254\275\274\241\253\273"
	"\0\0\0|++++++|+++++"
	"++++-++++++++-++"
	"+++++++++++\0\0\0\0\0"
	"\0\337\0\0\0\0\265\0\0\0\0\0\0\0\0\0"
	"\0\261\0\0\0\0\367\0\260\267\0\0\0\262\244\0" 
};


inline void run_vm86(void)
{
  /* always invoke vm86() with this call.  all the messy stuff will
   * be in here.
   */
  
  (void)vm86(&vm86s);

  /* this is here because ioctl() is non-reentrant, and signal handlers
   * may have to use ioctl().  This results in a possible (probable) time 
   * lag of indeterminate time.  Ah, life isn't perfect.
   *
   * I really need to clean up the queue functions to use real queues.
   */
  if (iq.queued) 
      do_queued_ioctl();
}

int outcbuf(int c)
{
  CHOUT(c);
  return 1;
}

int dostputs(char *a, int b, outfuntype c)
{
  /* discard c right now */
  CHFLUSH;
  tputs(a,b,outc);
}

void poscur(int x, int y)
{
  if ((unsigned)x >= CO || (unsigned)y >= LI) return;       /* were "co" and "li" */
  tputs(tgoto(cm, x, y), 1, outc);
}

void scrollup(int x0, int y0 , int x1, int y1, int l, int att)
{
	int dx, dy, x, y, ofs;
	us *sadr, *p, *q, blank = ' ' | (att << 8);


	if(l==0)		/* Wipe mode */
	{
		sadr=SCREEN_ADR(screen);
/*		must_update=1; */
		for(dy=y0;dy<=y1;dy++)
			for(dx=x0;dx<=x1;dx++)
				sadr[dy*CO+dx]=blank;
		return;
	}

	sadr = SCREEN_ADR(screen);
	sadr += x0 + CO * (y0 + l);
	dx = x1 - x0 +1;
	dy = y1 - y0 - l +1;
	ofs = -CO * l;
	for (y=0; y<dy; y++) {
		p = sadr;
		if (l != 0) for (x=0; x<dx; x++, p++) p[ofs] = p[0];
		else        for (x=0; x<dx; x++, p++) p[0] = blank;
		sadr += CO;
	}
	for (y=0; y<l; y++) {
		sadr -= CO;
		p = sadr;
		for (x=0; x<dx; x++, p++) *p = blank;
	}
}

void scrolldn(int x0, int y0 , int x1, int y1, int l, int att)
{
	int dx, dy, x, y, ofs;
	us *sadr, *p, blank = ' ' | (att << 8);

	if(l==0)
	{
		l=LI-1;		/* Clear whole if l=0 */
/*		must_update=1; */
	}

	sadr = SCREEN_ADR(screen);
	sadr += x0 + CO * (y1 -l);
	dx = x1 - x0 +1;
	dy = y1 - y0 - l +1;
	ofs = CO * l;
	for (y=0; y<dy; y++) {
		p = sadr;
		if (l != 0) for (x=0; x<dx; x++, p++) p[ofs] = p[0];
		else        for (x=0; x<dx; x++, p++) p[0] = blank;
		sadr -= CO;
	}
	for (y=0; y<l; y++) {
		sadr += CO;
		p = sadr;
		for (x=0; x<dx; x++, p++) *p = blank;
	}
}

v_write(int fd, unsigned char *ch, int len)
{
  if (!console_video) write(fd, ch, len);
  else error("ERROR: (video) v_write deferred for console_video\n");
}

void char_out_att(unsigned char ch, unsigned char att, int s)
{
	us *sadr, *p;

	if (s > max_page) return;

	if (console_video)
	  {
	    if (ch >= ' ') {
	      sadr = SCREEN_ADR(s);
	      sadr[ypos[s]*CO + xpos[s]++] = ch | (att << 8);
	    } else if (ch == '\r') {
	      xpos[s] = 0;
	    } else if (ch == '\n') {
	      ypos[s]++;
	      /* is this correct? EDLIN needs it */
	      xpos[s]=0;
	    } else if (ch == '\010' && xpos[s] > 0) {
	      xpos[s]--;
	    } else if (ch == '\t') {
	      v_printf("tab\n");
	      do {
		char_out(' ', s);
	      } while (xpos[s] % 8 != 0); 
	    }

	    poscur(xpos[s], ypos[s]);
	  }

	else {
	  if (ch >= ' ') {
	    sadr = SCREEN_ADR(s);
	    sadr[ypos[s]*CO + xpos[s]++] = ch | (7 << 8);
	    if (s == screen) outc(trans[ch]);
	  } else if (ch == '\r') {
	    xpos[s] = 0;
	    if (s == screen) write(2, &ch, 1);
	  } else if (ch == '\n') {
	    ypos[s]++;
	    if (s == screen) write(2, &ch, 1);
	  } else if (ch == '\010' && xpos[s] > 0) {
	    xpos[s]--;
	    if (s == screen) write(2, &ch, 1);
	  } else if (ch == '\t') {
		do {
		    char_out(' ', s);
	        } while (xpos[s] % 8 != 0);
	  } else if (ch == '\010' && xpos[s] > 0) {
	    xpos[s]--;
	    if (s == screen) write(2, &ch, 1);
	  }
	}

	if (xpos[s] == CO) {
		xpos[s] = 0;
		ypos[s]++;
	}
	if (ypos[s] == LI) {
		ypos[s]--;
		scrollup(0, 0, CO-1, LI-1, 1, 7);
	}
	*(unsigned char *)(0x450 + s) = ypos[s];
	*(unsigned char *)(0x451 + s) = xpos[s];
}

void clear_screen(int s, int att)
{
	us *sadr, *p, blank = ' ' | (att << 8);
	int lx, ly;

	if (s > max_page) return;
	if (!console_video)
	  if (s == screen) tputs(cl, 1, outc);
	xpos[s] = ypos[s] = 0;
	poscur(0,0);
	sadr = SCREEN_ADR(s);
	for (p = sadr; p < sadr+2000; *p++ = blank);
}

void restore_screen(void)
{
	us *sadr, *p; 
	unsigned char c, a;
	int x, y, oa;

	v_printf("RESTORE SCREEN\n");

	if (console_video) {
	  v_printf("restore cancelled for console_video\n");
	  return;
	}

	sadr = SCREEN_ADR(screen);
	oa = 7; 
	p = sadr;
	for (y=0; y<LI; y++) {
		dostputs(tgoto(cm, 0, y), 1, outcbuf);
		for (x=0; x<CO; x++) {
			c = *(unsigned char *)p;
			if ((a = ((unsigned char *)p)[1]) != oa) {
#ifndef OLD_RESTORE
			  /* do fore/back-ground colors */      
			  if (!(a & 7) || (a & 0x70)) dostputs(mr, 1, outcbuf);
			  else dostputs(me, 1, outcbuf);

			  /* do high intensity */
			  if (a & 0x8) dostputs(md, 1, outcbuf);
			  else if (oa & 0x8)
			    {
			      dostputs(me, 1, outcbuf);
			      if (!(a & 7) || (a & 0x70))
				dostputs(mr, 1, outcbuf);
			    }

			  /* do underline/blink */
			  if (a & 0x80) dostputs(so, 1, outcbuf);
			  else if (oa & 0x80) dostputs(se, 1, outcbuf);
			  
			  oa = a;   /* save old attr as current */
#else
				if ((a & 7) == 0) dostputs(mr, 1, outcbuf);
				else dostputs(me, 1, outcbuf);
				if ((a & 0x88)) dostputs(md, 1, outcbuf);
				oa = a;
#endif
			}
			CHOUT(trans[c] ? trans[c] : '_');
			p++;
		}
	}
	dostputs(me, 1, outcbuf);
	CHFLUSH;
	poscur(xpos[screen],ypos[screen]);
}


void disk_close(void) {
	struct disk * dp;

	for (dp = disktab; dp < &disktab[fdisks]; dp++) {
		if (dp->removeable && dp->fdesc >= 0) {
		        d_printf("DISK: Closing a disk\n");
			(void)close(dp->fdesc);
			dp->fdesc = -1;
		}
	}
}

void disk_open(struct disk *dp)
{
struct floppy_struct fl;

	if (dp == NULL || dp->fdesc >= 0) return;
	dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
	if (dp->fdesc < 0) {
		error("ERROR: can't open %s\n", dp->dev_name);
		error = 5;
		return;
	}
	if (ioctl(dp->fdesc, FDGETPRM, &fl) == -1) {
		if (errno == ENODEV) { /* no disk available */
			dp->sectors = 0;
			dp->heads = 0;
			dp->tracks = 0;
			return;
		}
		error("ERROR: can't get floppy parameter of %s (%s)\n", dp->dev_name, sys_errlist[errno]);
		error = 5;
		return;
	}
	d_printf("FLOPPY %s h=%d, s=%d, t=%d\n", dp->dev_name, fl.head, fl.sect, fl.track);
	dp->sectors = fl.sect;
	dp->heads = fl.head;
	dp->tracks = fl.track;
}

void disk_close_all(void)
{
	struct disk * dp;

	for (dp = disktab; dp < &disktab[fdisks]; dp++) {
		if (dp->fdesc >= 0) {
			(void)close(dp->fdesc);
			dp->fdesc = -1;
		}
	}
	for (dp = hdisktab; dp < &hdisktab[hdisks]; dp++) {
		if (dp->fdesc >= 0) {
			(void)close(dp->fdesc);
			dp->fdesc = -1;
		}
	}
}


void disk_init(void)
{
	int s;
	struct disk * dp;
	struct stat stbuf;
	char buf[30];

	for (dp = disktab; dp < &disktab[fdisks]; dp++) {
	  if (stat(dp->dev_name, &stbuf) < 0) {
	    error("ERROR: can't stat %s\n", dp->dev_name);
	    leavedos(1);
	  }
	  if (S_ISBLK(stbuf.st_mode)) d_printf("ISBLK\n");
	  d_printf ("dev : %x\n", stbuf.st_rdev);
	  if (S_ISBLK(stbuf.st_mode) && (stbuf.st_rdev & 0xff00) == 0x200) {
	    d_printf("DISK %s removeable\n", dp->dev_name);
	    dp->removeable = 1;
	    dp->fdesc = -1;
	    continue;
	  }
	  dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
	  if (dp->fdesc < 0) {
	    error("ERROR: can't open %s\n", dp->dev_name);
	    leavedos(1);
	  }
	}
	for (dp = hdisktab; dp < &hdisktab[hdisks]; dp++) {
	  dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
	  dp->removeable = 0;
	  if (dp->fdesc < 0) {
	    error("ERROR: can't open %s\n", dp->dev_name);
	    leavedos(1);
	  }

	  /* I haven't examined this code, so I don't know what it does.
	     I can, however, tell you what it doesn't do: work! -Robert */
#if 0
	  if (read(dp->fdesc, buf, 30) != 30) {
	    error("ERROR: can't read disk info of %s\n", dp->dev_name);
	    leavedos(1);
	  }
	  dp->sectors = *(us *)&buf[24];
	  dp->heads = *(us *)&buf[26];
	  s = *(us *)&buf[19] + *(us *)&buf[28];  /* used + hidden sectors */
	  dp->tracks = s / (dp->sectors * dp->heads);
	  d_printf("disk %s; h=%d, s=%d, t=%d, sz=%d, hid=%d\n",
		      dp->dev_name, dp->heads, dp->sectors, dp->tracks,
		      s, *(us *)&buf[28]);
	  if (s % (dp->sectors * dp->heads) != 0) {
	    error("ERROR: can't read track number of %s\n", dp->dev_name);
	    leavedos(1);
	  }
#endif
	}
}


void show_regs(void)
{
	int i;
	unsigned char *cp = SEG_ADR((unsigned char *), cs, ip);

	dbug_printf("\nEIP: %04x:%08x",0xffff & _regs.cs,_regs.eip);
	dbug_printf(" ESP: %04x:%08x",0xffff & _regs.ss,_regs.esp);
	dbug_printf(" EFLAGS: %08x",_regs.eflags);
	dbug_printf("\nEAX: %08x EBX: %08x ECX: %08x EDX: %08x",
		    _regs.eax,_regs.ebx,_regs.ecx,_regs.edx);
	dbug_printf("\nESI: %08x EDI: %08x EBP: %08x",
		    _regs.esi, _regs.edi, _regs.ebp);
	dbug_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n",
		    0xffff & _regs.ds,0xffff & _regs.es,
		    0xffff & _regs.fs,0xffff & _regs.gs);
	dbug_printf("DIS: ");
	for (i=0; i<10; i++)
		dbug_printf("%02x ", *cp++);
	dbug_printf("\n");
}

int inb(int port)
{
	static unsigned int cga_r=0;
	static unsigned int tmp=0;
	int holder;
	unsigned int tmpkeycode;

	port &= 0xffff;

	/* graphic status - many programs will use this port to sync with
	 * the vert & horz retrace so as not to cause CGA snow */
	if ((port == 0x3da) || (port == 0x3ba))
	  return (cga_r ^= 1) ? 0xcf : 0xc6;
	else
	  switch (port)
	    {
	    case 0x60:
#ifndef new8042
	      k_printf("direct 8042 read1: 0x%02x\n", lastscan);
	      tmp=lastscan;
	      lastscan=0;
	      return tmp;
#else
  /* this code can't send non-ascii scancodes like alt/ctrl/scrollock,
     nor keyups, etc. */
	      if (!ReadKeyboard(&tmpkeycode, NOWAIT)) 
		{
		  k_printf("failed direct 8042 read\n");
		  return 0;
		}
	      else {
		tmp=tmpkeycode >> 8;
		k_printf("direct 8042 read: 0x%02x\n", tmp);
		return tmp;
	      }
	      break;
#endif
	    case 0x64:
	      tmp=0x10 | (lastscan ? 1 : 0);  /* low bit set = sc ready */
	      /* lastscan=0; */
	      k_printf("direct 8042 status check: 0x%02x\n", tmp);
	      return tmp;
	    case 0x61:
	      i_printf("inb [0x61] =  0x%x (8255 chip)\n", port61);
	      return port61;

	    case 0x70:
	    case 0x71:
	      return cmos_read(port);

	    case 0x2f8:
	      serial_read(1);
	      s_printf("com2 RX --> %c [%d]\n", uarts[1].RX, uarts[1].RX);
	      return uarts[1].RX;
	      break;
	    case 0x2f9:
	      s_printf("com2 IER --> 0x%2x\n", uarts[1].IER);
	      return uarts[1].IER;
	      break;
	    case 0x2fA:
	      s_printf("com2 IIR --> 0x%2x\n", uarts[1].IIR);
	      s_printf("setting IIR to 0x01\n");
	      tmp=uarts[1].IIR;
	      uarts[1].IIR = 0x1;
	      return tmp;
	      break;
	    case 0x2fb:
	      s_printf("com2 LCR --> 0x%2x\n", uarts[1].LCR);
	      return uarts[1].LCR;
	      break;
	    case 0x2fc:
	      s_printf("com2 MCR --> 0x%2x\n", uarts[1].MCR);
	      return uarts[1].MCR;
	      break;
	    case 0x2FD:
/*	      get_LSR(1); */
	      uarts[1].LSR &= 0xfe;
	      s_printf("com2 LSR --> 0x%2x\n", uarts[1].LSR); 
	      return uarts[1].LSR;
	      break;
	    case 0x2fe:
	      s_printf("com2 MSR --> 0x%2x\n", uarts[1].MSR);
	      return uarts[1].MSR;
	      break;
	    case 0x2ff:
	      s_printf("com2 SCR --> 0x%2x\n", uarts[1].SCR);
	      return uarts[1].SCR;
	      break;
	    case 0x40:
	      i_printf("inb [0x41] = 0x%x  1st timer inb\n",
			  pit.CNTR0--);
	      return pit.CNTR0;
	    case 0x41:
	      i_printf("inb [0x41] = 0x%x  2nd timer inb\n",
			  pit.CNTR1--);
	      return pit.CNTR1;
	    case 0x42:
	      i_printf("inb [0x42] = 0x%x  3rd timer inb\n",
			  pit.CNTR2--);
	      return pit.CNTR2;
	    case 0x3bc:
	      i_printf("printer port inb [0x3bc] = 0\n");
	      return 0;
	    case 0x3db:  /* light pen strobe reset */
	      return 0;
	    default:
	      i_printf("inb [0x%x] = 0x%x\n", port, _regs.eax);
	      return 0;
	    }
	return 0;
}

void outb(int port, int byte)
{
  int comidx;
  static int timer_beep=0;
  static int lastport=0;

  port &= 0xffff;
  byte &= 0xff;

  i_printf("outb [0x%x] 0x%x   ", port, byte);
  if ((port == 0x60) || (port == 0x64)) i_printf("keyboard outb\n");
  else if (port == 0x61) 
    {
      port61=byte;
      i_printf("8255 outb\n");
      if ((byte & 3 == 3) && (timer_beep == 1))
	{
	  i_printf("beep!\n");
	  fprintf(stderr,"\007");
	  timer_beep=0;
	} 
      else timer_beep=1;
    }

  else if ((port == 0x70) || (port == 0x71))
    cmos_write(port, byte);

  else if (port >= 0x40 && port <= 0x43)
    {
      i_printf("timer outb 0x%02x\n", byte);
      if ((port == 0x42) && (lastport == 0x42))
	{
	  if (timer_beep == 1)
	    {
	      fprintf(stderr,"\007");
	      timer_beep=0;
	    }
	  else timer_beep=1;
	}
    }
  else
    {
      int baseport=port & 0xfff8;	/* mask out low 3 bits */
      int thre_int;			/* irq for transmit done */

      /* if port is in a UART, perform UART emulation */
      for (comidx = 0; comidx <= 3; comidx ++)
	{
/*	  s_printf("outb port: 0x%04x, combase: 0x%04x, low: 0x%04x\n",
		 port & 0xfff8, uarts[comidx].base, port & 0x7); */
	  if (uarts[comidx].base == (port & 0xfff8))
	    switch ((port & 0x7))		/* get low 3 bits */
	      {
	      case 0:
		s_printf("com%d TX := %c [0x%x]\n", comidx+1, byte, byte);
		serial_write(comidx, byte);
		if (comidx & 1) thre_int=3 /* 0xb */;  /* for COM2 and COM4 only! */
		else thre_int= 4 /* 0xc */;		/* COM1 and COM3 */
		
		if (1 /* uarts[comidx].IER & 2 */ ) 
		  {
		    int i;
		    s_printf("Calling THRE interrupt 0x%x\n", thre_int);
		    for (i=0; i < 40000; i++)
		      ;
		    do_int(4 /* thre_int */);
		  }
		break;
	      case 1:
		s_printf("com%d IER := 0x%x\n", comidx+1,byte);
		uarts[comidx].IER = byte;
		break;
	      case 2:
		s_printf("com%d FCR := 0x%x\n", comidx+1,byte);
		uarts[comidx].FCR = byte;
		break;
	      case 3:
		s_printf("com%d LCR := 0x%x\n", comidx+1, byte);
		uarts[comidx].LCR = byte;
		break;
	      case 4:
		s_printf("com%d MCR := 0x%x\n", comidx+1,byte);
		uarts[comidx].MCR = byte;
		break;
	      case 5:
		s_printf("com%d SCR := 0x%x\n", comidx+1, byte);
		uarts[comidx].SCR = byte;
		break;
	      }
	}
    }
  lastport = port;
}

void dos_ctrl_alt_del(void)
{
  dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
  disk_close();
  clear_screen(screen,0);
  show_cursor();
  special_nowait=0;
  p_dos_str("Rebooting DOS.  Be careful...this is partially implemented\r\n");
  boot(hdiskboot? hdisktab : disktab);
}

void dos_ctrlc(void)
{
  k_printf("DOS ctrl-c!\n");
  p_dos_str("^C\n\r");   /* print DOS's ctrl-c message */
  keybuf_clear();
  if (_regs.eflags & IF) do_int(0x23);
  else warn("CAN'T DO INT 0x23: IF CLEAR\n");
}

void boot(struct disk *dp)
{
  char *buffer;
  unsigned int i;
  unsigned char *ptr;
  
  boot_in_progress=1;

  cmos_init();
  
  disk_close();
  disk_open(dp);
  buffer = (char *)0x7c00;
  memset(NULL, 0, 0x7c00); /* clear the first 32 k */
  memset((char *)0xff000, 0xF4, 0x1000); /* fill the last page w/HLT */
  /* init trapped interrupts if called via jump */
  
  for (i=0; i<256; i++) {
    if ((i & 0xf8) == 0x60) continue; /* user interrupts */
    SETIVEC(i, 0xe000, 16*i);
    ptr=(unsigned char *)(0xe000*16 + 16*i);
    ptr[0]=0xcd;     /* 0xcd = INT */
    ptr[1]=i;
    ptr[2]=0xcf;     /* 0xcf = IRET */
  }
  
  SETIVEC(0x33,0,0);     /* zero mouse address */
  SETIVEC(0x1c,0xe000,2);  /* should be an IRET */
  SETIVEC(0x67,0,0);  /* no EMS */

#ifdef XMS
  /* XMS has it's handler just after the interrupt dummy segment */
  ptr=(unsigned char *)(XMSControl_ADD);
  ptr[0]=0xeb;       /* jmp short forward 3 */
  ptr[1]=3;
  ptr[2]=0x90;       /* nop */
  ptr[3]=0x90;       /* nop */
  ptr[4]=0x90;       /* nop */
  ptr[5]=0xf4;       /* HLT...the current emulator trap */
  ptr[6]=XMS_MAGIC;  /* just an info byte. reserved for later */
  ptr[7]=0xcb;       /* FAR RET */
  xms_init();
#endif
  
  *(us *)0x402 = 0x2f8; 	/* com port addresses */
  *(us *)0x404 = 0x3f8;  
  *(us *)0x406 = 0x2e8;
  *(us *)0x408 = 0x3e8;
  *(us *)0x410 = CONFIGURATION;
  if (fdisks > 0) *(us *)0x410 |= 1 | ((fdisks -1)<<6);
  
  *(us *)0x413 = MEM_SIZE;	/* size of memory */
  *(char *)0x449 = screen_mode;	/* screen mode */
  *(us *)0x44a = CO;	/* chars per line */
  *(us *)0x44c = CO * LI * 2;  /* size of video regen area in bytes */
  *(us *)0x44e = 0;       /* offset of current page in buffer */
  *(unsigned char *)0x484 = LI-1;    /* lines on screen - 1 */
  *(us *)0x41a = 0x1e;	/* key buf start ofs */
  *(us *)0x41c = 0x1e;	/* key buf end ofs */
  /* key buf 0x41e .. 0x43d */
  
  keybuf_clear();
  
  *(us *)0x463 = 0x3d4;           /* base port of CRTC - IMPORTANT! */
  *(char *)0x465 = 9;		  /* current 3x8 (x=b or d) value */
  *(char *)0x487 = 0x61;
  *(char *)0x488 = 0x81;          /* video display data */
  *(char *)0x48a = VID_COMBO;     /* video type */

  *(char *)0x496 = 16;		  /* 102-key keyboard */
  *(long *)0x4a8 = 0;             /* pointer to video table */

  
  /* from Alan Cox's mods */
  /* we need somewhere for the bios equipment. Put it at d0000 */
  *(char *)0xd0000=0x09;
  *(char *)0xd0001=0x00;	/* 9 byte table */
  *(char *)0xd0002=0xFC;        /* PC AT */
  *(char *)0xd0003=0x00;
  *(char *)0xd0004=0x04;	/* bios revision 4 */
  *(char *)0xd0005=0x20;	/* no mca no ebios no wat no keybint
				   rtc no slave 8259 no dma 3 */
  *(char *)0xd0006=0x00;
  *(char *)0xd0007=0x00;
  *(char *)0xd0008=0x00;
  *(char *)0xd0009=0x00;
  
  lseek(dp->fdesc, 0, 0);
  i = read(dp->fdesc, buffer, 512);
  if (i != 512) {
    error("can't boot from disk, using harddisk\n");
    dp = hdisktab;
    lseek(dp->fdesc, 0, 0);
    i = read(dp->fdesc, buffer, 512);
    if (i != 512) {
      error("ERROR: can't boot from disk\n");
      leavedos(1);
    }
  }
  disk_close();
  _regs.eax = _regs.ebx = _regs.edx = 0;
  _regs.ecx = 0;
  _regs.ebp = _regs.esi = _regs.edi = _regs.esp = 0;
  _regs.cs = _regs.ss = _regs.ds = _regs.es = _regs.fs = _regs.gs = 0x7c0;
  _regs.eip = 0;
  _regs.eflags = 0;
  boot_in_progress=0;

  if (exitearly)
    {
      dbug_printf("Leaving DOS before booting\n");
      leavedos(0);
    }
}


void show_ints(int max)
{
  int i;

  max |= 2;
  for (i=0x0; i<=max; i+=3)  /* interrupt tbl. entries 0 - 0xf */
    {
      dbug_printf("%02x| %04x:%04x->%05x    ",i,ISEG(i), IOFF(i),
		  IVEC(i));
      dbug_printf("%02x| %04x:%04x->%05x    ",i+1,ISEG(i+1), IOFF(i+1),
		  IVEC(i+1));
      dbug_printf("%02x| %04x:%04x->%05x\n",i+2,ISEG(i+2),IOFF(i+2),
		  IVEC(i+2));
    }

}

hide_cursor()
{
  /* the Linux terminal driver hide_cursor string is
   * "\033[?25l" - put it in as the "vi" termcap in /etc/termcap
   */
  tputs(vi, 1, outc);
}

show_cursor()
{
  /* the Linux terminal driver show_cursor string is
   * "\033[?25h" - put it in as the "ve" termcap in /etc/termcap
   */
  tputs(ve, 1, outc);
}

vga_set_cursor(unsigned short int c)
{
  if (ioperm(0x3c0,0x1b,1))
    {
      error("ERROR: can't get i/o perms in vga_set_cursor\n");
      return(1);
    }

  port_in(0x3da);	 /* reset address/data flipflop */

  port_out(0xa, 0x3d4);
  port_out((c >> 8) * 2, 0x3d5);
  port_out(0xb, 0x3d4);
  port_out((c & 0x31) * 2, 0x3d5);

  ioperm(0x3c0, 0x1b, 0);
}

void int10(void)
{
/********************** new int10() *********************/
/* some code here is copied from Alan Cox ***************/
  int x, y, s, i, tmp;
  static int gfx_flag=0;
  char tmpchar;

  char c, m;
  us *sm;
  NOCARRY;
  switch(HI(ax))
    {
    case 0x0: /* define mode */
      v_printf("define mode: 0x%x\n", LO(ax));
      switch (LO(ax))
	{
	case 2:
	case 3:
	  v_printf("text mode from %d to %d\n", screen_mode, LO(ax));
	  if (gfx_flag && vga)
	    {
#ifdef EXPERIMENTAL_GFX
	      v_printf("returning from gfx mode\n");
	      vga_setmode(TEXT);
	      ioperm(0x3b0, 0x3db-0x3b0, 0);
	      show_cursor();
#else
	      error("returning from gfx mode, but no gfx support!\n");
#endif
	    }
	  max_page=4;
	  screen_mode = LO(ax);
	  gfx_flag=0;  /* we're in a text mode now */
	  gfx_mode = TEXT;
	  /* mode change clears screen unless bit7 of AL set */
	  if (! (LO(ax) & 0x80)) clear_screen(screen,0);
	  NOCARRY;
	  break;
#ifdef EXPERIMENTAL_GFX
      case 0xd:
	  tmp = G320x200x16;
	  goto s_gfx;
      case 0xe:
	  tmp = G640x200x16;
	  goto s_gfx;
      case 0x10:
	  tmp = G640x350x16;
	  goto s_gfx;
      case 0x12:
	  tmp = G640x480x16;
	  goto s_gfx;
      case 0x13:
	  tmp = G320x200x256;
	  goto s_gfx;
s_gfx:	  
	if (vga && graphics)
	  {
	    v_printf("DOS setmode 0x%x\n", LO(ax));
	    screen_mode = LO(ax);
	    gfx_mode=tmp;
#ifdef SYNC_ALOT
	    fflush(stdout);
	    sync();
#endif	    
	    vga_setmode(gfx_mode);
	    /* vga_newscreen(); */
	    ioperm(0x3b0, 0x3db-0x3b0, 1);
	    gfx_flag=1;
	    NOCARRY;
	    v_printf("DOS setmode finished!\n");
	  }
	else
	  {
	    v_printf("non-VGA card...can't do gfx mode 0x%x\n", LO(ax));

	    CARRY;
	    return;
	  }
	break;
#endif  /* EXPERIMENTAL_GFX */
      default:
	v_printf("undefined video mode 0x%x\n",
		 LO(ax));
	CARRY;
	return;
      }

    /* put the mode in the BIOS Data Area */
    ignore_segv=1;
    *(unsigned char *)0x449=screen_mode;
    ignore_segv=0;
    break;

  case 0x1: /* define cursor shape */
    v_printf("define cursor: 0x%x\n", _regs.ecx);
    if (console_video && vga && 0) /* won't run now! */
      {
	v_printf("vga_set_cursor: 0x%x\n", _regs.ecx);
	vga_set_cursor(_regs.ecx);
      }
    else
      {
   /* 0x20 is the cursor no blink/off bit */
   /*	if (HI(cx) & 0x20) */
	if (_regs.ecx == 0x2000) 
	  hide_cursor();
	else show_cursor();
      }
    CARRY;
    break;
  case 0x2: /* set cursor pos */
    s = HI(bx);
    x = LO(dx);
    y = HI(dx);
    if (s != 0) {
      error("ERROR: video error(0x2 s!=0)\n");
      CARRY; 
      return;
    }
    if (x >= CO || y >= LI) 
      break;
    xpos[s] = x;
    ypos[s] = y;
    *(unsigned char *)(0x450 + s) = ypos[s];
    *(unsigned char *)(0x451 + s) = xpos[s];
    if (s == screen) 
      poscur(x, y);
    break;
  case 0x3: /* get cursor pos */
    s = HI(bx);
    if (s != 0) {
      /* CARRY; */
      error("ERROR: video error(0x3 s!=0)\n");
      return;
    }
    _regs.edx = (ypos[s] << 8) | xpos[s];
    break;
  case 0x5: /* change page */ 
    warn("changing page from %d to %d\n", screen, LO(ax));
    if ((s = LO(ax)) == screen) break;
    if (s < max_page) {
      screen = s;
      if (!console_video)
	{
	  scrtest_bitmap = 1 << (24 + screen);
	  vm86s.screen_bitmap = -1;
	}
      return;
    }
    error("ERROR: video error: set wrong screen %d\n", s);
    CARRY;
    return;
  case 0x6: /* scroll up */
    v_printf("scroll up %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    scrollup(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    if (!console_video) vm86s.screen_bitmap = -1;
    break;
  case 0x7: /* scroll down */
    v_printf("scroll dn %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    scrolldn(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    if (!console_video) vm86s.screen_bitmap = -1;
    break;
    
  case 0x8: /* read character at x,y + attr */ 
    s=HI(bx);
    if(s<0||s>max_page)
      {
	CARRY;
	break;
      }
    sm=SCREEN_ADR(s);
    _regs.eax=sm[CO*ypos[s]+xpos[s]];
    break;
    
  case 0x9:
      /* v_printf("int10h ax=0x%04x cx=0x%04x\n", _regs.eax, _regs.ecx); */
#ifdef XDOS
      x = *(us *)&_regs.ecx;
      c = LO(ax);
      s = HI(bx);
      for (i=0; i<x; i++) char_out_att(c,(char)LO(bx),s);
#else
    i=0;	/* repeat count */
    s=HI(bx);
    sm=SCREEN_ADR(s);
    while(i<_regs.ecx)
      {
	sm[CO*ypos[s]+xpos[s]+i]=(LO(bx)<<8)|LO(ax);
	i++;
      }
#endif
    break;

  case 0xA: /* set chars at cursor pos */
      v_printf("int10h ax=0x%04x cx=0x%04x\n", _regs.eax, _regs.ecx);
#ifdef XDOS
      /* xdos code */
      x = *(us *)&_regs.ecx;
      c = LO(ax);
      s = HI(bx);
      for (i=0; i < x; i++) char_out(c, s);
      /* end of xdos code */
#else
    c = _regs.eax&0xFF;;
    s = HI(bx);
    if (s != 0) {
      error("ERROR: video error(0xA s=%d)\n", s);
      CARRY;
      return;
    }
    sm=SCREEN_ADR(s);
    while(i<_regs.ecx)
      {
	sm[CO*ypos[s]+xpos[s]+i]&=0xFF00;
	sm[CO*ypos[s]+xpos[s]+i]|=LO(ax);
	i++;
      }
#endif
    break;
  case 0xe: /* print char */ 
    s=HI(bx);
    if (s > max_page) 
      {
	v_printf("in int10h/ah=e, s=%d\n", s);
	s=screen;
      }
    char_out(*(char *)&_regs.eax, s);    /* char in AL */
    break;
  case 0x0f: /* get screen mode */
    _regs.eax = (CO << 8) | screen_mode;
    v_printf("get screen mode: 0x%04x\n", _regs.eax);
    _regs.ebx &= 0xff;  /* clear top byte (BH) */
    _regs.ebx |= screen << 8;
    break;
#if 0
  case 0xb: /* palette */
  case 0xc: /* set dot */
  case 0xd: /* get dot */
    break;
#endif
  case 0x4: /* get light pen */
    error("ERROR: video error(no light pen)\n");
    CARRY;
    return;
  case 0x1a: /* get display combo */
    if (LO(ax) == 0)
      {
	v_printf("get display combo!\n");
	LO(ax) = 0x1a;          /* valid function=0x1a */
	LO(bx) = VID_COMBO;     /* active display */
	HI(bx) = 0;             /* no inactive display */
      }
    else {
      v_printf("set display combo not supported\n");
    }
    break;
  case 0x12: /* video subsystem config */
    v_printf("get video subsystem config ax=0x%04x bx=0x%04x\n",
	     _regs.eax, _regs.ebx);
    switch (LO(bx))
      {
      case 0x10:
	HI(bx)=VID_SUBSYS;
	/* this breaks qedit! (any but 0x10) */
	/* LO(bx)=3;  */
	v_printf("video subsystem 0x10 BX=0x%04x\n", _regs.ebx);
	_regs.ecx=0x0809;

	break;
      case 0x20:
	v_printf("select alternate printscreen\n");
	break;
      case 0x30:
	if (LO(ax) == 0) tmp=200;
	else if(LO(ax) == 1) tmp=350;
	else tmp=400;
	v_printf("select textmode scanlines: %d", tmp);
	LO(ax)=12;   /* ok */
	break;
      case 0x32:  /* enable/disable cpu access to cpu */
	if (LO(ax) == 0)
	  v_printf("disable cpu access to video!\n");
	else v_printf("enable cpu access to video!\n");
	break;
      default:
	error("ERROR: unrecognized video subsys config!!\n");
	show_regs();
      }
    break;
  case 0xfe: /* get shadow buffer..return unchanged */
  case 0xff: /* update shadow buffer...do nothing */
    break;
  case 0x10: /* ega palette */
  case 0x11: /* ega character generator */
  case 0x4f: /* vesa interrupt */
  default:
    error("new unknown video int 0x%x\n", _regs.eax);
    CARRY;
    break;
  }
}

checkdp(struct disk *disk)
{
  if (disk == NULL) 
    {
      error("DISK: null dp\n");
      return 1;
    }
  else if (disk->fdesc == -1) {
    error("DISK: closed disk\n");
    return 1;
  }
  else return 0;
}

void int13(void)
{
	unsigned int disk, head, sect, track, number, pos, res;
	char *buffer;
	struct disk *dp;
	
	disk = LO(dx);
	if (disk < fdisks) {
		dp = &disktab[disk];
	} else if (disk >= 0x80 && disk < 0x80 + hdisks) 
		dp = &hdisktab[disk - 0x80];
	else dp = NULL;
	switch(HI(ax)) {
		case 0: /* init */
			d_printf("DISK init %d\n", disk);
			_regs.eax &= ~0xff;
			NOCARRY;
			break;
		case 1: /* read error code */	
			_regs.eax &= ~0xff;
			NOCARRY;
			d_printf("DISK error code\n");
			break;
		case 2: /* read */
			disk_open(dp);
			head = HI(dx);
			sect = (_regs.ecx & 0x3f) -1;
			track = (HI(cx)) |
				((_regs.ecx & 0xc0) << 2);
			buffer = SEG_ADR((char *), es, bx);
			number = LO(ax);
			/* d_printf("DISK %d read [h%d,s%d,t%d](%d)->0x%x\n", disk, head, sect, track, number, buffer); */
			if (checkdp(dp) || head >= dp->heads || 
			    sect >= dp->sectors || track >= dp->tracks) {
			    error("ERROR: Sector not found 1!\n");
			    show_regs();
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    break;
			}
			pos = ((track * dp->heads + head) * dp->sectors + sect) << 9;
			if (pos != lseek(dp->fdesc, pos, 0)) {
			    error("ERROR: Sector not found 2!\n");
			    show_regs();
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    break;
			}
			res = read(dp->fdesc, buffer, number << 9);
			if (res & 0x1ff != 0) { /* must read multiple of 512 bytes  and res != -1 */
			    error("ERROR: sector_corrupt 1!\n");
			    show_regs();
			    _regs.eax = 0x200; /* sector corrrupt */
			    _regs.eflags |= CF;
			    break;
			}
			_regs.eax = res >> 9;
			_regs.eflags &= ~CF;
			R_printf("DISK read @%d (%d) OK.\n", pos, res >> 9); 
			break;
		case 3: /* write */
			disk_open(dp);
			head = HI(dx);
			sect = (_regs.ecx & 0x3f) -1;
			track = (HI(cx)) |
				((_regs.ecx & 0xc0) << 2);
			buffer = SEG_ADR((char *), es, bx);
			number = LO(ax);
			W_printf("DISK write [h%d,s%d,t%d](%d)->0x%x\n", head, sect, track, number, buffer); 
			if (checkdp(dp) || head >= dp->heads || 
			    sect >= dp->sectors || track >= dp->tracks) {
			    error("ERROR: Sector not found 3!\n");
			    show_regs();
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    break;
			}
			if (dp->rdonly) {
			    error("ERROR: write protect!\n");
			    show_regs();
			    _regs.eax = 0x300; /* write protect */
			    _regs.eflags |= CF;
			    break;
			}
			pos = ((track * dp->heads + head) * dp->sectors + sect) << 9;
			if (pos != lseek(dp->fdesc, pos, 0)) {
			    error("ERROR: Sector not found 4!\n");
			    show_regs();
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    break;
			}
			res = write(dp->fdesc, buffer, number << 9);
			if (res & 0x1ff != 0) { /* must read multiple of 512 bytes  and res != -1 */
			    error("ERROR: Sector corrupt 2!\n");
			    show_regs();
			    _regs.eax = 0x200; /* sector corrrupt */
			    _regs.eflags |= CF;
			    break;
			}
			_regs.eax = res >> 9;
			_regs.eflags &= ~CF;
			W_printf("DISK write @%d (%d) OK.\n", pos, res >> 9); 
			break;
		case 4: /* test */
			disk_open(dp);
			head = HI(dx);
			sect = (_regs.ecx & 0x3f) -1;
			track = (HI(cx)) |
				((_regs.ecx & 0xc0) << 2);
			number = LO(ax);
			d_printf("DISK %d test [h%d,s%d,t%d](%d)\n", disk, head, sect, track, number);
			if (checkdp(dp) || head >= dp->heads || 
			    sect >= dp->sectors || track >= dp->tracks) {
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    error("ERROR: test: sector not found 5\n");
			    dbug_printf("hds: %d, sec: %d, tks: %d\n",
					dp->heads, dp->sectors, dp->tracks);
			    break;
			}
			pos = ((track * dp->heads + head) * dp->sectors + sect) << 9;
			if (pos != lseek(dp->fdesc, pos, 0)) {
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    error("ERROR: test: sector not found 6\n");
			    break;
			}
#if 0
			res = lseek(dp->fdesc, number << 9);
			if (res & 0x1ff != 0) { /* must read multiple of 512 bytes  and res != -1 */
			    _regs.eax = 0x200; /* sector corrrupt */
			    _regs.eflags |= CF;
			    error("ERROR: test: sector corrupt 3\n");
			    break;
			}
#endif
			_regs.eax = res >> 9;
			_regs.eflags &= ~CF;
			break;
		case 8: /* get disk drive parameters */
			d_printf("disk get parameters %d\n", disk); 

			if (dp != NULL) {
			  /* get CMOS type */
			  /* LO(bx) = 3; */
			  switch(dp->tracks)
			    {
			    case 9:
			      LO(bx)=3;
			      break;
			    case 15:
			      LO(bx)=2;
			      break;
			    case 18:
			      LO(bx)=4;
			      break;
			    case 0:
			      LO(bx)=dp->default_cmos;
			      dp->tracks=80;
			      dp->heads=2;
			      if (LO(bx) == 4)
				dp->sectors=18;
			      else dp->sectors=15;
			      d_printf("auto type defaulted to CMOS %d, sectors: %d\n", LO(bx), dp->sectors);
			      break;
			    default:
			      LO(bx)=4;
			      d_printf("type det. failed. num_tracks is: %d\n", dp->tracks);
			      break;
			    }

			  /* these numbers are "zero based" */
			  HI(dx) = dp->heads - 1; 
			  HI(cx) = (dp->tracks - 1) & 0xff;

			  LO(dx) = (disk < 0x80) ? fdisks : hdisks;
			  LO(cx) = dp->sectors | ((dp->tracks & 0x300) >> 2);
			  LO(ax) = 0;
			  /* show_regs(); */
			  _regs.eflags &= ~CF; /* no error */
			} else {
			  _regs.edx = 0; /* no hard disks */
			  _regs.ecx = 0;
			  LO(bx) = 0;
			  LO(ax) = 1; /* bad command */
			  _regs.eflags |= CF; /* error */
			}	
			break;
			
/* beginning of Adam's additions */
		case 0x9:	/* initialise drive from bpb */
			CARRY;
			break;
		case 0x0A:	/* We dont have access to ECC info */
		case 0x0B:
			CARRY;
			_regs.eax&=0xFF;
			_regs.eax|=0x0100;	/* unsupported opn. */
			break;
		case 0x0C:	/* explicit seek heads. - bit hard */
			CARRY;
			_regs.eax&=0xFF;
			_regs.eax|=0x0100;
			break;
		case 0x0D:	/* Drive reset (hd only) */
			NOCARRY;
			_regs.eax&=0xFF;
			break;
		case 0x0E:	/* XT only funcs */
		case 0x0F:
			CARRY;
			_regs.eax&=0xFF;
			_regs.eax|=0x0100;
			break;
		case 0x10:	/* Test drive is ok */
		case 0x11:	/* Recalibrate */
			disk=LO(dx);
			if(disk<0x80||disk>=0x80+hdisks)
			{
				_regs.eax&=0xFF;
				_regs.eax|=0x2000;	/* Controller didnt respond */
				CARRY;
				break;
			}
			_regs.eax&=0xFF;
			NOCARRY;
			break;
		case 0x12:	/* XT diagnostics */
		case 0x13:
			_regs.eax&=0xFF;
			CARRY;
			break;
		case 0x14:	/* AT diagnostics. Unix keeps the drive happy
					so report ok if it valid */
			_regs.eax&=0xFF;
			NOCARRY;
			break;
/* end of Adam's additions */


                case 0x15: /* Get type */
			d_printf("disk gettype %d\n", disk); 
			if (dp != NULL && disk >= 0x80) {
			  if (dp->removeable) {
			    HI(ax) = 1; /* floppy disk, no change detect */
			    d_printf("disk gettype: floppy\n");
			    _regs.edx = 0; 
			    _regs.ecx = 0; 
			  } else {
			    d_printf("disk gettype: hard disk\n");
			    HI(ax) = 3; /* fixed disk */
			    number = dp->tracks * dp->sectors * dp->heads;
			    _regs.ecx = number >> 16;
			    _regs.edx = number & 0xffff;
			  }
			  _regs.eflags &= ~CF; /* no error */
			} else {
			  if (dp != NULL)
			    {
			      d_printf("gettype on floppy %d\n", disk);
			      HI(ax) = 1;  /* floppy, no change detect=1 */
			      NOCARRY;
			    }
			  else
			    {
			      error("ERROR: gettype: no disk %d\n", disk);
			      HI(ax) = 0; /* disk not there */
			      _regs.eflags |= CF; /* error */
			    }
			}
			break;

/* beg of Adam's 1st mods */
		case 0x16: 
			/* get disk change status - hard - by claiming
			our disks dont have a changed line we are kind of ok */
			warn("int13: CHECK DISKCHANGE LINE\n");
			disk=LO(dx);
			if(disk<0||disk>=fdisks)
			{
			  d_printf("int13: DISK CHANGED\n");
			  CARRY;
			  /* _regs.eax&=0xFF;
			     _regs.eax|=0x200; */
			  HI(ax)=1;  /* change occurred */
			}
			else {
			  NOCARRY;
			  _regs.eax &= ~0xFF00;  /* clear AH */
			  d_printf("int13: NO CHANGE\n");
			}
			break;
		case 0x17:
			/* set disk type: should do all the ioctls etc
			   but I'm not feeling that brave yet */
			/* al=type dl=drive */
			CARRY;
			break;
/* end of Adam's 2nd mods */

		case 0x18: /* Set media type for format */
			track = HI(cx) + ((LO(cx) & 0xc0) << 2);
			sect = LO(cx) & 0x3f;
			d_printf("disk: set media type %x, %d sectors, %d tracks\n", disk, sect, track);
			HI(ax) = 1; /* function not avilable */
			break;
		case 0x20: /* ??? */
			d_printf("weird int13, ah=0x%x\n", _regs.eax);
			break;
	        case 0x28: /* DRDOS 6.0 call ??? */
			d_printf("int 13h, ax=%04x...DRDOS call\n",_regs.eax);
			break;
		case 0x5:  /* format */
			NOCARRY;  /* successful */
			HI(ax)=0; /* no error */
			break;
		default:
			error("ERROR: disk IO error: int13, ax=0x%x\n",
				    _regs.eax);
			show_regs();
			error = 5;
			return;
	}
}

void int14(void)
{
	int num;

	switch(HI(ax)) {
		case 0: /* init */
			_regs.eax = 0;
			num = _regs.edx & 0xffff;
			uarts[LO(dx)].fd=0;		/* no files yet */
			uarts[LO(dx)].IIR = 1;  /* no FIFO, no interrupt */
			uarts[LO(dx)].IER = 3;  /* int on data,  THRE */
			uarts[LO(dx)].LSR = 0x60;  /* TSHE&TSRE, no data*/
			uarts[LO(dx)].MSR = 0xb0;  /* DSR, CTS, CD */
			s_printf("init serial %d\n", num);
			break;
 	        case 1: /* write char */
			/* dbug_printf("send serial char '%c' com%d (0x%x)\n",
			   LO(ax), LO(dx)+1, LO(ax)); */
			if (LO(dx) > 3) LO(dx)=3;
			HI(ax) = serial_write(LO(dx), LO(ax));
			break;
 	        case 2: /* read char */
			if (LO(dx) > 3) LO(dx)=3;
			_regs.eax = serial_read(LO(dx));
			if (HI(ax) & 1)   /* data ready */
			  {
			    /* dbug_printf("recv serial char com%d '%c' (0x%x)\n",
				   LO(dx)+1, LO(ax), LO(ax)); */
			    HI(ax) &= 0x7f;   /* clear timeout bit */
			  }
			else
			  {
			    HI(ax) |= 0x80;   /* set timeout bit */
			    s_printf("failed serial read !!\n");
			  }
			break;
 	        case 3: /* port status */

			if (LO(dx) > 3) LO(dx) = 3;
			HI(ax) = 0x60;
			LO(ax) = 0xb0;   /* CD, DSR, CTS */
			if ( d_ready(uarts[LO(dx)].fd) )
			  {
			    HI(ax) |= 1; /* receiver data ready */
			    /* dbug_printf("BIOS serial port status com%d = 0x%x\n",
				   LO(dx), _regs.eax); */
			  }
			break;
	        case 4: /* extended initialize */
			s_printf("extended serial initialize\n");
			return;
		default:
			error("ERROR: serial error\n");
			show_regs();
			error = 5;
			return;
	}
}

void int15(void)
{
  static int extcount=0;
  struct timeval wait_time;
  int num;
  NOCARRY;

#define MAX_EXT_COUNT 10

  switch(HI(ax)) {
  case 0x41: /* wait on external event */
    extcount++;
    if (extcount >= MAX_EXT_COUNT) return;
    if (d.general)
      {
	g_printf("wait on external event\n");
	show_regs();
      }
    break;
  case 0x4f:   /* Keyboard intercept */
    HI(ax) = 0x86;
    NOCARRY;  /* original scancode (no translation) */
    break;
  case 0x80:	/* default BIOS device open event */
    _regs.eax&=0x00FF;
    return;
  case 0x81:
    _regs.eax&=0x00FF;
    return;
  case 0x82:
    _regs.eax&=0x00FF;
    return;
  case 0x83:
    h_printf("int 15h event wait:\n");
    show_regs();
    CARRY;
    return;	/* no event wait */
  case 0x84:
    CARRY;
    return;	/* no joystick */
  case 0x85:
    num=_regs.eax&0xFF;	/* default bios handler for sysreq key */
    if(num==0||num==1)
      {
	_regs.eax&=0x00FF;
	return;
      }
    _regs.eax&=0xFF00;
    _regs.eax|=1;
    CARRY;
    return;
  case 0x86:
    /* wait...cx:dx=time in usecs */
    g_printf("doing int15 wait...ah=0x86\n");
    show_regs();
    wait_time.tv_sec = 0;
    wait_time.tv_usec = ((_regs.ecx&0xffff) << 16) | (_regs.edx&0xffff);
    select(STDIN_FILENO, NULL, NULL, NULL, &scr_tv);
    NOCARRY;
    return;

  case 0x87:
#ifdef XMS
    if (xms) xms_int15();
    else
#endif
      {
	_regs.eax&=0xFF;
	_regs.eax|=0x0300;	/* say A20 gate failed - a lie but enough */
	CARRY;
      }
    return;

  case 0x88:
#ifdef XMS
    if (xms) xms_int15();
    else
#endif
    {
      _regs.eax &= ~0xffff;   /* we don't have extended ram if it's not XMS */
      NOCARRY;
    }
    return;

  case 0x89:			/* enter protected mode : kind of tricky! */
    _regs.eax|=0xFF00;	/* failed */
    CARRY;
    return;
  case 0x90:			/* no device post/wait stuff */
    CARRY;
    return;
  case 0x91:
    CARRY;
    return;
  case 0xc0:
    g_printf("incomplete EXT. BIOS DATA AREA requested...\n");
    _regs.es=0xd000;
    _regs.ebx=0x0000;	/* bios data area - see emulate.. */
    return;
  case 0xc1:
    CARRY;
    return;			/* no ebios area */
  case 0xc2:
    _regs.eax&=0x00FF;
    _regs.eax|=0x0300;	/* interface error if use a ps2 mouse */
    CARRY;
    return;
  case 0xc3:
    /* no watchdog */
    CARRY;
    return;
  case 0xc4:
    /* no post */
    CARRY;
    return;
  default:
    g_printf("int 15h error: ax=0x%04x\n", _regs.eax&0xffff);
    CARRY;
    return;
  }
}

void int16(void)
{
	unsigned int key;
	fd_set fds;

	switch(HI(ax)) {
		case 0: /* read key code, wait */
	                if (special_nowait)
			  {
			    /* k_printf("doing special_nowait\n"); */
			    if (ReadKeyboard(&key, NOWAIT) == 1)
			      {
				_regs.eax=key;
				_regs.eflags &= ~(ZF|CF);
			      } else {
				_regs.eax=0;
				_regs.eflags |= ZF | CF;
			      }
			    return;
			  }
			for (;;) {
				if (ReadKeyboard(&key, WAIT)) break;
			}
			if ((key & 0xff) == 3) k_printf("CTRL-C!\n");
			_regs.eax = key;
			break;
		case 1: /* test key code */
			if (ReadKeyboard(&key, TEST)) {
				_regs.eflags &= ~(ZF | CF); /* key pressed */
				_regs.eax = key;
			} else {
				_regs.eflags |= ZF | CF; /* no key */
				_regs.eax = 0;
			}
			break;
		case 2: /* read key state */
			if (console_keyb) LO(ax)=kbd_flags & 0xff;
			else _regs.eax &= ~0xff; 
			break;
 		case 5:	/* insert key code */
 			k_printf("ins ext key 0x%04x\n", _regs.eax & 0xffff);
 			_regs.eax &= ~0xff;
 			_regs.eax |= ((InsKeyboard((unsigned short)
 						  _regs.ecx & 0xffff)) 
 				      ? 0 : 1);
 			break;
		case 0x10: /* read extended key code, wait */
#ifdef OLD_EXTKEYREAD
			for (;;) {
				if (ReadKeyboard(&key, WAIT)) break;
			}
			_regs.eax = key;
#else
	                if (special_nowait)
			  {
			    /* k_printf("doing special_nowait\n"); */
			    if (ReadKeyboard(&key, NOWAIT) == 1)
			      {
				_regs.eax=key;
				_regs.eflags &= ~(ZF|CF);
			      } else {
				_regs.eax=0;
				_regs.eflags |= ZF | CF;
			      }
			    return;
			  }
			for (;;) {
				if (ReadKeyboard(&key, WAIT)) break;
			}
			if ((key & 0xff) == 3) k_printf("CTRL-C!\n");
			_regs.eax = key;
#endif
			break;

		case 0x11: /* check for enhanced keystroke */
			if (ReadKeyboard(&key, TEST)) {
				_regs.eflags &= ~(ZF | CF); /* key pressed */
				_regs.eax = key;
			} else {
				_regs.eflags |= ZF | CF; /* no key */
				_regs.eax = 0;
			}
			break;
		case 0x12: /* get extended shift states */
			if (console_keyb) _regs.eax=kbd_flags;
			else _regs.eax &= ~0xff; 
			return;
	        case 0x55: /* MS word coop w/TSR? */
			special_nowait ^= 1; 
			if (d.keyb)
			  {
			    dbug_printf("weird int16 ax=%04x call: %d!\n",
				     _regs.eax,special_nowait);
			    show_regs();
			    show_ints(0x16);
			  }
			_regs.eax=0x4d53;  /* say keyboard already handled */
			return;
		default:
			error("keyboard error (int 16h, ah=0x%x)\n", HI(ax));
			show_regs();
			/* error = 7; */
			return;
	}
}

void int17(void)
{
	int num;


	switch(HI(ax)) {
	        case 0: /* write char */
	                p_printf("print character on lpt%d : %c (%d)\n",
			       LO(dx), LO(ax), LO(ax)); 
			HI(ax) = printer_print(LO(dx), LO(ax));
			/* HI(ax) = 0xd0;  not busy, ACK, selected */
			break;
		case 1: /* init */
			HI(ax) = 0;
			num = _regs.edx & 0xffff;
			p_printf("init printer %d\n", num);
			break;
		case 2: /* get status */
			HI(ax) = 0xd0;    /* not busy, ack, selected */
			/* dbug_printf("printer 0x%x status: 0x%x\n", LO(dx), HI(ax)); */
			break;
		default:
			error("printer error: int 17h, ax=0x%x\n", _regs.eax);
			show_regs();
			error = 8;
			return;
	}
}

void int1a(void)
{
	int num;
	unsigned long ticks;
	long akt_time, elapsed;
	struct timeval tp;
	struct timezone tzp;
	struct tm *tm;


	switch(HI(ax)) {
		case 0: /* read time counter */
			time(&akt_time);
			elapsed = akt_time - start_time;
			ticks = (elapsed *182) / 10 + last_ticks;
			_regs.eax &= ~0xff; /* 0 hours */
			_regs.ecx = ticks >> 16;
			_regs.edx = ticks & 0xffff;
			/* dbug_printf("read timer st: %ud %ud t=%d\n",
				    start_time, ticks, akt_time); */
			break;
		case 1: /* write time counter */
			last_ticks = (_regs.ecx << 16) | (_regs.edx & 0xffff);
			set_ticks(last_ticks);
			time(&start_time);
			g_printf("set timer to %ud \n", last_ticks);
			break;
		case 2: /* get time */
			gettimeofday(&tp, &tzp);
			ticks = tp.tv_sec - (tzp.tz_minuteswest*60);
			tm = localtime((time_t *)&ticks);
        		/* g_printf("get time %d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec); */
			HI(cx) = tm->tm_hour % 10;
			tm->tm_hour /= 10;
			HI(cx) |= tm->tm_hour << 4;
			LO(cx) = tm->tm_min % 10;
			tm->tm_min /= 10;
			LO(cx) |= tm->tm_min << 4;
			HI(dx) = tm->tm_sec % 10;
			tm->tm_sec /= 10;
			HI(dx) |= tm->tm_sec << 4;
			/* LO(dx) = tm->tm_isdst; */
			_regs.eflags &= ~CF;
			break;
		case 4: /* get date */
			gettimeofday(&tp, &tzp);
			ticks = tp.tv_sec - (tzp.tz_minuteswest*60);
			tm = localtime((time_t *)&ticks);
			tm->tm_year += 1900;
			tm->tm_mon ++;
        		/* g_printf("get date %d.%d.%d\n", tm->tm_mday, tm->tm_mon, tm->tm_year); */
			_regs.ecx = tm->tm_year % 10;
			tm->tm_year /= 10;
			_regs.ecx |= (tm->tm_year % 10) << 4;
			tm->tm_year /= 10;
			_regs.ecx |= (tm->tm_year % 10) << 8;
			tm->tm_year /= 10;
			_regs.ecx |= (tm->tm_year) << 12;
			LO(dx) = tm->tm_mday % 10;
			tm->tm_mday /= 10;
			LO(dx) |= tm->tm_mday << 4;
			HI(dx) = tm->tm_mon % 10;
			tm->tm_mon /= 10;
			HI(dx) |= tm->tm_mon << 4;
			_regs.eflags &= ~CF;
			break;
		case 3: /* set time */
		case 5: /* set date */
			error("ERROR: timer: can't set time/date\n");
			break;
		default:
			error("ERROR: timer error AX=0x%04x\n", _regs.eax);
			/* show_regs(); */
			/* error = 9; */
			return;
	}
}

/* note that the emulation herein may cause problems with programs
 * that like to take control of certain int 21h functions, or that
 * change functions that the true int 21h functions use.  An example
 * of the latter is ANSI.SYS, which changes int 10h, and int 21h
 * uses int 10h.  for the moment, ANSI.SYS won't work anyway, so it's
 * no problem.
 */
int ms_dos(int nr) /* returns 1 if emulated, 0 if internal handling */
{

  char *csp, *p; 
  unsigned int c;
  us *ssp;
  static int nextcode=0;   

/* int 21, ah=1,7,or 8:  return 0 for an extended keystroke, but the
   next call returns the scancode */
/* if I press and hold a key like page down, it causes the emulator to
   exit! */

Restart: 
	/* dbug_printf("DOSINT 0x%x\n", nr); */
	/* emulate keyboard io to avoid DOS' busy wait */
	switch(nr) {

/* define NO_FIX_STDOUT if you want faster dos screen output, at the 
 * expense of being able to redirect things.  I don't seriously consider
 * leaving this in for real, but I just don't feel like taking it out
 * yet...I'd forget it if I did.
 */
#ifdef NO_FIX_STDOUT
	case 2:		/* char out */
	  v_printf("dos_c_out: %d %c\n", LO(dx), LO(dx));
	  char_out(LO(dx), screen);
	  NOCARRY;
	  break;
#endif
	case 7: /* read char, do not check <ctrl> C */
	case 1: /* read and echo char */
	case 8: /* read char */
	  /* k_printf("KBD/DOS: int 21h ah=%d\n", nr); */
	  disk_close();   /* DISK */
	  if (nextcode)
	    {
	      _regs.eax = nextcode;
	      nextcode = 0;
	    }
	  else
	    {
	      while (ReadKeyboard(&c, WAIT) != 1);
	      _regs.eax = c & 0xff;  /* mask out scan code */
	      if (_regs.eax == 0)    /* extended scan code */
		nextcode = c >> 8; /* get scan code */
	    }
	  if ((nr == 1) && (_regs.eax != 0)) 
	    char_out(_regs.eax, screen);
	  /* ctrl-C checking */
	  if ((_regs.eax == 3) && (nr != 7))
	    dos_ctrlc();
	  NOCARRY;
	  break;
#ifdef NO_FIX_STDOUT  /* dir /w doesn't care about this one, anyway */
	case 9: /* str out */
	  csp = SEG_ADR((char *), ds, dx);
	  for (p = csp; *p != '$';) char_out(*p++, screen);
	  NOCARRY;
	  break;
#endif
	case 10: /* read string */
	  disk_close(); /* DISK */
	  csp = SEG_ADR((char *), ds, dx);
	  ReadString(((unsigned char *)csp)[0], csp +1);
	  NOCARRY;
	  break;
	case 12: /* clear key buffer, do int AL */
	  while (ReadKeyboard(&c, NOWAIT) == 1);
	  nr = LO(ax);
	  if (nr == 0) break; /* thanx to R Michael McMahon for the hint */
	  HI(ax) = LO(ax);
	  NOCARRY;
	  goto Restart;
	case 0xfa: /* unused by DOS, for LINUX.EXE */
	  if ((_regs.ebx & 0xffff) == 0x1234) { /* MAGIC */
	    _regs.eax = ext_fs(LO(ax), SEG_ADR((char *), fs, di), 
			       SEG_ADR((char *), gs, si), _regs.ecx & 0xffff); 
	    d_printf("RESULT %d\n", _regs.eax);
	    break;
	  } else
	    return 0;
	  
	case 0xfe: /* dos helper :-) */
	  /* this is a hodgepodge of functions implemented
	   * in the emulator to help emulator-specific
	   * programs interact with the user/debugger (me!)
	   */
	  g_printf("DOS HELPER CALLED: 0x%x!\n", LO(ax));
	  switch(LO(ax))
	    {
	    case 1:	/* SHOW_REGS */
	      show_regs();
	      break;
	    case 2:	/* SHOW IRQS */
	      show_ints(LO(bx));
	      break;
	    case 3:	/* SET IOPERMS: bx=start, ch=flag,
			   cl=number */
	      dbug_printf("I/O perms: 0x%x 0x%x %d\n",
		    _regs.ebx, LO(cx), HI(cx));
	      if (ioperm(_regs.ebx, LO(cx), HI(cx) & 1))
		{
		  error("ERROR: SET_IOPERMS request failed!!\n");
		  CARRY;
		}
	      else
		{
		  if (HI(cx) &1) warn("WARNING! DOS can now access I/O ports 0x%x to 0x%x\n", _regs.ebx, _regs.ebx + LO(cx) - 1);
		  else dbug_printf("Access to ports 0x%x to 0x%x clear\n",
				   _regs.ebx,_regs.ebx + LO(cx) - 1);
		  NOCARRY;   /* success */
		}
	      break;
	    case 4:  /* initialize video card */
	      if (LO(bx) == 0)
		{
		  warn("shutting off iopl()\n");
		  if (ioperm(0x3b0, 0x3db-0x3b0, 0))
		    warn("couldn't shut off ioperms\n");
		  /* iopl(0); */
		}
	      else
		{
		  if (ioperm(0x3b0,0x3db-0x3b0, 1)) 
		    warn("couldn't get range!\n");
		  /* iopl(3); */
		  warn("WARNING: iopl set to 3!!\njumping to 0c000:0003\n");
		  ssp = SEG_ADR((us *), ss, sp);
/*		  *--ssp = _regs.eflags; */
		  *--ssp = _regs.cs;
		  *--ssp = _regs.eip;
		  precard_eip=_regs.eip;
		  precard_cs=_regs.cs;
		  _regs.esp -= 4;
		  _regs.cs = 0xc000;
		  _regs.eip = 3;
		  show_regs();
		  card_init=1;
		}
	    }
	  return 0;

	case 0xff:
	  if ((_regs.eax & 0xffff) == 0xffff)
	    {
	      /* show_ints(0xf); */
	      dbug_printf("DOS termination requested\n");
	      p_dos_str("\n\rLeaving DOS...\n\r");
	      leavedos(0);
	    }
	  break;
	  
	default:
	  if (d.dos)
	    dbug_printf(" dos interrupt 0x%x, ax=0x%x, bx=0x%x\n",
			nr, _regs.eax, _regs.ebx); 
	  return 0;
	}
	return 1;
}
	

int int28(void)		 /* keyboard busy loop */
{
	fd_set fds;

	/* this DELAY is a bad idea, as is the select() in it.
         * it just slows things down.
         */

	k_printf("called int28()\n");
	return 1;  /* this tells do_int to return */
}

run_int(int i)
{
  us *ssp;
  
  if ((_regs.eip == 0) && (_regs.cs == 0))
    {
      error("run_int: not running NULL interrupt 0x%x handler\n",i);
      show_regs();
      return;
    }
  ssp = SEG_ADR((us *), ss, sp);
  *--ssp = _regs.eflags;
  *--ssp = _regs.cs;
  *--ssp = _regs.eip;
  _regs.esp -= 6;
  _regs.cs =  ((us *)0)[ (i<<1) +1];
  _regs.eip = ((us *)0)[  i<<1    ];

  _regs.eflags &= 0xfffffcff;
}

is_iret(int i)
{
  /* return 1 if points to iret */
  unsigned char *byte=(unsigned char *)(ISEG(i)*16 + IOFF(i));

  if (*byte == 0xcf) 
    return 1;
  else 
    return 0;
}

can_revector (int i)
{
/* here's sort of a guideline:
 * if we emulate it completely, but there is a good reason to stick
 * something in front of it, and it seems to work, by all means revector it.
 * if we emulate none of it, say yes, as that is a little bit faster.
 * if we emulate it, but things don't seem to work when it's revectored,
 * (like int15h and LINUX.EXE), then don't let it be revectored.
 *
 */

  switch(i)
    {
      /* some things, like 0x29, need to be unrevectored if the vectors
       * are the DOS vectors...but revectored otherwise
       */
    case 0x21: /* we want it first...then we'll pass it on */
    case 0x2a: 
    case 0x15: /* need this for LINUX.EXE...why??  */
    case 0x29: /* DOS fast char output... */
#ifdef XMS
    case 0x2f:
#endif
      return 0;


    case 0x28: /* keyboard busy loop */
               /* keep int28h revectorable so that things go fast */
    case 0: 
    case 1:
    case 2:
    case 3:
    case 4:
    case 0x25: /* absolute disk read, calls int 13h */
    case 0x26: /* absolute disk write, calls int 13h */
    case 0x1b: /* ctrl-break handler */
    case 0x1c: /* user timer tick */
    case 0x17: /* BIOS printer */
    case 0x10: /* BIOS video */
    case 0x16: /* BIOS keyboard */
    case 0x13: /* BIOS disk */
    case 0x27: /* TSR */
#ifndef XMS
    case 0x2f: /* multiplex */
#endif
    case 0x20: /* exit program */
    case 0x33: /* mouse */
      /* if (i == 0x16) k_printf("SHOCK! revectoring int 0x%02x\n", i); */
      return 1;
    default:
      g_printf("revectoring 0x%02x\n", i);
      return 1;
    }
}


void do_int(int i)
{
  us *ssp;
  int highint=0;

  in_interrupt++;

  if ((_regs.cs&0xffff) == 0xe000)
    highint=1;
  else
    if ((ISEG(i) != 0xe000) && can_revector(i) /* && !is_iret(i) */)
      {
	run_int(i);
	return;
      }

  switch(i) {
      case 0x5   :
	g_printf("BOUNDS exception\n");
	goto default_handling;
      case 0x08  :
      case 0x0a  :
      case 0x0b  : /* com2 interrupt */
      case 0x0c  : /* com1 interrupt */
      case 0x0d  :
      case 0x0e  :
      case 0x0f  :
      case 0x09  : /* IRQ1, keyb data ready */
	g_printf("IRQ->interrupt %x\n", i);
	goto default_handling;
	return;
      case 0x10 : /* VIDEO */
	int10();
	return;
      case 0x11 : /* CONFIGURATION */
	_regs.eax = CONFIGURATION;
	if (fdisks > 0) _regs.eax |= 1 | ((fdisks -1)<<6);
	return;
      case 0x12 : /* MEMORY */
	_regs.eax = MEM_SIZE;
	g_printf("memory tested: %dK\n", MEM_SIZE);
	return;
      case 0x13 : /* DISK IO */
	int13();
	return;
      case 0x14 : /* COM IO */
	int14();
	return;
      case 0x15 : /* Cassette */
	int15();
	return;
      case 0x16 : /* KEYBOARD */
	int16();
	return;
      case 0x17 : /* PRINTER */
	int17();
	return;
      case 0x18 : /* BASIC */
	break;
      case 0x19 : /* LOAD SYSTEM */
	if (hdiskboot != 2)
	  boot(hdiskboot? hdisktab : disktab);
	else boot(&disktab[1]);
	return;
      case 0x1a : /* CLOCK */
	int1a();
	return;
#if 0
      case 0x1b : /* BREAK */
      case 0x1c : /* TIMER */
      case 0x1d : /* SCREEN INIT */
      case 0x1e : /* DEF DISK PARMS */
      case 0x1f : /* DEF GRAPHIC */
      case 0x20 : /* EXIT */
      case 0x27 : /* TSR */
#endif
      case 0x21 : /* MS-DOS */
	if (ms_dos(HI(ax))) return;
	/* else do default handling in vm86 mode */
	goto default_handling;
	
      case 0x28 : /* KEYBOARD BUSY LOOP */
	if (int28()) return;
	/* else do default handling in vm86 mode..no, don't */
	goto default_handling; 
	return;
	
      case 0x29 : /* FAST CONSOLE OUTPUT */
	char_out(*(char *)&_regs.eax, screen);    /* char in AL */
	return;

      case 0x2a : /* CRITICAL SECTION */
	goto default_handling;

      case 0x2f : /* Multiplex */
#ifdef XMS
	if ((HI(ax) == XMS_MAGIC) && xms) {
	  switch(LO(ax))
	    {
	    case 0:  /* check for XMS */
	      x_printf("Check for XMS\n");
	      LO(ax)=0x80;
	      break;
	    case 0x10:
	      x_printf("Get XMSControl address\n");
	      _regs.es = XMSControl_SEG;
	      _regs.ebx &= ~0xffff;
	      _regs.ebx |= XMSControl_OFF;
	      break;
	    default:
	      x_printf("BAD int 0x2f XMS function\n");
	    }
	  in_interrupt--;
	  return;
	}
	else 
#endif
        goto default_handling;

      default :
	if (d.defint)
	  dbug_printf("int 0x%x, ax=0x%x\n", i, _regs.eax);
	  /* fall through */

      default_handling:

	if (highint)
	  {
	    g_printf("default handling for highint: 0x%02x!\n", i);
	    in_interrupt--;
	    return;
	  }

	if (ISEG(i) == 0xe000) 
	  {
	    g_printf("DEFIVEC: int 0x%x  SG: 0x%04x  OF: 0x%04x\n",
			i, ISEG(i), IOFF(i));
	    in_interrupt--;
	    return;
	  }
 	if (is_iret(i))
	  {
	    if ((i != 0x2a) && (i != 0x28))
	      g_printf("just an iret 0x%02x\n", i);
	    in_interrupt--;
	    return; 
	  }
	ssp = SEG_ADR((us *), ss, sp);
	*--ssp = _regs.eflags;
	*--ssp = _regs.cs;
	*--ssp = _regs.eip;
	_regs.esp -= 6;
	_regs.cs =  ((us *)0)[ (i<<1) +1];
	_regs.eip = ((us *)0)[  i<<1    ];
	if ((_regs.eip == 0) && (_regs.cs == 0) && d.warning)
	  {
	    warn("NULL interrupt 0x%x handler\n",i);
	    show_regs();
	  }
	_regs.eflags &= 0xfffffcff;
	return;
  }
  error("\nERROR: int 0x%x not implemented\n", i);
  show_regs();
  error = 1;
  return;
}

void sigalrm(int sig)
{
    static int running,
             lastint=0;
    static inalrm=0;

    int didkbd=0;

    if (inalrm)
      error("ERROR: Reentering SIGALRM!\n");

    inalrm=1;
    in_sighandler=1;

    if ((vm86s.screen_bitmap & scrtest_bitmap) && !running) {
      running = 1;
      restore_screen();
      vm86s.screen_bitmap = 0;
      setitimer(ITIMER_REAL, &itv, NULL);
      running = 0;
    }

    if (console_keyb && poll_io && !in_readkeyboard) 
      {
	if (in_ioctl)
	  k_printf("not polling keyboard: in_ioctl: %d %04x %04x\n",
		   curi.fd, curi.req, curi.param3);
	else
	  {
	    /* do int9 if key was polled.
             * this is a hack.  keyups, for example, are iffy.
	     */
	    if ( (didkbd=PollKeyboard()) && keybint)
	      {
		k_printf("KBD: doing int 9 after PollKeyboard\n");
		if (_regs.eflags & IF) do_int(9);
		else k_printf("CAN'T DO INT 9...IF CLEAR\n");
	      }
	  }
      }

    /* update the Bios Data Area timer dword if interrupts enabled */
    if (_regs.eflags & IF) timer_tick();

    /* this is severely broken */
    if (timers && !didkbd)
      {
	h_printf("starting timer int 8...\n");
	if (_regs.eflags & IF) do_int(8);
	else h_printf("CAN'T DO TIMER INT 8...IF CLEAR\n");
      }
    in_sighandler=0;
    inalrm=0;
}

void sigsegv(int sig)
{
  unsigned long a;
  us *ssp;
  unsigned char *csp, *lina;
  static int haltcount=0;
  
#define MAX_HALT_COUNT 1
  
  if (ignore_segv)
    {
      g_printf("sigsegv ignored!\n");
      return;
    }
  
  /* In a properly functioning emulator :-), sigsegv's will never come
   * while in a non-reentrant system call (ioctl, select, etc).  Therefore,
   * there's really no reason to worry about them, so I say that I'm NOT
   * in a signal handler (I might make this a little clearer later, to
   * show that the purpose of in_sighandler is to stop non-reentrant system
   * calls from being reentered.
   * I reiterate: sigsegv's should only happen when I'm running the vm86
   * system call, so I really shouldn't be in a non-reentrant system call
   * (except maybe vm86)
   */
  in_sighandler=0;

  lina=(unsigned char *)((WORD(_regs.cs)<<4) + WORD(_regs.eip));

  if (_regs.eflags & TF) {
    g_printf("SIGSEGV %d received\n", sig);
    show_regs(); 
  }
  csp = SEG_ADR((unsigned char *), cs, ip);
  if (! (_regs.eflags & VM)) /* test VM bit */
    {
      error("ERROR: NON-VM86 general protection!\n");
    }

  switch (*csp) {
  case 0x62: /* bound */
    error("BOUND INSN");
    show_regs();
    _regs.eip += 4;  /* check this! */
    /* do_int(5); */
    break;
  case 0x6c: /* insb */
  case 0x6d: /* insw */
  case 0x6e: /* outsb */
  case 0x6f: /* outsw */
    error("ERROR: IN/OUT SB/SW: 0x%02x\n",*csp);
    _regs.eip++;
    break;
  case 0xcb: /* ret far */
    error("FAR RET SIGSEGV???\n");
    show_regs();
    ssp = SEG_ADR((us *), ss, sp);
    _regs.eip = *ssp++;
    _regs.cs = *ssp++;
    break;
  case 0xcd: /* int xx */
    _regs.eip += 2;
    do_int((int)*++csp);
    break;
  case 0xcc: /* int 3 */
    _regs.eip += 1;
    do_int(3);
    break;
  case 0xcf: /* iret */
    ssp = SEG_ADR((us *), ss, sp);
    _regs.eip = *ssp++;
    _regs.cs = *ssp++;
    _regs.eflags = (_regs.eflags & 0xffff0000) | *ssp++;
    _regs.esp += 6;
    in_interrupt--;
    break;
  case 0xe5: /* inw xx */
    _regs.eax &= ~0xff00;
    _regs.eax |= inb((int)csp[1] +1) << 8;
  case 0xe4: /* inb xx */
    _regs.eax &= ~0xff;
    _regs.eax |= inb((int)csp[1]);
    _regs.eip += 2;
    break;
  case 0xed: /* inw dx */
    _regs.eax &= ~0xff00;
    _regs.eax |= inb(_regs.edx +1) << 8;
  case 0xec: /* inb dx */
    _regs.eax &= ~0xff;
    _regs.eax |= inb(_regs.edx);
    _regs.eip += 1;
    break;
  case 0xe7: /* outw xx */
    outb((int)csp[1] +1, HI(ax));
  case 0xe6: /* outb xx */
    outb((int)csp[1], LO(ax));
    _regs.eip += 2;
    break;
  case 0xef: /* outw dx */
    outb(_regs.edx +1, HI(ax));
  case 0xee: /* outb dx */
    outb(_regs.edx, LO(ax));
    _regs.eip += 1;
    break;
  case 0xfa: /* cli */
    iflag = 0;
    _regs.eip += 1;
    break;
  case 0xfb: /* sti */
    iflag = 1;
    _regs.eip += 1;
    break;
  case 0x9c: /* pushf */
    if (iflag) _regs.eflags |= IF;
    else _regs.eflags &= ~IF;
    ssp = SEG_ADR((us *), ss, sp);
    *--ssp = (us)_regs.eflags;
    _regs.esp -= 2;
    _regs.eip += 1;
    break;
  case 0x9d: /* popf */
    ssp = SEG_ADR((us *), ss, sp);
    _regs.eflags &= ~0xffff;
    _regs.eflags |= (int)*ssp;
    _regs.esp += 2;
    _regs.eip += 1;
    break;
  case 0xf4: /* hlt...I use it for various things, 
		like trapping direct jumps into the XMS function */
    if ((_regs.cs & 0xffff) == 0xf000) /* jump into BIOS */
      {
	error("jump into BIOS...simulating IRET\n");
	/* simulate IRET */
	ssp = SEG_ADR((us *), ss, sp);
	_regs.eip = *ssp++;
	_regs.cs = *ssp++;
	_regs.eflags = (_regs.eflags & 0xffff0000) | *ssp++;
	_regs.esp += 6;	
	in_interrupt--;
	return;
    } else if (lina == (unsigned char *)0xffff0) {
        error("Jump to BIOS exit routine, flag: 0x%04x\n",
	      *(unsigned short int *)0x472);
	leavedos(1);
#ifdef XMS
    } else if (lina == (unsigned char *)XMSTrap_ADD) {
        _regs.eip+=2;  /* skip halt and info byte to point to FAR RET */
	xms_control();
	return;
#endif
    } else {
        error("HLT requested: lina=0x%06x!\n", lina);
        show_regs();
	haltcount++;
	if (haltcount > MAX_HALT_COUNT) error=0xf4;
	_regs.eip += 1;
      }
    break;
  case 0xf0: /* lock */
  default:
    /* er, why don't we advance eip here, and
       why does it work??  */
    error("general protection %x\n", *csp);
    show_regs();
    show_ints(0x30);
    if (card_init)
      {
        error("card_init gone bad...RETurning\n");
	card_init=0;
#if 0
	ssp = SEG_ADR((us *), ss, sp);
	_regs.eip = *ssp++;
	_regs.cs = *ssp++;
	_regs.esp += 4;
#endif
	_regs.eip=precard_eip;
	_regs.cs=precard_cs;

        /* the next two might need to be changed for RET vs. IRET! 
	   _regs.eflags = (_regs.eflags & 0xffff0000) | *ssp++;
	   _regs.esp += 6; */
	in_sighandler=0;
	return;
      }
	
    /* don't trap bad DOS 5 boot instructions */
    if (! boot_in_progress) 
      {
	error("Not boot in progress, bad instruction\n");
	error=4;
      }
    else {
      dbug_printf("Bad Insn %x in DOS boot\n", *csp);
      error = 0;
      _regs.eip += 1;
    }
    /* error = 4; */
  }	
  if (_regs.eflags & TF) {
    g_printf("emulation done");
    show_regs(); 
  }
  in_sighandler=0;
}

void sigquit(int sig)
{
  in_sighandler=1;

  warn("sigquit called\n");
  show_ints(0x30);
  show_regs();

  *(unsigned char *)0x471 = 0x80;  /* ctrl-break flag */
  do_int(0x1b);
  in_sighandler=0;
}

void timint(int sig)
{
  in_sighandler=1;

  warn("timint called: %04x:%04x -> %05x\n", ISEG(8), IOFF(8), IVEC(8));
  warn("(vec 0x1c)     %04x:%04x -> %05x\n", ISEG(0x1c), IOFF(0x1c),
       IVEC(0x1c));
  show_regs();

  do_int(0x8);

  in_sighandler=0;
}

void sigill(int sig)
{
  unsigned char *csp;
  int i, d;

  if (! (_regs.eflags & VM)) /* test VM bit */
    {
      error("ERROR: NON-VM86 illegal insn!\n");
    }
  error("SIGILL %d received\n", sig);
  show_regs();
  csp = SEG_ADR((unsigned char *), cs, ip);
  if (csp[0] == 0xf0)
    {
      dbug_printf("ERROR: LOCK prefix not permitted!\n");
      _regs.eip++;
      return;
    }
  i = (csp[0] << 8) + csp[1]; /* swapped */
  if ((i & 0xf800) != 0xd800) { /* no FPU insns */
    error("NO FPU instructions\n");
    error = 4;
    return;
  }
  switch(i & 0xc0) {
  case 0x00:
    if ((i & 0x7) == 0x6) {
      d = *(short *)(csp +2);
      _regs.eip += 4;
    } else {
      _regs.eip += 2;
      d = 0;
    }
    break;
  case 0x40:
    d = (signed)csp[2];
    _regs.eip += 3;
    break;
  case 0x80:
    d = *(short *)(csp +2);
    _regs.eip += 4;
    break;
  default:
    _regs.eip += 2;
    d = 0;
  }
  dbug_printf("math emulation %x d=%x\n", i, d);
}

void sigfpe(int sig)
{
  if (! (_regs.eflags & VM)) /* test VM bit */
    {
      error("ERROR: NON-VM86 SIGFPE insn!\n");
    }
	error("SIGFPE %d received\n", sig);
	show_regs();
	if (_regs.eflags & IF) do_int(0); 
        else error("ERROR: FPE and interrupts disabled\n");
}

/* this is for single stepping code */
void sigtrap(int sig)
{
  if (_regs.eflags & TF)  /* trap flag */
    _regs.eip++;
  
  do_int(3);
}

#define SETSIG(sig, fun)	sa.sa_handler = fun; \
				sa.sa_flags = 0; \
				sa.sa_mask = 0; \
				sigaction(sig, &sa, NULL);

open_terminal_pipe(char *path)
{
  terminal=fopen(path, "a+");
  if (!terminal)
    {
      terminal_pipe=0;
      error("ERROR: open_terminal_pipe failed - cannot open %s!\n", path);
      return;
    } 
  else terminal_pipe=1;

  fcntl(fileno(terminal), F_SETFL, 
	fcntl(fileno(terminal), F_GETFL,0) | O_NONBLOCK);
  
  setbuf(stdout, NULL);
}

/* this part is fairly flexible...you specify the debugging flags you wish
 * with -D string.  The string consists of the following characters:
 *   +   turns the following options on (initial state)
 *   -   turns the following options off
 *   a   turns all the options on/off, depending on whether +/- is set
 *   0   turns all options off
 *   1   turns all options on
 *   #   where # is a letter from the valid option list (see docs), turns
 *       that option off/on depending on the +/- state.
 *
 * Any option letter can occur in any place.  Even meaningless combinations,
 * such as "01-a-1+0vk" will be parsed without error, so be careful.
 * Some options are set by default, some are clear. This is subject to my 
 * whim.  You can ensure which are set by explicitly specifying.
 */

parse_debugflags(const char *s)
{
  char c;
  unsigned char flag=1;
  const char allopts[]="+vsdRWkpiwghx";

/* if you add new classes of debug messages, make sure to add the
 * letter to the allopts string above so that "1" and "a" can work
 * correctly.
 */

  dbug_printf("debug flags: %s\n", s);
  while (c=*s++)
    switch (c)
      {
      case '+':	/* begin options to turn on */
	flag=1;
	break;
      case '-':	/* begin options to turn off */
	flag=0;
	break;
      case 'v': /* video */
	d.video=flag;
	break;
      case 's': /* serial */
	d.serial=flag;
	break;
      case 'd': /* disk */
	d.disk=flag;
	break;
      case 'R': /* disk READ */
	d.read=flag;
	break;
      case 'W': /* disk WRITE */
	d.write=flag;
	break;
      case 'k': /* keyboard */
	d.keyb=flag;
	break;
      case 'p': /* printer */
	d.printer=flag;
	break;
      case 'i': /* i/o instructions (in/out) */
	d.io=flag;
	break;
      case 'w': /* warnings */
	d.warning=flag;
	break;
      case 'g': /* general messages */
	d.general=flag;
	break;
      case 'x': /* XMS */
	d.xms=flag;
	break;
      case 'a':	/* turn all on/off depending on flag */
	d.all=flag;
	if (flag) parse_debugflags("1");
	else parse_debugflags("0");
	break;
      case 'h': /* hardware */
	d.hardware=flag;
	break;
      case '0':	/* turn everything off */
	memset(&d, 0, sizeof(d));
	break;
      case '1':	/* turn everything on */
	flag=1;
	parse_debugflags(allopts);	/* point to a string w/every option */
	break;
      default:
	p_dos_str("Unknown debug-msg mask: %c\n\r", c);
	dbug_printf("Unknown debug-msg mask: %c\n", c);
      }
}

flip_disks(struct disk *d1, struct disk *d2)
{
  struct disk temp;

  temp=*d1;
  *d1=*d2;
  *d2=temp;
}

int emulate(int argc, char **argv)
{
	struct sigaction sa;
	int c;
	int open_mem=0;  /* flag for opening mem_fd */

	iq.queued=0;
	in_sighandler=0;
	sync(); /* for safety */
	setbuf(stdout, NULL);

	hdiskboot=1;   /* default hard disk boot */
	console_video=0;
	console_keyb=0;
	mapped_bios=0;
	keybint=0; 

	opterr=0;
	while ( (c=getopt(argc, argv, "ABCfckm:D:pP:bH:F:VNtsgxK")) != EOF) {
	  switch(c) {
	  case 'A':
	    hdiskboot=0;
	    break;
	  case 'B':
	    hdiskboot=2;
	    break;	    
	  case 'C':
	    hdiskboot=1;
	    break;
	  case 'f':  /* flip A: and B: floppies */
	    flip_disks(&disktab[0], &disktab[1]);
	    break;
	  case 'c':
	    console_video=1;
	    open_mem=1;
	    break;
	  case 'k':
	    console_keyb=1;
	    break;
	  case 'K':
	    warn("Keyboard interrupt enabled...this is still buggy!\n");
	    keybint=1;
	    break;
	  case 'm':
	    mem_size=atoi(optarg);
	    if (mem_size > MAX_MEM_SIZE)
	      mem_size = MAX_MEM_SIZE;
	    break;
	  case 'D':
	    parse_debugflags(optarg);
	    break;
	  case 'p':
	    poll_io=0;
	    warn("Warning: turning off RAW mode keyboard polling!\n");
	    break;
	  case 'P':
	    open_terminal_pipe(optarg);
	    break;
	  case 'b':
	    open_mem=1;
	    mapped_bios=1;
	    break;
	  case 'F':
	    fdisks=atoi(optarg);
	    g_printf("%d floppy disks specified\n", fdisks);
	    break;
	  case 'H':
	    hdisks=atoi(optarg);
	    g_printf("%d hard disks specified\n", hdisks);
	    break;
	  case 'V':
	    g_printf("Assuming VGA video card\n");
	    vga=1;
	    break;
	  case 'N':
	    warn("DOS will not be started\n");
	    exitearly=1;
	    break;
	  case 't':
	    g_printf("doing timer emulation\n");
	    timers=1;
	    d.hardware=1;  /* turn "hardware" debug-msgs on */
	    break;
	  case 's':
	    g_printf("using new scrn size code\n");
	    sizes=1;
	    break;
	  case 'g':
#ifdef EXPERIMENTAL_GFX
	    g_printf("turning graphics option on\n");
	    graphics=1;
#else
	    error("Graphics support not compiled in!\n");
#endif
	    break;
	  case 'x':
#ifdef XMS
	    x_printf("turning XMS option on\n");
	    xms=1;
#else
	    error("XMS support not compiled in!\n");
#endif
	    break;
	  case '?':
	    p_dos_str("unrecognized option: -%c\n\r", c);
	    usage();
	    exit(1);
	  }
	}
	dbug_printf("DEBUG FLAGS:\n");
	dbug_printf("disk: %d, keyboard: %d, video: %d, io: %d\n",
		    d.disk, d.keyb, d.video, d.io);
	dbug_printf("serial: %d, printer: %d, warning: %d , general: %d\n",
		    d.serial, d.printer, d.warning, d.general);
	if (open_mem) open_kmem();
	if (mapped_bios) map_bios();

	g_printf("EMULATE\n");

	/* init signal handlers */
	SETSIG(SIGSEGV, sigsegv);
	SETSIG(SIGILL, sigill);
	SETSIG(SIGALRM, sigalrm);
	SETSIG(SIGFPE, sigfpe);
	SETSIG(SIGTRAP, sigtrap);

	SETSIG(SIGHUP, leavedos);    /* for "graceful" shutdown */
	SETSIG(SIGTERM, leavedos);
	SETSIG(SIGKILL, leavedos);
	SETSIG(SIGQUIT, sigquit);
	SETSIG(SIGUNUSED, timint);

	/* do time stuff - necessary for initial time setting */
	{
	  struct timeval tp;
	  struct timezone tzp;
	  struct tm *tm;
	  unsigned long ticks;

	  time(&start_time);
	  gettimeofday(&tp, &tzp);
	  ticks = tp.tv_sec - (tzp.tz_minuteswest*60);
	  tm = localtime((time_t *)&ticks);
	  last_ticks=(tm->tm_hour*60*60 + tm->tm_min*60 + tm->tm_sec)*18.206;
	  set_ticks(last_ticks);
	};

	disk_init();
	termioInit();
	hardware_init();
	clear_screen(screen, 7);
	dbug_printf("$Header: /usr/src/dos/RCS/emu.c,v 1.18 1993/02/16 00:21:29 root Exp $\n");
	p_dos_str("Linux DOS emulator $Revision: 1.18 $  1993\n\r");
	p_dos_str("Mods by Robert Sanders, Alan Cox\n\r");
	if (hdiskboot != 2)
	  boot(hdiskboot? hdisktab : disktab); 
	else boot(&disktab[1]);
	/* boot(hdiskboot? hdisktab : disktab); */
	fflush(stdout);
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = UPDATE;
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = UPDATE;
	setitimer(ITIMER_REAL, &itv, NULL);
	if (!console_video)
	  vm86s.flags = VM86_SCREEN_BITMAP;
	else
	  vm86s.flags = 0;
	vm86s.screen_bitmap = 0;
	scrtest_bitmap = 1 << (24 + screen);

	for(;!error;) {
	  run_vm86();
	}
	error("error exit: %d 0x%x\n", error, error);

	leavedos(0);
}

/* "graceful" shutdown */
void leavedos(int sig)
{
  int loop;

  p_dos_str("\n\rDOS killed!\n\r");
  error("sig %d received - shutting down\n", sig);  
  for (loop=0; loop<3; loop++)
    {
      if (lpt[loop] != NULL)
	{
	  p_printf("Closing printer %d (%s)\n", loop, prtnames[loop]);
	  fclose(lpt[loop]);
	}
    }
  for (loop=0; loop<4; loop++)
    {
      if (uarts[loop].fd != 0)
	{
	  s_printf("Closing serial com%d\n", loop+1);
	  close(uarts[loop].fd);
	}
    }

  show_ints(0x30);
  fflush(stderr);
  fflush(stdout);

  termioClose();
  disk_close_all();
  _exit(0);
}

/*  this function opens the pseudo-printer file, if necessary, and
 *  writes a character to it.  no buffering, no nothing.  no status
 *  is returned, although the mechanism is there and screaming to
 *  be implemented.  however, it's 2 a.m., and I've got work tomorrow
 *  Robert
 */

int printer_print(int prnum, char outchar)
{
  if (lpt[prnum] == NULL) lpt[prnum] = fopen(prtnames[prnum], "a");
  fputc(outchar, lpt[prnum]);

  return(0xd0);   /* not busy, selected, ACK */
}



/*  this function opens the pseudo-serial out file, if necessary, and
 * this is purely for debugging! serial stuff is hardly unidirectional
 */

int serial_write(int comnum, char outchar)
{
  if (uarts[comnum].fd == 0)
    {
      uarts[comnum].fd=open(comnames[comnum], O_RDWR);
      s_printf("Serial port com%d opened out to %d\n", comnum+1, comnames[comnum]);
    }

  uarts[comnum].TX = outchar;
  uarts[comnum].MSR = 0xb0;     /* DCD, DSR, CTS for modem status */

  if (uarts[comnum].fd) write(uarts[comnum].fd, &outchar, 1);
  else s_printf("ERROR: can't write serial char\n");

  uarts[comnum].IIR = 2;   /* tr-reg-empty, int pending */

  return(0x60);   /* send ok, transmit empty, tr. hold empty, no error */
}

/* this functions returns a character from
 * the serial port */
int serial_read(int comnum)
{
   char tempchar; 
   int ready;

  if (uarts[comnum].fd == 0)
    {
      uarts[comnum].fd=open(comnames[comnum], O_RDWR);
      s_printf("Serial port com%d opened in to %s\n", comnum+1, comnames[comnum]);
    }
  uarts[comnum].MSR = 0xb0;     /* DCD, DSR, CTS for modem status */
  uarts[comnum].LSR = 0x60;
  
  if (uarts[comnum].fd && d_ready(uarts[comnum].fd)) {
       read(uarts[comnum].fd, &tempchar, 1);
       uarts[comnum].RX = tempchar;
       uarts[comnum].LSR |= 0x1;       /* receiver data ready */
       s_printf("chars ready at com%d\n", comnum+1);
     }
   else uarts[comnum].RX = 0;

  /* high byte is line stat, low is char */
  return((uarts[comnum].LSR << 8) | uarts[comnum].RX);
}

void hardware_init(void)
{
  int i;
  unsigned int ics, iip;

  /* do PIT init here */

  /* do UART init here - need to setup registers*/
  for (i = 0; i < 4; i++)
    uarts[i].fd = 0;

  uarts[0].base = 0x3f8;   /* COM1 */
  uarts[1].base = 0x2f8;   /* COM2, etc. */
  uarts[2].base = 0x3e8;
  uarts[3].base = 0x2e8;   

  /* LPT file init here */
  for (i = 0; i < 3; i++)
    lpt[i] = NULL;

  /* PIC init */
  for (i=0; i < 2; i++)
    {
      pics[i].OCW1 = 0;   /* no IRQ's serviced */
      pics[i].OCW2 = 0;   /* no EOI's received */
      pics[i].OCW3 = 8;   /* just marks this as OCW3 */
    }
  g_printf("Hardware (8259, 8253, 8250) initialized\n"); 

}

/* check the fd for data ready for reading */
int d_ready(int fd)
{
   struct timeval w_time;
   fd_set checkset;

   w_time.tv_sec=0;
   w_time.tv_usec=200000;

   FD_ZERO(&checkset);
   FD_SET(fd, &checkset);
   
   if (select(fd+1, &checkset, NULL, NULL, &w_time) == 1)
     {
       if (FD_ISSET(fd, &checkset)) return(1);
       else return(0);
     }
   else return(0);
 }

void usage(void)
{
   fprintf(stderr, "$Header: /usr/src/dos/RCS/emu.c,v 1.18 1993/02/16 00:21:29 root Exp $\n");
   fprintf(stderr,"usage: dos [-ABCfckbVtspgxK] [-D flags] [-m SIZE] [-P FILE] [-H|F #disks] > doserr\n");
   fprintf(stderr,"    -A boot from first defined floppy disk (A)\n");
   fprintf(stderr,"    -B boot from second defined floppy disk (B) (#)\n");
   fprintf(stderr,"    -C boot from first defined hard disk (C)\n");
   fprintf(stderr,"    -f flip definitions for A: and B: floppies\n");
   fprintf(stderr,"    -c use PC console video (kernel 0.99pl3+) (!%%)\n");
   fprintf(stderr,"    -k use PC console keyboard (kernel 0.99pl3+) (!)\n");
   fprintf(stderr,"    -D set debug-msg mask to flags (+-)(avkdRWspwgxhi01)\n");
   fprintf(stderr,"    -m set memory size to SIZE kilobytes (!)\n");
   fprintf(stderr,"    -P copy debugging output to FILE\n");
   fprintf(stderr,"    -b map BIOS into emulator RAM (%%)\n");
   fprintf(stderr,"    -H specify number of hard disks (1 or 2)\n");
   fprintf(stderr,"    -F specify number of floppy disks (1-4)\n");
   fprintf(stderr,"    -V use VGA specific video optimizations (!#%%)\n");
   fprintf(stderr,"    -N No boot of DOS\n");
   fprintf(stderr,"    -t try new timer code (COMPLETELY BROKEN)(#)\n");
   fprintf(stderr,"    -s try new screen size code (COMPLETELY BROKEN)(#)\n");
   fprintf(stderr,"    -p turn off the RAW keyboard mode polling (!)\n");
   fprintf(stderr,"    -g enable graphics modes (COMPLETELY BROKEN) (!%%#)\n");
   fprintf(stderr,"    -x enable XMS (#)\n");
   fprintf(stderr,"    -K Do int9 within PollKeyboard (!#)\n");
   fprintf(stderr,"\n     (!) means BE CAREFUL! READ THE DOCS FIRST!\n");
   fprintf(stderr,"     (%%) marks those options which require dos be run as root (i.e. suid)\n");
   fprintf(stderr,"     (#) marks options which do not fully work yet\n");
}

ifprintf(unsigned char flg,const char *fmt, ...)
{
  va_list args;
  char buf[1025];
  int i;
  
  if (! flg) return;

  va_start(args, fmt);
  i=vsprintf(buf, fmt, args);
  va_end(args);

  printf(buf);
  if (terminal_pipe) fprintf(terminal, buf);
}  

p_dos_str(char *fmt, ...)
{
  va_list args;
  char buf[1025], *s;
  int i;
  
  va_start(args, fmt);
  i=vsprintf(buf, fmt, args);
  va_end(args);

  s=buf;
  while (*s) char_out(*s++, screen);
}

#undef EMU_C
