/* dos emulator, Matthias Lautner */
#define EMU_C 1
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/02/05 02:54:13 $
 * $Source: /usr/src/dos/RCS/emu.c,v $
 * $Revision: 1.14 $
 * $State: Exp $
 *
 * $Log: emu.c,v $
 * Revision 1.14  1993/02/05  02:54:13  root
 * this is for 0.47.6
 *
 * Revision 1.13  1993/02/04  01:16:44  root
 * version 0.47.5
 *
 * Revision 1.12  1993/01/28  02:17:38  root
 * fixed get_leds(), CloseKeyboard(), added -V option to set the high attribute
 * bit to mean high intensity background (as opposed to blinking) on VGAs.
 * THIS IS THE DOSEMU47 DISTRIBUTION EMU.C
 *
 * Revision 1.11  1993/01/25  22:59:38  root
 * added the -H and -F options to choose # of hard/flpy disks.
 * LINUX.EXE won't work with more than 2 floppies defined, so I changed
 * default number of floppy disks to 2.
 *
 * Revision 1.10  1993/01/21  21:58:30  root
 * i don't remember.  it must have been important.
 *
 * Revision 1.9  1993/01/21  01:57:15  root
 * lots o' stuff. generalized the hide_cursor, show_cursor routines.
 * really fixed up the floppy disk I/O.  should be much better now, although
 * it's not nearly done.
 * took out some useless messages, put some in.
 * I STILL NEED TO FIND OUT WHY TIME GIVES THE WRONG ANSWER!!!!!!
 * added the -b flag to map in 64k of BIOS at 0xf0000 and 32k of video BIOS
 * at 0xc0000. this is mainly useful for ME.
 *
 * Revision 1.8  1993/01/19  15:28:54  root
 * i forget what all I did. fixed some non-console keyboard mode problems
 * (alt-numkeys, stuff like that).  fixed int 21h, ah=1,7,8 problem
 * with extended keystrokes. cleaned up restore_screen() a bit, but
 * you need a fairly complete termcap to use it.  it still exhibits
 * a bug with pcshell.
 *
 * Revision 1.7  1993/01/16  01:15:32  root
 * fixed various things, like the BDA base port address of the CRTC.
 * now telix "port checked" writes work, as does tv.com (partly).
 * fixed the int 10h, ah=09 problem (the func wasn't there :-).
 *
 * Revision 1.6  1993/01/15  01:02:35  root
 * implemented int10h, ah=1 (define cursor) for the console_video mode.
 * safer console_video mode, as it now unmaps the screen before switching
 * VC's.  -m switch for max DOS mem.
 *
 * Revision 1.5  1993/01/14  21:55:52  root
 * HURRAY! got the direct screen stuff working.  sorta.  it's real
 * unstable right now.  DON'T SWITCH CONSOLES WHILE IT's WORKING!
 * don't run it in anything but 80x25.  there's a bug in that you
 * have to switch away and back at the initial DOS prompt to get
 * it to work ???
 * put the dos options into getopt now.
 *
 * Revision 1.4  1993/01/14  19:28:27  root
 * some more fixes here and there, nothing special.  next version
 * will do mmap'ing of display, getopt() for options.  this version
 * uses the 0.99pl3 VT_PROCESS modes for console switching!
 *
 * Revision 1.3  1993/01/12  01:19:03  root
 * BIOS serial ports now work (had timeout bit reversed).
 * all devices will be redirected to /dev/modem (BAD!).
 *
 * Revision 1.2  1993/01/11  21:24:07  root
 * minor changes for making the BIOS serial ports.  still flaky.
 * they almost work, but kermit gets extra characters.
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

#define CONFIGURATION 0x002c;   /* without disk information */
/* #define CO	80
   #define LI	25 */

/* would use the info termio.c nicely got for us, but it works badly now */
#define CO	co2
#define LI	li2

#define SCREEN_ADR(s)	(us *)(0xb8000 + s * 0x1000)

/* these were 330000 and 250000 */
#define UPDATE	330000		/* waiting time in usec */
#define DELAY	250000		/* sleeping time in usec */
#define OUTBUFSIZE	3000

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
int mem_size=640;

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
#define INIT_SCREEN_MODE	2  /* 80x25 monochrome */ 
#else
#define INIT_SCREEN_MODE	3  /* 80x25 color */
#endif

void boot(struct disk *);

extern void map_bios(void);
extern int open_kmem();

void leavedos(int sig),
  usage(void),
  robert_irq(int sig),
  hardware_init(void),
  show_ints(int),
  do_int(int);		/* was protyped wrong */

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

/* Programmable Interval Timer, 8253/8254 */
/* ports 0x40 - 0x43 */
struct pit
{   
  unsigned int
    CNTR0,	/* 0x40, time of day clock (usu/ mode 3) */
    CNTR1,	/* 0x41, RAM refresh cntr (usu. mode 2) */
    CNTR2,	/* 0x42, cassette/spkr */
    MCR;	/* 0x43, mode control register */
  unsigned char
    s0,         /* states */
    s1,
    s2,
    sm;
} pit;

int port61;  /* the pseudo-8255 device on AT's */

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
int max_page = 3;		/* number of highest vid page - 1*/

struct ioctlq iq = {0,0,0,0};	/* one-entry queue :-( for ioctl's */
int in_sighandler=0;		/* so I know to not use non-reentrant
				 * syscalls like ioctl() :-( */
int in_ioctl=0;
struct ioctlq curi = {0,0,0,0};				 

/* this is DEBUGGING code! */
int timers=0;			/* no timer emulation */
int sizes=0;

int screen_mode=INIT_SCREEN_MODE;

struct debug_flags d = {0,0,0,0,1,0,1,1,0,0,0,1,1,1,0};

int poll_io=1;			/* polling io, default on */
int ignore_segv;		/* ignore sigsegv's */
int terminal_pipe;
FILE *terminal;

extern unsigned int kbd_flags,
  		    key_flags;
int in_interrupt=0;		/* for unimplemented interrupt code */


/************** Adam's changes ***********************/
#define CARRY	_regs.eflags|=CF;
#define NOCARRY _regs.eflags&=~CF;
/************** end of Adam's changes ****************/



unsigned char trans[] = /* LATIN CHAR SET */
{
/*	"\000\001\002\003\004\005\006\007\010\011\000\013\000\000\016\017"
	"\020\021\022\023\024\025\026\027\030\031\032\000\034\035\036\037"
	"\040\041\042\043\044\045\046\047\050\051\052\053\054\055\056\057"
	"\060\061\062\063\064\065\066\067\070\071\072\073\074\075\076\077"
	"\100\101\102\103\104\105\106\107\110\111\112\113\114\115\116\117"
	"\120\121\122\123\124\125\126\127\130\131\132\133\134\135\136\137"
	"\140\141\142\143\144\145\146\147\150\151\152\153\154\155\156\157"
	"\160\161\162\163\164\165\166\167\170\171\172\173\174\175\176\177"
	"\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217"
	"\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237"
	"\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257"
	"\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277"
	"\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317"
	"\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337"
	"\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357"
	"\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377" */

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
   * C++ would be nice for this...sigh.  Wish I knew C++ :-)
   */
  if (iq.queued) 
      do_queued_ioctl();
}

int outcbuf(int c)
{
  CHOUT(c);
  return 1;
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
  else v_printf("v_write deferred for console_video\n");
}

void char_out(unsigned char ch, int s)
{
	us *sadr, *p;

	if (s > max_page) return;

	if (console_video)
	  {
	    if (ch >= ' ') {
	      sadr = SCREEN_ADR(s);
	      sadr[ypos[s]*CO + xpos[s]++] = ch | (7 << 8);
	      /* if (s == screen) outc(trans[ch]); */
	    } else if (ch == '\r') {
	      xpos[s] = 0;
	      /* if (s == screen) write(2, &ch, 1); */
	    } else if (ch == '\n') {
	      ypos[s]++;
	      /* if (s == screen) write(2, &ch, 1); */
	    } else if (ch == '\010' && xpos[s] > 0) {
	      xpos[s]--;
	      /* if (s == screen) write(2, &ch, 1); */
	      /*	} else if (ch == '\t') {
			do {
			char_out(' ', s);
			} while (xpos[s] % 8 != 0); */
	    } else if (ch == '\010' && xpos[s] > 0) {
	      xpos[s]--;
	      /* if (s == screen) write(2, &ch, 1); */
	    }

	    poscur(xpos[s], ypos[s]);
	    /* sadr = SCREEN_ADR(s);
	       sadr[ypos[s]*CO + xpos[s]++] = ch | (7 << 8);
	       v_printf("char_out w/console video: %c\n", ch); */
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
	    /*	} else if (ch == '\t') {
		do {
		char_out(' ', s);
		} while (xpos[s] % 8 != 0); */
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
		tputs(tgoto(cm, 0, y), 1, outcbuf);
		for (x=0; x<CO; x++) {
			c = *(unsigned char *)p;
			if ((a = ((unsigned char *)p)[1]) != oa) {
#ifndef OLD_RESTORE
			  /* do fore/back-ground colors */      
			  if (!(a & 7) || (a & 0x70)) tputs(mr, 1, outcbuf);
			  else tputs(me, 1, outcbuf);

			  /* do high intensity */
			  if (a & 0x8) tputs(md, 1, outcbuf);
			  else if (oa & 0x8)
			    {
			      tputs(me, 1, outcbuf);
			      if (!(a & 7) || (a & 0x70))
				tputs(mr, 1, outcbuf);
			    }

			  /* do underline/blink */
			  if (a & 0x80) tputs(so, 1, outcbuf);
			  else if (oa & 0x80) tputs(se, 1, outcbuf);
			  
			  oa = a;   /* save old attr as current */
#else
				if ((a & 7) == 0) tputs(mr, 1, outcbuf);
				else tputs(me, 1, outcbuf);
				if ((a & 0x88)) tputs(md, 1, outcbuf);
				oa = a;
#endif
			}
			CHOUT(trans[c] ? trans[c] : '_');
			p++;
		}
	}
	tputs(me, 1, outcbuf);
	CHFLUSH;
	poscur(xpos[screen],ypos[screen]);
}


void disk_close(void) {
	struct disk * dp;

	for (dp = disktab; dp < &disktab[fdisks]; dp++) {
		if (dp->removeable && dp->fdesc >= 0) {
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
	static int cga_r=0;
	static int inb_flg=0, tmp=0;

	port &= 0xffff;

	/* graphic status - many programs will use this port to sync with
	 * the vert & horz retrace so as not to cause CGA snow */
	if ((port == 0x3da) || (port == 0x3ba))
	  return (cga_r ^= 1) ? 0xcf : 0xc6;

	else if ((port == 0x60) || (port == 0x64))
	  i_printf("keyboard inb [0x%x]\n", port);
	else
	  switch (port)
	    {
	    case 0x61:
	      i_printf("inb [0x61] =  0x%x (8255 chip)\n", port61);
	      /* if (inb_flg) { port61++; inb_flg=0; }
	      else inb_flg=1;
              return inb_flg ? (port61 - 1) : port61; */
	      return port61;
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
	    case 0x3f8:
	      s_printf("serial 1 check RX\n");
	      break;
	    case 0x3fA:
	      s_printf("serial 1 check int ID\n");
	      break;
	    case 0x3FD:
	      s_printf("serial 1 check line status\n");
	      break;
	    case 0x3fe:
	      s_printf("serial 1 check modem status\n");
	      break;
	    case 0x3ff:
	      s_printf("serial 1 scratch\n");
	      return uarts[1].SCR;
	      break;
	    case 0x40:
	      i_printf("inb [0x41] = 0x%x  1st timer inb\n",
			  pit.CNTR0 /*++*/ );
	      return pit.CNTR0;
	    case 0x41:
	      i_printf("inb [0x41] = 0x%x  2nd timer inb\n",
			  pit.CNTR1 /*++*/);
	      return pit.CNTR1;
	    case 0x42:
	      i_printf("inb [0x42] = 0x%x  3rd timer inb\n",
			  pit.CNTR2 /* +=100 */);
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
  clear_screen(screen,0);
  p_dos_str("Rebooting DOS.  Be careful...this is partially implemented\r\n");
  show_cursor();
  boot(hdiskboot? hdisktab : disktab);
}

void boot(struct disk *dp)
{
	char *buffer;
	unsigned int i;
	unsigned char *ptr;

	boot_in_progress=1;
	disk_close();
	disk_open(dp);
	buffer = (char *)0x7c00;
	memset(NULL, 0, 0x7c00); /* clear the first 32 k */
	memset((char *)0xff000, 0xF4, 0x1000); /* fill the last page w/HLT */
	/* init trapped interrupts if called via jump */
#if 0
	for (i=0; i<0x100; i++) {
		if ((i & 0xf8) == 0x60) continue; /* user interrupts */
		SETIVEC(i, 0xff01, 4*i);
	}
#else
	for (i=0; i<256; i++) {
		if ((i & 0xf8) == 0x60) continue; /* user interrupts */
		SETIVEC(i, 0xe000, 16*i);
		ptr=(unsigned char *)(0xe000*16 + 16*i);
		ptr[0]=0xcd;     /* 0xcd = INT */
		ptr[1]=i;        /* was i + 128 */
		ptr[2]=0xcf;     /* 0xcf = IRET */
	}
	/* show_ints(0x30); */
#endif
	SETIVEC(0x33,0,0);     /* zero mouse address */
	SETIVEC(0x1c,0xe000,2);  /* should be an IRET */

	*(us *)0x402 = 0x2f8;	/* com port addresses */
	*(us *)0x404 = 0x3f8;  
	*(us *)0x406 = 0x2e8;
	*(us *)0x408 = 0x3e8;
	*(us *)0x410 = 0x7cc8;
	*(us *)0x419 = 0xc849;	/* equip. present word */


	*(us *)0x413 = MEM_SIZE;	/* size of memory */
	*(char *)0x449 = screen_mode;	/* screen mode */
	*(us *)0x44a = CO;	/* chars per line */
	*(us *)0x44c = CO * LI * 2;  /* size of video regen area in bytes */
	*(us *)0x44e = 0;       /* offset of current page in buffer */
	*(unsigned char *)0x484 = LI-1;    /* lines on screen - 1 */
	*(us *)0x41a = 0x1e;	/* key buf start ofs */
	*(us *)0x41c = 0x1e;	/* key buf end ofs */
	/* key buf 0x41e .. 0x43d */

        /* dosemu0.4 missed this trick, and many programs that don't
           assume 0x3d4 were reading from port 6 (base 0 + 6) instead
           of 0x3da.  oh, well. live and learn */
	*(us *)0x463 = 0x3d4;   /* base port of CRTC - IMPORTANT! */

	*(long *)0x4a8 = 0;  /* pointer to video table */

	/* from Alan Cox's mods */
	/* we need somewhere for the bios equipment. Put it at ffe00 */
	*(char *)0xffe00=0x09;
	*(char *)0xffe01=0x00;	/* 9 byte table */
	*(char *)0xffe02=0xFC;  /* PC AT */
	*(char *)0xffe03=0x00;
	*(char *)0xffe04=0x04;	/* bios revision 4 */
	*(char *)0xffe05=0x20;	/* no mca no ebios no wat no keybint
				   rtc no slave 8259 no dma 3 */
	*(char *)0xffe06=0x00;
	*(char *)0xffe07=0x00;
	*(char *)0xffe08=0x00;
	*(char *)0xffe09=0x00;

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
  unsigned short int icsp1,iipp1, icsp2, iipp2;

  max |= 1;
#if 0
  for (i=0x0; i<=max; i+=2)  /* interrupt tbl. entries 0 - 0xf */
    {
      iipp1 = IOFF(i);
      iipp2 = IOFF(i+1);
      icsp1 = ISEG(i);
      icsp2 = ISEG(i+1);
      dbug_printf("%02x| %04x:%04x->%05x    ",i,icsp1, iipp1,
		  (icsp1 << 4) + iipp1 );
      dbug_printf("%02x| %04x:%04x->%05x\n",i+1,icsp2, iipp2,
		  (icsp2 << 4) + iipp2 ); 
    }
#endif
  for (i=0x0; i<=max; i+=2)  /* interrupt tbl. entries 0 - 0xf */
    {
      dbug_printf("%02x| %04x:%04x->%05x    ",i,ISEG(i), IOFF(i),
		  IVEC(i));
      dbug_printf("%02x| %04x:%04x->%05x\n",i+1,ISEG(i+1),IOFF(i+1),
		  IVEC(i+1));
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
	  max_page=3;
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
    
  case 0xff:
    break;
    
  case 0x9:
    i=0;	/* repeat count */
    s=HI(bx);
    sm=SCREEN_ADR(s);
    while(i<_regs.ecx)
      {
	sm[CO*ypos[s]+xpos[s]+i]=(LO(bx)<<8)|LO(ax);
	i++;
      }
    break;
#if 0
  case 0x9:
    v_printf("second int10h, ah=9 called\n");
#endif
  case 0xA: /* set chars at cursor pos */
    c = _regs.eax&0xFF;;
    s = HI(bx);
    if (s != 0) {
      error("ERROR: video error(0xA s!=0)\n");
      /*				show_regs();*/
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
    /* must_update=1; */
    break;
  case 0xe: /* print char */ 
     /* found the problem: int 29 was calling int 10h/ah=e, and guess what?
      * Matthias forgot to set HI(bx) to be the current screen. so I just
      * made int 29h stand-alone and use char_out()
      */
    s=HI(bx);
    if (s > max_page) 
      {
	v_printf("in int10h/ah=3, s=%d\n", s);
	s=screen;
      }
    /* else if (s != screen) v_printf("int 10h, ah=e with s=%d\n", s); */
    char_out(*(char *)&_regs.eax, s);    /* char in AL */
    break;
  case 0x0f: /* get screen mode */
    v_printf("get screen mode!\n"); 
    _regs.eax = (CO << 8) | screen_mode;
    _regs.ebx &= 0xff;  /* clear top byte (BH) */
    _regs.ebx |= screen << 8;
    break;
  case 0xb: /* palette */
  case 0xc: /* set dot */
  case 0xd: /* get dot */
    break;
  case 0x4: /* get light pen */
    error("ERROR: video error(no light pen)\n");
    CARRY;
    return;
  case 0x1a: /* get display combo */
    if (LO(ax) == 0)
      {
	v_printf("get display combo!\n");
	LO(ax) = 0x1a;  /* valid function */
	LO(bx) = 4;     /* active display = color EGA */
	HI(bx) = 0;     /* no inactive display */
      }
    else {
      v_printf("set display combo not supported\n");
    }
    break;
  case 0x12: /* video subsystem config */
    switch (LO(bx))
      {
      case 0x10:
	if (d.video)
	  {
	    dbug_printf("return video config\n");
	    show_regs();
	  }
#if 0
	_regs.ebx=1;   /* color 128k EGA */
#endif
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
  case 0xfe: /* get shadow buffer */
    v_printf("get shadow buffer\n");
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
			if (dp == NULL || head >= dp->heads || 
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
			if (dp == NULL || head >= dp->heads || 
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
			if (dp == NULL || head >= dp->heads || 
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
			      HI(ax) = 1;  /* floppy, no change detect */
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
			disk=LO(dx);
			if(disk<0||disk>=fdisks)
			{
				CARRY;
				_regs.eax&=0xFF;
				_regs.eax|=0x200;
				break;
			}
			NOCARRY;
			_regs.eax&=0xFF;
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
  
#ifdef OLD_INT_15
	int num;

	switch(HI(ax)) {
		case 0x41: /* wait on external event */
			dbug_printf("wait on external event\n");
			show_regs();
			break;
		case 0x87: /* block move */
			dbug_printf("block move not supported\n");
			_regs.eflags |= CF; /* not supported */
			break;
		case 0x88: /* get memory size */
			_regs.eax = 0;
			break;
		case 0xc0: /* get configuration */
			dbug_printf("get configuration not supported\n");
			_regs.eflags |= CF; /* not supported */
			break;
		case 0xc1: /* ext-BIOS data segment */
			_regs.eflags |= CF; /* not supported */
			break;
		default:
			dbug_printf(" cassette 0x%x ignoring\n", HI(ax));
			show_regs();
			break;
	}
#else
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
  case 0x80:	/* defaul BIOS device open event */
    _regs.eax&=0x00FF;
    return;
  case 0x81:
    _regs.eax&=0x00FF;
    return;
  case 0x82:
    _regs.eax&=0x00FF;
    return;
  case 0x83:
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
    /* cx:dx=time in usecs. Spose we should usleep here... */
    CARRY;
    return;
  case 0x87:
    _regs.eax&=0xFF;
    _regs.eax|=0x0300;	/* say A20 gate failed - a lie but enough */
    CARRY;
    return;
  case 0x88:
    _regs.eax=0;		/* we don't have extended ram */
    NOCARRY;
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
    _regs.es=0xFFE0;
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
    g_printf("int 15h error: ax=0x%04x\n", _regs.eax);
    CARRY;
    return;
  }
#endif
}

void int16(void)
{
	unsigned int key;
	fd_set fds;
	static int special_nowait=0;

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
#if 0
			if (!(vm86s.screen_bitmap & scrtest_bitmap)) {
				FD_ZERO(&fds);
				FD_SET(kbd_fd, &fds);
				scr_tv.tv_sec = 0;
				scr_tv.tv_usec = DELAY;
				select(kbd_fd+1, &fds, NULL, NULL, &scr_tv);
			}
#endif
			break;
 		case 5:	/* insert key code */
 			k_printf("ins ext key 0x04x\n", _regs.eax & 0xffff);
 			_regs.eax &= ~0xff;
 			_regs.eax |= ((InsKeyboard((unsigned short)
 						  _regs.ecx & 0xffff)) 
 				      ? 0 : 1);
 			break;
		case 0x10: /* read extended key code, wait */
			k_printf("get ext key\n"); 
			for (;;) {
				if (ReadKeyboard(&key, WAIT)) break;
			}
			_regs.eax = key;
			break;
		case 0x11: /* check for enhanced keystroke */
			if (ReadKeyboard(&key, 0))
			{
				InsKeyboard(key);
				_regs.eax = key;
				_regs.eflags &= ~ZF;
			}
			else
				_regs.eflags |= ZF;
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
	                /* dbug_printf("print character on lpt%d : %c (%d)\n",
			       LO(dx), LO(ax), LO(ax)); */
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
			time(&start_time);
			g_printf("set timer to %ud \n", last_ticks);
			break;
		case 2: /* get time */
			gettimeofday(&tp, &tzp);
			ticks = tp.tv_sec - (tzp.tz_minuteswest*60);
			tm = localtime((time_t *)&ticks);
        		g_printf("get time %d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
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
        		g_printf("get date %d.%d.%d\n", tm->tm_mday, tm->tm_mon, tm->tm_year);
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
			error("ERROR: timer error\n");
			show_regs();
			error = 9;
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
	case 2:		/* char out */
	  char_out(LO(dx), screen);
	  NOCARRY;
	  break;
	case 1: /* read and echo char */
	case 8: /* read char */
	case 7: /* read char, do not check <ctrl> C */
	  disk_close();
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
	  if ((nr == 1) && (_regs.eax != 0)) char_out(c, screen);
	  NOCARRY;
	  break;
	case 9: /* str out */
	  csp = SEG_ADR((char *), ds, dx);
	  for (p = csp; *p != '$';) char_out(*p++, screen);
	  NOCARRY;
	  break;
	case 10: /* read string */
	  disk_close();
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

	if (!(vm86s.screen_bitmap & scrtest_bitmap)) {
		FD_ZERO(&fds);
		FD_SET(kbd_fd, &fds);
		scr_tv.tv_sec = 0;
		scr_tv.tv_usec = DELAY;
		select(kbd_fd+1, &fds, NULL, NULL, &scr_tv);
	}
	return 0;
}

run_int(int i)
{
  us *ssp;
  
  ssp = SEG_ADR((us *), ss, sp);
  *--ssp = _regs.eflags;
  *--ssp = _regs.cs;
  *--ssp = _regs.eip;
  _regs.esp -= 6;
  _regs.cs =  ((us *)0)[ (i<<1) +1];
  _regs.eip = ((us *)0)[  i<<1    ];
  if ((_regs.eip == 0) && (_regs.cs == 0))
    {
      warn("run_int: NULL interrupt 0x%x handler\n",i);
      show_regs();
    }
  /* else
    dbug_printf("run_int 0x%02x: SG: 0x%04x   OF: 0x%04x\n",
		i, _regs.cs, _regs.eip); */

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
 * if it's something we deliberately steal from DOS, like int 28h, then
 * don't revector it (as that would miss the point: we don't want DOS to
 * ever get run with int 28h, as it just polls.)
 */

  switch(i)
    {
/* these, the old vectors, are MUCH faster in TP 5.5 w/pgup pgdn.
 * I wonder why?  The difference is that 0x28 must be revectorable
 * for this speed. it points to an IRET in the standard DOS setup...
 */
#ifdef OLD_REVECS
    case 0x1b:
    case 0x1c:
    case 0x20:
    case 0x21: /* we want it first...then we'll pass it on */
    case 0x29:
    case 0x2f:
    case 0x2a:
    case 0x15:  /* need this for LINUX.EXE...why??  */
    case 0x25:
      return 0;

    case 0: 
    case 1:
    case 2:
    case 3:
    case 4:
    case 0x17: 
    case 0x10:
    case 0x16:
    case 0x13: 
    case 0x28:   /* idle loop - this is s'posed to be here */
    case 0x33: /* mouse */
      return 1;
    default:
      g_printf("revectoring 0x%02x\n", i);
      return 1;
#else
      /* these are the new ones... */

      /* some things, like 0x29, need to be unrevectored if the vectors
       * are the DOS vectors...but revectored otherwise
       */
    case 0x21: /* we want it first...then we'll pass it on */
    case 0x2a: 
    case 0x15: /* need this for LINUX.EXE...why??  */
    case 0x29: /* DOS fast char output... */
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
    case 0x2f: /* multiplex */
    case 0x20: /* exit program */
    case 0x33: /* mouse */
      return 1;
    default:
      g_printf("revectoring 0x%02x\n", i);
      return 1;
#endif
    }
}

void do_int(int i)
{
  us *ssp;
  int highint=0;

  in_interrupt++;

  if ((_regs.cs&0xffff) == 0xe000)
    {
      /* dbug_printf("highint 0x%x!\n", i); */
      highint=1;
    } 
  else {
    if ((ISEG(i) != 0xe000) && can_revector(i) /* && !is_iret(i) */)
      {
	/* dbug_printf("going to run_int 0x%02x, SG: 0x%04x OF: 0x%04x\n",
	   i, ISEG(i), IOFF(i));
	   show_ints(i); */
	run_int(i);
	return;
      }
    /* else dbug_printf("will be DEFIVEC_0x%x   SG: 0x%04x OF: 0x%04x\n",
		    i, ISEG(i), IOFF(i)); */
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
	g_printf("configuration read\n");
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
    static int running;

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
	  PollKeyboard();
      }

    /* this is severely broken */
    if (timers)
      {
	int old_in = in_interrupt;
	do_int(8);

	h_printf("starting timer int 0x1c...\n");

	/* is vm86() reentrant?  or does it just return on a sigsegv?
         * I really should read the kernel sources...
         */
#if 0
	while ((in_interrupt != old_in) && (in_interrupt > 0)) {
	  g_printf("i: %d  o: %d\n", in_interrupt, old_in);
	  run_vm86();
	}
#endif
      }

    in_sighandler=0;
}

void sigsegv(int sig)
{
  unsigned long a;
  us *ssp;
  unsigned char *csp;
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

  if (_regs.eflags & TF) {
    g_printf("SIGSEGV %d received\n", sig);
    show_regs(); 
  }
  csp = SEG_ADR((unsigned char *), cs, ip);
  switch (*csp) {
  case 0x62: /* bound */
    show_regs();
    _regs.eip += 4;  /* check this... */
    do_int(5);
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
    if (timers) h_printf("in_interrupt: %d\n", timers);
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
  case 0xf4: /* halt */
    error("HLT requested!\n");
    show_regs();
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
      } else {
	haltcount++;
	if (haltcount > MAX_HALT_COUNT) error=0xf4;
	_regs.eip += 1;
      }
    break;
  case 0xf0: /* lock */
  default:
    /* er, why don't we advance eip here, and
       why does it work??  */
    if (! (_regs.eflags && (1 << 17))) /* test VM bit */
      {
	error("ERROR: NON-VM86 general protection!\n");
      }
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
    if (! boot_in_progress) error=4;
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
	if ((i & 0xf800) != 0xd800) { /* no fpu instruction */
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
	error("SIGFPE %d received\n", sig);
	show_regs();
	do_int(0); 
}

void sigtrap(int sig)
{
#if 0
  if (d.warning)
    {
      warn("SIGTRAP %d received\n", sig);
      show_regs();
    }
#endif
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
  const char allopts[]="+vsdRWkpiwgh";

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
	int mapped_bios=0;
	iq.queued=0;
	in_sighandler=0;
	sync(); /* for safety */
	setbuf(stdout, NULL);

	hdiskboot=1;   /* default hard disk boot */
	console_video=0;
	console_keyb=0;
	opterr=0;
	while ( (c=getopt(argc, argv, "ABCfckm:D:pP:bH:F:VNtsg")) != EOF) {
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
	};

	disk_init();
	termioInit();
	hardware_init();
	clear_screen(screen, 7);
	dbug_printf("$Header: /usr/src/dos/RCS/emu.c,v 1.14 1993/02/05 02:54:13 root Exp root $\n");
	p_dos_str("Linux DOS emulator $Revision: 1.14 $  1993\n\r");
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
   fprintf(stderr, "$Header: /usr/src/dos/RCS/emu.c,v 1.14 1993/02/05 02:54:13 root Exp root $\n");
   fprintf(stderr,"usage: dos [-ABCfckbVtspg] [-D flags] [-m SIZE] [-P FILE] [-H|F #disks] > doserr\n");
   fprintf(stderr,"    -A boot from first defined floppy disk (A)\n");
   fprintf(stderr,"    -B boot from second defined floppy disk (B) (#)\n");
   fprintf(stderr,"    -C boot from first defined hard disk (C)\n");
   fprintf(stderr,"    -f flip definitions for A: and B: floppies\n");
   fprintf(stderr,"    -c use PC console video (kernel 0.99pl3+) (!%%)\n");
   fprintf(stderr,"    -k use PC console keyboard (kernel 0.99pl3+) (!)\n");
   fprintf(stderr,"    -D set debug-msg mask to flags (+-)(avkdRWspwg01)\n");
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

p_dos_str(const char *fmt, ...)
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
