/* dos emulator, Matthias Lautner */

/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/01/28 02:19:16 $
 * $Source: /usr/src/dos/RCS/termio.c,v $
 * $Revision: 1.11 $
 * $State: Exp $
 *
 * $Log: termio.c,v $
 * Revision 1.11  1993/01/28  02:19:16  root
 * see emu.c 1.12.
 * THIS IS THE DOSEMU47 DISTRIBUTION TERMIO.C
 *
 * Revision 1.10  1993/01/21  21:56:59  root
 * I did it! I added the WAITACTIVE support to termio.c!  basically, now it
 * can detect being run from a console, what console # it is also, of course.
 * it won't overwrite another VC's memory no matter what tortuous VC
 * switching you try, either!  yea! safe for the masses, almost.
 *
 * Revision 1.9  1993/01/21  02:01:12  root
 * basically just added the cursor-visibility support. I really need to
 * add the race-condition code for setting raw video, the asynchronous
 * keyboard, ioctl return code checking (like, to see if kbd_fd really
 * DOES point to a console).
 *
 * Revision 1.8  1993/01/19  15:30:23  root
 * see emu.c 1.8. kbd flags are now echoed to the BIOS Data Area.
 * fixed some of the fkey scancodes in funkey[]. expanded highscan[] a
 * bit. fixed reversed entries in alt_keys[]. cleaned up convKey() a bit,
 * too.  remember to include all the national keyboards in the finished
 * product, or tell people how to get them from keyboard.c in the kernel.
 *
 * Revision 1.7  1993/01/16  01:18:47  root
 * tried to fix some of the race conditions with allow_switch(),
 * but I still can't atomically tell if I'm the current console
 * and mmap the mem if I am.  this is bad :-(.  I also haven't fixed
 * the problem with the kernel fast scrolling hardware offset stuff.
 * waiting for KDMAPDISP to do that one.
 *
 * Revision 1.6  1993/01/15  01:03:43  root
 * safer console_video mode, as the screenis now unmap'ed before switching
 * VC's.  need to extend this for all 4 pages of EGA/VGA.  took out
 * some annoying key-debugging messages for console_keyb mode.
 *
 * Revision 1.5  1993/01/14  21:57:21  root
 * see emu.c version 1.5.  GOT THE DIRECT SCREEN THING WORKING!
 * bad news: it assumes you're using an 80x25 color EGA display.
 * (i.e. it uses address 0xb8000 no matter what Linux thinks you're
 * doing).  This will be fixed when KDMAPDISP is done, although I'd
 * rather he not put the ENABIO stuff with it (Zorst, that is).
 * If he does it.  If not, Robert to the rescue!
 * anyway, it's a work very much in progress.
 *
 * Revision 1.4  1993/01/12  01:25:57  root
 * added log RCS variable
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <termio.h>
#include <sys/time.h>
#include <termcap.h>
#include <sys/mman.h>
#include <linux/mm.h>
#include <signal.h>

#include "emu.h"
#include "termio.h"

/*********** Robert's changes ************/
#include <linux/vt.h>
#include <linux/kd.h>

/* imports from dos.c */
extern int console_video,
           console_keyb,
  	   vga;

unsigned char old_modecr,
         new_modecr;  /* VGA mode control register */
unsigned short int vga_start;  /* start of video mem */

extern int ignore_segv;
extern int dbug_printf(const char *, ...);
struct sigaction sa;

#define SETSIG(sig, fun)	sa.sa_handler = fun; \
				sa.sa_flags = 0; \
				sa.sa_mask = 0; \
				sigaction(sig, &sa, NULL);

unsigned int convscanKey(unsigned char);
unsigned int queue;

#define put_queue(psc) (queue = psc)

void release_vt(), acquire_vt();

static void gettermcap(void),
 CloseKeyboard(void),
 getKeys(void),
 sysreq(unsigned int),
 ctrl(unsigned int),
 alt(unsigned int),
 unctrl(unsigned int),
 unalt(unsigned int),
 lshift(unsigned int),
 unlshift(unsigned int),
 rshift(unsigned int),
 unrshift(unsigned int),
 caps(unsigned int),
 uncaps(unsigned int),
 scroll(unsigned int),
 num(unsigned int),
 do_self(unsigned int),
 cursor(unsigned int),
 cur(unsigned int),
 func(unsigned int),
 slash(unsigned int),
 star(unsigned int),
 enter(unsigned int),
 minus(unsigned int),
 plus(unsigned int),
 backspace(unsigned int),
 tab(unsigned int),
 none(unsigned int),
 spacebar(unsigned int);

extern void dos_ctrl_alt_del(void);

 void set_kbd_flag(int),
  clr_kbd_flag(int),
  chg_kbd_flag(int),
  set_key_flag(int),
  clr_key_flag(int),
  chg_key_flag(int);

 int kbd_flag(int),
     key_flag(int);


/* initialize these in OpenKeyboard! */
unsigned int kbd_flags = 0;
unsigned int key_flags = 0;

int vt_allow,      /* allow VC switches now? */
    vt_requested;  /* was one requested in a disallowed state? */

/* the file descriptor for /dev/mem when mmap'ing the video mem */
int mem_fd=-1; 
int video_ram_mapped;   /* flag for whether the video ram is mapped */

typedef void (*fptr)(unsigned int);

static fptr key_table[] = {
	none,do_self,do_self,do_self,		/* 00-03 s0 esc 1 2 */
	do_self,do_self,do_self,do_self,	/* 04-07 3 4 5 6 */
	do_self,do_self,do_self,do_self,	/* 08-0B 7 8 9 0 */
	do_self,do_self,backspace,tab,		/* 0C-0F + ' bs tab */
	do_self,do_self,do_self,do_self,	/* 10-13 q w e r */
	do_self,do_self,do_self,do_self,	/* 14-17 t y u i */
	do_self,do_self,do_self,do_self,	/* 18-1B o p } ^ */
	enter,ctrl,do_self,do_self,		/* 1C-1F enter ctrl a s */
	do_self,do_self,do_self,do_self,	/* 20-23 d f g h */
	do_self,do_self,do_self,do_self,	/* 24-27 j k l | */
	do_self,do_self,lshift,do_self,		/* 28-2B { para lshift , */
	do_self,do_self,do_self,do_self,	/* 2C-2F z x c v */
	do_self,do_self,do_self,do_self,	/* 30-33 b n m , */
	do_self,slash,rshift,star,		/* 34-37 . / rshift * */
	alt,spacebar,caps,func,			/* 38-3B alt sp caps f1 */
	func,func,func,func,			/* 3C-3F f2 f3 f4 f5 */
	func,func,func,func,			/* 40-43 f6 f7 f8 f9 */
	func,num,scroll,cursor,			/* 44-47 f10 num scr home */
	cursor,cursor,minus,cursor,		/* 48-4B up pgup - left */
	cursor,cursor,plus,cursor,		/* 4C-4F n5 right + end */
	cursor,cursor,cursor,cursor,		/* 50-53 dn pgdn ins del */
	sysreq,none,do_self,func,		/* 54-57 sysreq ? < f11 */
	func,none,none,none,			/* 58-5B f12 ? ? ? */
	none,none,none,none,			/* 5C-5F ? ? ? ? */
	none,none,none,none,			/* 60-63 ? ? ? ? */
	none,none,none,none,			/* 64-67 ? ? ? ? */
	none,none,none,none,			/* 68-6B ? ? ? ? */
	none,none,none,none,			/* 6C-6F ? ? ? ? */
	none,none,none,none,			/* 70-73 ? ? ? ? */
	none,none,none,none,			/* 74-77 ? ? ? ? */
	none,none,none,none,			/* 78-7B ? ? ? ? */
	none,none,none,none,			/* 7C-7F ? ? ? ? */
	none,none,none,none,			/* 80-83 ? br br br */
	none,none,none,none,			/* 84-87 br br br br */
	none,none,none,none,			/* 88-8B br br br br */
	none,none,none,none,			/* 8C-8F br br br br */
	none,none,none,none,			/* 90-93 br br br br */
	none,none,none,none,			/* 94-97 br br br br */
	none,none,none,none,			/* 98-9B br br br br */
	none,unctrl,none,none,			/* 9C-9F br unctrl br br */
	none,none,none,none,			/* A0-A3 br br br br */
	none,none,none,none,			/* A4-A7 br br br br */
	none,none,unlshift,none,		/* A8-AB br br unlshift br */
	none,none,none,none,			/* AC-AF br br br br */
	none,none,none,none,			/* B0-B3 br br br br */
	none,none,unrshift,none,		/* B4-B7 br br unrshift br */
	unalt,none,uncaps,none,			/* B8-BB unalt br uncaps br */
	none,none,none,none,			/* BC-BF br br br br */
	none,none,none,none,			/* C0-C3 br br br br */
	none,none,none,none,			/* C4-C7 br br br br */
	none,none,none,none,			/* C8-CB br br br br */
	none,none,none,none,			/* CC-CF br br br br */
	none,none,none,none,			/* D0-D3 br br br br */
	none,none,none,none,			/* D4-D7 br br br br */
	none,none,none,none,			/* D8-DB br ? ? ? */
	none,none,none,none,			/* DC-DF ? ? ? ? */
	none,none,none,none,			/* E0-E3 e0 e1 ? ? */
	none,none,none,none,			/* E4-E7 ? ? ? ? */
	none,none,none,none,			/* E8-EB ? ? ? ? */
	none,none,none,none,			/* EC-EF ? ? ? ? */
	none,none,none,none,			/* F0-F3 ? ? ? ? */
	none,none,none,none,			/* F4-F7 ? ? ? ? */
	none,none,none,none,			/* F8-FB ? ? ? ? */
	none,none,none,none			/* FC-FF ? ? ? ? */
};

/********** end of Robert's changes *******/


#define us unsigned short
#define	KEYBOARD	"/dev/tty"

int kbd_fd=-1,		/* the fd for the keyboard */
    old_kbd_flags,      /* flags for STDIN before our fcntl */
    console_no;		/* if console_keyb, the number of the console */

int akt_keycode, kbcount;
unsigned char kbbuf[50], *kbp, akt_key, erasekey;
static  struct termio   oldtermio;      /* original terminal modes */

char tc[1024], termcap[1024],
     *cl,	/* clear screen */
     *le,	/* cursor left */
     *cm,	/* goto */
     *ce,	/* clear to end */
     *sr,	/* scroll reverse */
     *so,	/* stand out start */
     *se,	/* stand out end */
     *md,	/* hilighted */
     *mr,	/* reverse */
     *me,	/* normal */
     *ti,	/* terminal init */
     *te,	/* terminal exit */
     *ks,	/* init keys */
     *ke,	/* ens keys */
     *vi,       /* hide cursor */
     *ve,       /* return corsor to normal */
     *tp;
int   li, co;   /* lines, columns */     

struct funkeystruct {
	char *esc;
	char *tce;
	us code;
};


#define FUNKEYS 20
static struct funkeystruct funkey[FUNKEYS] = {
	{NULL, "kI", 0x5200}, /* Ins */
	{NULL, "kD", 0x5300}, /* Del...he had 127 */
	{NULL, "kh", 0x4700}, /* Ho...he had 0x5c00 */
	{NULL, "kH", 0x4f00}, /* End...he had 0x6100 */
	{NULL, "ku", 0x4800}, /* Up */
	{NULL, "kd", 0x5000}, /* Dn */
	{NULL, "kr", 0x4d00}, /* Ri */
	{NULL, "kl", 0x4b00}, /* Le */
	{NULL, "kP", 0x4900}, /* PgUp */
	{NULL, "kN", 0x5100}, /* PgDn */
	{NULL, "k1", 0x3b00}, /* F1 */
	{NULL, "k2", 0x3c00}, /* F2 */
	{NULL, "k3", 0x3d00}, /* F3 */
	{NULL, "k4", 0x3e00}, /* F4 */
	{NULL, "k5", 0x3f00}, /* F5 */
	{NULL, "k6", 0x4000}, /* F6 */
	{NULL, "k7", 0x4100}, /* F7 */
	{NULL, "k8", 0x4200}, /* F8 */
	{NULL, "k9", 0x4300}, /* F9 */
	{NULL, "k0", 0x4400}, /* F10 */
};

/* this table is used by convKey() to give the int16 functions the
   correct scancode in the high byte of the returned key (AH) */

#ifndef OLD_SCAN_TABLE
unsigned char highscan[256] = {
0,0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0xe,0x0f,0x24,0x25,0x2e,0x1c,  /* ASCII 0 - 0xd */
0x31,0x18,0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c, /* -> 0x1a */
1,0x2b,0,7,0xc,  /* ASCII 1b-1F */
0x39,2,0x28,4,5,6,8,0x28,0xa,0xb,9,0xd, 0x33, 0x0c, 0x34, 0x35,  /* -> 2F */
0x0b,2,3,4,5,6,7,8,9,0xa,    /* numbers 0, 1-9; ASCII 0x30-0x39 */
0x27,0x27,0x33,0xd,0x34,0x35,3,  /* ASCII 0x3A-0x40  */
0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31, /* CAP LETERS A-N */
0x18,0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c, /* CAP O-Z last ASCII 0x5a */
0x1a,0x2b,0x1b,7,0x0c,0x29, /* ASCII 0x5b-0x60 */
/* 0x61 - 0x7a on next 2 lines */
0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18, /* lower a-o */
0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c,   /* lowercase p-z */
0x1a,0x2b,0x1b,0x29,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0x7b-0x8f */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0x90-0x9f */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0xa0-0xaf */
0x81,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x80,0,0,0,0,0,0, /* 0xb0-0xbf */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0xc0-0xcf */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0xd0-0xdf */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0xe0-0xef */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* ASC 0xf0-0xff */
};
#else
char highscan[256] = {
0,0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0xe,0x0f,0,0,0,0x1c,  /* ASCII 0 - 0xd */
0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,  /* ASCII 0xD-1F */
0x39, 0,0,0,0,0,0,0,0,0,0,0, 0x33 /*2c*/, 0x0c, 0x34, 0x35,
0x0b,2,3,4,5,6,7,8,9,0xa,    /* numbers 0, 1-9; ASCII 0x30-0x39 */
0,0,0,0,0,0, 3 /* ASC0x40 shft-2 */,  /* ASCII 0x3A-0x40  */
0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31, /* CAP LETERS A-N */
0x18,0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c, /* CAP O-Z last ASCII 0x5a */
0x1a,0x2b,0x1b,0,0x0c,0x29, /* ASCII 0x5b-0x60 */
/* 0x61 - 0x7a on next 2 lines */
0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18, /* lower a-o */
0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c,   /* lowercase p-z */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0x7b-0x8f */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0x90-0x9f */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0xa0-0xaf */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0xb0-0xbf */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0xc0-0xcf */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0xd0-0xdf */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ASC 0xe0-0xef */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* ASC 0xf0-0xff */
};
#endif

int outc(int c)
{
  write(2, (char *)&c, 1);
  return 1;
}

static void gettermcap(void)
{
struct winsize ws;		/* buffer for TIOCSWINSZ */
struct funkeystruct *fkp;

  li = 0;
  co = 0;
  if (ioctl(2, TIOCGWINSZ, &ws) >= 0) {
    li = ws.ws_row;
    co = ws.ws_col;
    dbug_printf("SCREEN SIZE----CO: %d, LI: %d\n", co, li);
  }
  if(tgetent(termcap, getenv("TERM")) != 1)
  {
    dbug_printf("ERROR: no termcap \n");
    leavedos(1);
  }
  if (li == 0 || co == 0) {
    li = tgetnum("li");      /* lines   */
    co = tgetnum("co");      /* columns */
  }
  tp = tc;
  cl = tgetstr("cl", &tp);   /* clear entire screen */
  le = tgetstr("le", &tp);   /* move cursor left one column */
  cm = tgetstr("cm", &tp);   /* cursor motion */
  ce = tgetstr("ce", &tp);   /* clear cursor to EOL */
  sr = tgetstr("sr", &tp);   /* scroll screen one line down */
  so = tgetstr("so", &tp);   /* enter standout mode */
  se = tgetstr("se", &tp);   /* leave standout mode */
  ti = tgetstr("ti", &tp);   /* return term. to sequential output */
  te = tgetstr("te", &tp);   /* init. term. for random cursor motion */
  ks = tgetstr("ks", &tp);   /* make fkeys transmit */
  ke = tgetstr("ke", &tp);   /* make fkeys work locally */
  mr = tgetstr("mr", &tp);   /* enter reverse video mode */
  md = tgetstr("md", &tp);   /* enter double bright mode */
  me = tgetstr("me", &tp);   /* turn off all appearance modes */
  vi = tgetstr("vi", &tp);   /* hide cursor */
  ve = tgetstr("ve", &tp);   /* return cursor to normal */
  if (se == NULL) so = NULL;
  if (md == NULL || mr == NULL) me = NULL;
  if (li == 0 || co == 0) {
    dbug_printf("ERROR: unknown window sizes \n");
    leavedos(1);
  }
  for (fkp=funkey; fkp < &funkey[FUNKEYS]; fkp++) {
	fkp->esc = tgetstr(fkp->tce, &tp);
	if (!fkp->esc) dbug_printf("ERROR: can't get termcap %s\n", fkp->tce);
  }
}

static void CloseKeyboard(void)
{
  if (kbd_fd != -1)
    {
      if (console_keyb)
	clear_raw_mode();
      if (console_video)
	clear_console_video();

      if (console_keyb || console_video)
	clear_process_control();

      printf("old keyboard flags on exit: 0x%x\n", old_kbd_flags);
      fcntl(kbd_fd, F_SETFL, old_kbd_flags);

      ioctl(kbd_fd, TCSETAF, &oldtermio);

      close(kbd_fd);
      kbd_fd = -1;
    }
}

static int OpenKeyboard(void)
{
	struct termio	newtermio;	/* new terminal modes */
	struct stat chkbuf;
	unsigned int i;
	int major,minor;

	kbd_fd = dup(STDIN_FILENO);
	old_kbd_flags = fcntl(kbd_fd, F_GETFL);
	fcntl(kbd_fd, F_SETFL, O_RDONLY | O_NONBLOCK);

	if (kbd_fd < 0)
		return -1;
	
	fstat(kbd_fd, &chkbuf);
	major=chkbuf.st_rdev >> 8;
	minor=chkbuf.st_rdev & 0xff;
	/* console major num is 4, minor 64 is the first serial line */
	if ((major == 4) && (minor < 64))
	    console_no=minor;  /* get minor number */
	else
	  {
	    if (console_keyb || console_video) 
	      dbug_printf("ERROR: STDIN not a console-can't do console modes!\n");
	    console_no=0;
	    console_keyb=0;
	    console_video=0;
	  }

	if (ioctl(kbd_fd, TCGETA, &oldtermio) < 0) {
		close(kbd_fd);
		kbd_fd = -1;
		return -1;
	}

	newtermio = oldtermio;
	newtermio.c_iflag &= (ISTRIP|IGNBRK); /* (IXON|IXOFF|IXANY|ISTRIP|IGNBRK);*/
	/* newtermio.c_oflag &= ~OPOST; */
	newtermio.c_lflag &= /* ISIG */ 0;
	newtermio.c_cc[VMIN] = 1;
	newtermio.c_cc[VTIME] = 0;
	erasekey = newtermio.c_cc[VERASE];
	if (ioctl(kbd_fd, TCSETAF, &newtermio) < 0) {
		close(kbd_fd);
		kbd_fd = -1;
		return -1;
	}

	if (console_keyb || console_video)
	  set_process_control();

	if (console_keyb)
	  {
	    set_raw_mode();
	    kbd_flags=0;
	    key_flags=0;
	    get_leds(); 
	  }

	if (console_video)
	  {
	    int other_no=(console_no == 1 ? 2 : 1);

	    set_console_video();
	    ioctl(kbd_fd, VT_ACTIVATE, other_no);
	    ioctl(kbd_fd, VT_ACTIVATE, console_no);
	  }
	dbug_printf("$Header: /usr/src/dos/RCS/termio.c,v 1.11 1993/01/28 02:19:16 root Exp $\n");
	return 0;
}

forbid_switch()
{
  vt_allow=0;
}

allow_switch()
{
  /* dbug_printf("allow_switch called\n"); */
  vt_allow=1;
  if (vt_requested)
    {
      dbug_printf("clearing old vt request\n");
      release_vt(0);
    }
  /* else dbug_printf("allow_switch finished\n"); */
}
      
void acquire_vt(int sig)
{
  int kb_mode;

  SETSIG(SIG_ACQUIRE, acquire_vt);

  /* dbug_printf("acquire_vt() called!\n"); */
  if (ioctl(kbd_fd, VT_RELDISP, VT_ACKACQ)) /* switch acknowledged */
#if 0
    dbug_printf("VT_REL(ACQ)DISP failed!\n");
  else
    dbug_printf("VT_REL(ACQ)DISP succeeded!\n");
#else
  ;
#endif

  if (console_video)
    {
      get_video_ram();
      if (vga) set_dos_video();
    }
}

set_dos_video()
{
  /* turn blinking attr bit -> a background color bit */

  dbug_printf("Setting DOS video: modecr = 0x%x\n", new_modecr);

  if (ioperm(0x3da,1,1) || ioperm(0x3c0,1,1) || ioperm(0x3d4,2,1))
    {
      dbug_printf("ERROR: Can't get I/O permissions!\n");
      return (-1);
    }

  port_in(0x3da);
  port_out(0x10+32, 0x3c0);
  port_out(new_modecr, 0x3c0); 

#if SET_ORIGIN
  /* set screen origin to 0 */
  port_out(0xc, 0x3d4);
  port_out(0, 0x3d5);
  port_out(0xd, 0x3d4);
  port_out(0, 0x3d5);
#endif

  ioperm(0x3da,1,0);
  ioperm(0x3c0,1,0);
  ioperm(0x3d4,2,0);
}

set_linux_video()
{
  /* return vga card to Linux normal setup */

  dbug_printf("Setting Linux video: modecr = 0x%x\n", old_modecr);

  if (ioperm(0x3da,1,1) || ioperm(0x3c0,1,1) || ioperm(0x3d4,2,1))
    {
      dbug_printf("ERROR: Can't get I/O permissions!\n");
      return (-1);
    }

  port_in(0x3da);
  port_out(0x10+32, 0x3c0);
  port_out(old_modecr, 0x3c0); 

#if SET_ORIGIN
  /* set screen origin to vga_start */
  port_out(0xc, 0x3d4);
  port_out(vga_start >> 8, 0x3d5);
  port_out(0xd, 0x3d4);
  port_out(vga_start & 0xff, 0x3d5);
#endif

  ioperm(0x3da,1,0);
  ioperm(0x3c0,1,0);
  ioperm(0x3d4,2,0);
}

void release_vt(int sig)
{
  SETSIG(SIG_RELEASE, release_vt);
  /* dbug_printf("release_vt() called!\n"); */

  if (! vt_allow)
    {
      dbug_printf("disallowed vt switch!\n");
      vt_requested=1;
      return;
    }

  if (console_video)
    {
      put_video_ram();
      if (vga) set_linux_video();
    }

  if (ioctl(kbd_fd, VT_RELDISP, 1))       /* switch ok by me */
#if 0
    dbug_printf("VT_RELDISP failed!\n");
  else
    dbug_printf("VT_RELDISP succeeded!\n");
#else
  ;
#endif
}



get_video_ram()
{
  char *graph_mem;
  void (*oldhandler)();
  int tmp;

  /* dbug_printf("get_video_ram called\n"); */

#if 1
  console_video=0;

  /* wait until our console is current */
  oldhandler=signal(SIG_ACQUIRE, SIG_IGN);
  do
    {
      if (tmp=ioctl(kbd_fd, VT_WAITACTIVE, console_no) < 0)
	printf("WAITACTIVE gave %d. errno: %d\n", tmp,errno);
      else break;
    } while (errno == EINTR);

  SETSIG(SIG_ACQUIRE, acquire_vt);
  console_video=1;
#endif

  if (video_ram_mapped)
    memcpy((caddr_t)SCRN_BUF_ADDR, (caddr_t)VIRT_SCRN_BASE, SCRN_SIZE);

  graph_mem =
    (char *)mmap(
		 (caddr_t)VIRT_SCRN_BASE, 
		 SCRN_SIZE,
		 PROT_READ|PROT_WRITE,
		 MAP_SHARED|MAP_FIXED,
		 mem_fd, 
		 PHYS_SCRN_BASE
		 );
  
  if ((long)graph_mem < 0) {
    dbug_printf("ERROR: mmap error in get_video_ram\n");
    return (1);
  }
  else dbug_printf("CONSOLE VIDEO address: 0x%x\n", graph_mem);

  if (video_ram_mapped)
    {
      memcpy((caddr_t)VIRT_SCRN_BASE, (caddr_t)SCRN_BUF_ADDR, SCRN_SIZE);
    }
  else video_ram_mapped=1;
}

put_video_ram()
{
  /* dbug_printf("put_video_ram called\n"); */
  memcpy((caddr_t)SCRN_BUF_ADDR, (caddr_t)VIRT_SCRN_BASE, SCRN_SIZE);
  munmap((caddr_t)VIRT_SCRN_BASE, SCRN_SIZE);
  memcpy((caddr_t)VIRT_SCRN_BASE, (caddr_t)SCRN_BUF_ADDR, SCRN_SIZE);
}

/* this puts the VC under process control */
set_process_control()
{
  struct vt_mode vt_mode;

  vt_mode.mode = VT_PROCESS;
  vt_mode.waitv=0;
  vt_mode.relsig=SIG_RELEASE;
  vt_mode.acqsig=SIG_ACQUIRE;
  vt_mode.frsig=0;

  vt_requested=0;    /* a switch has not been attempted yet */  
  allow_switch();

  SETSIG(SIG_RELEASE, release_vt);
  SETSIG(SIG_ACQUIRE, acquire_vt);

  if (ioctl(kbd_fd, VT_SETMODE, &vt_mode))
#if 0
    dbug_printf("initial VT_SETMODE failed!\n");
  else
    dbug_printf("initial VT_SETMODE to VT_PROCESS succeded!\n");
#else
   ;
#endif
}

clear_process_control()
{
 struct vt_mode vt_mode;

 vt_mode.mode = VT_AUTO;
 ioctl(kbd_fd, VT_SETMODE, &vt_mode);
 SETSIG(SIG_RELEASE, SIG_IGN);
 SETSIG(SIG_ACQUIRE, SIG_IGN);
}

open_kmem()
{
    if ((mem_fd = open("/dev/mem", O_RDWR) ) < 0) {
	dbug_printf("ERROR: can't open /dev/mem \n");
	return (-1);
    }
}

set_console_video()
{
    int i;

    /* clear Linux's (unmapped) screen */
    tputs(cl, 1, outc);
    dbug_printf("set_console_video called\n");
   
    forbid_switch();
    get_video_ram();

    if (vga)
      {
	int permtest;

	permtest = ioperm(0x3d4, 2, 1);  /* get 0x3d4 and 0x3d5 */
	permtest |= ioperm(0x3da, 1, 1);
	permtest |= ioperm(0x3c0, 2, 1);  /* get 0x3c0 and 0x3c1 */

	if (permtest)
	  {
	    dbug_printf("ERROR: can't get I/O permissions: vga disabled!\n");
	    vga=0;  /* if i can't get permissions, forget -V mode */
	  }
	else
	  {
	    port_in(0x3da);
	    port_out(0x10+32, 0x3c0);
	    old_modecr=port_in(0x3c1); 
	    new_modecr=old_modecr & ~(1 << 3);  /* turn off blink-enable bit */

	    /* get vga memory start */
	    port_in(0x3da);
	    port_out(0xc, 0x3d4);
	    vga_start=port_in(0x3d5) << 8;
	    port_in(0x3da);
	    port_out(0xd, 0x3d4);
	    vga_start |= (unsigned char)port_in(0x3d5);
	    
	    dbug_printf("MODECR: old=0x%x...ORIG: 0x%x\n",
			old_modecr,vga_start);
	    
	    ioperm(0x3da,1,0);
	    ioperm(0x3c0,2,0);
	    ioperm(0x3d4,2,0);
	  }
      }

    clear_screen(0,0);

/* the offset bug is caused, I think, by the kernel using screen offsets
   on the EGA/VGA for faster scrolling (i.e. at times the beg of video
   is NOT 0xb8000.  KDMAPDISP should somehow make the hardware offsets
   go back to zero if the MAPPED console is the current one (it is
   done automatically on VC-switches, otherwise).  The fuction
   responsible for this is set_origin(cons#)  
 
   the temporary fix is to switch away, then back - but only after the
   emulator has given you a C> prompt! I have to clean up this race
   condition. */

    allow_switch();
}


clear_console_video()
{
  dbug_printf("clear_console_video called\n");
  put_video_ram();		/* unmap the screen */
  show_cursor();		/* restore the cursor */
  if (vga)
    set_linux_video();
}

clear_raw_mode()
{
 ioctl(kbd_fd, KDSKBMODE, K_XLATE);
}

set_raw_mode()
{
  dbug_printf("Setting keyboard to RAW mode\n");
  if (!console_video) fprintf(stderr, "\nEntering RAW mode for DOS!\n");
  ioctl(kbd_fd, KDSKBMODE, K_RAW);
}


void map_bios(void)
{
  char *video_bios_mem, *system_bios_mem;

  dbug_printf("map_bios called\n");

  video_bios_mem =
    (char *)mmap(
		 (caddr_t)0xc0000, 
		 32*1024,
		 PROT_READ,
		 MAP_SHARED|MAP_FIXED,
		 mem_fd, 
		 0xc0000
		 );
  
  if ((long)video_bios_mem < 0) {
    dbug_printf("ERROR: mmap error in map_bios\n");
    return;
  }
  else dbug_printf("VIDEO BIOS address: 0x%x\n", video_bios_mem);

  system_bios_mem =
    (char *)mmap(
		 (caddr_t)0xf0000, 
		 64*1024,
		 PROT_READ,
		 MAP_SHARED|MAP_FIXED,
		 mem_fd, 
		 0xf0000
		 );
  
  if ((long)system_bios_mem < 0) {
    dbug_printf("ERROR: mmap error in map_bios\n");
    return;
  }
  else dbug_printf("SYSTEM BIOS address: 0x%x\n", system_bios_mem);
}


static struct termios   save_termios;

int tty_raw(int fd)
{
  struct termios buf;

  if (tcgetattr(fd, &save_termios) < 0)
      return(-1);
  buf = save_termios;

  buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  buf.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  buf.c_cflag &= ~(CSIZE | PARENB);
  buf.c_cflag |= CS8;
  buf.c_oflag &= ~(OPOST);
  buf.c_cc[VMIN] = 2;
  buf.c_cc[VTIME] = 0;

  if (tcsetattr(fd, TCSAFLUSH, &buf) < 0)
    return(-1);
#if 0
  ttystate = RAW;
  ttysavefd = fd;
#endif
  return(0);


}

static us alt_keys[] = { /* <ALT>-A ... <ALT>-Z */
	0x1e00, 0x3000, 0x2e00, 0x2000, 0x1200, 0x2100,
	0x2200, 0x2300, 0x1700, 0x2400, 0x2500, 0x2600,
	0x3200, 0x3100, 0x1800, 0x1900, 0x1000, 0x1300,
	0x1f00, 0x1400, 0x1600, 0x2f00, 0x1100, 0x2d00,
	0x1500, 0x2c00};

static us alt_nums[] = { /* <ALT>-0 ... <ALT>-9 */
	0x8100, 0x7800, 0x7900, 0x7a00, 0x7b00, 0x7c00,
	0x7d00, 0x7e00, 0x7f00, 0x8000};

static void getKeys(void)
{
        int     cc;

        if (kbcount == 0) {
                kbp = kbbuf;
        } else if (kbp > &kbbuf[30]) {
                memmove(kbbuf, kbp, kbcount);
                kbp = kbbuf;
        }
        cc = read(kbd_fd, &kbp[kbcount], &kbbuf[50] - kbp);
        if (cc > 0) {
                kbcount += cc;
        }
}


/* static */ unsigned int convKey()
{
	int i;
	struct timeval scr_tv;
	struct funkeystruct *fkp;
	fd_set fds;
	unsigned int kbd_mode;
	static int rawcount=0;

	if (kbcount == 0) return 0;

	ioctl(kbd_fd, KDGKBMODE, &kbd_mode);   /* get kb mode */

	if (kbd_mode == K_RAW) 
	{
	  unsigned char scancode = *kbp & 0xff;
	  unsigned int tmpcode = 0;

	  if (rawcount == 0) 
	    {
	      if (!console_video) fprintf(stderr,"Going into RAW mode!\n");
	      dbug_printf("Going into RAW mode!\n");
	      get_leds();
	      key_flags = 0; 
	    }

	  rawcount++;

	  tmpcode = convscanKey(scancode);
	  kbp++;
	  kbcount--;
	  return tmpcode;
	}

	xlate:
	if (rawcount != 0)
	  {
	    dbug_printf("left RAW mode, I guess\n");
	    if (!console_video) fprintf(stderr,"\nleft RAW mode, I guess\n");

	    rawcount=0;
	  }

	/* get here only if in cooked mode (i.e. K_XLATE) */
	if (*kbp == '\033') {
		if (kbcount == 1) {
			scr_tv.tv_sec = 0;
			scr_tv.tv_usec = 500000;
			FD_ZERO(&fds);
			FD_SET(kbd_fd, &fds);
			select(kbd_fd+1, &fds, NULL, NULL, &scr_tv);
			getKeys();
			if (kbcount == 1) {
				kbcount--;
				return ((highscan[*kbp] << 8 ) +
					(unsigned char)*kbp++); 
			}

		}
#define LATIN1 1
#define METAKEY 1
#ifdef LATIN1
		dbug_printf("latin1 extended keysensing\n");
		if (islower((unsigned char)kbp[1])) {
			kbcount -= 2;
			kbp++;
			return alt_keys[*kbp++ - 'a'];
		} else if (isdigit((unsigned char)kbp[1])) {
			kbcount -= 2;
			kbp++;
			return alt_nums[*kbp++ - '0'];
		}
#endif
		fkp = funkey;
		for (i=1;;) {
			if (fkp->esc == NULL || 
			    fkp->esc[i] < kbp[i]) {
				if (++fkp >= &funkey[FUNKEYS])
					break;
			} else if (fkp->esc[i] == kbp[i]) {
				if (fkp->esc[++i] == '\0') {
					kbcount -= i;
					kbp += i;
					return fkp->code;
				}
				if (kbcount <= i) {
					scr_tv.tv_sec = 0;
					scr_tv.tv_usec = 800000;
					FD_ZERO(&fds);
					FD_SET(kbd_fd, &fds);
					select(kbd_fd+1, &fds, NULL, NULL, &scr_tv);
					getKeys();
					if (kbcount <= i) {
						break;
					}
				}
			} else {
				break;
			}
		}
	} else if (*kbp == erasekey) {
		kbcount--;
		kbp++;
		return (((unsigned char)highscan[8] << 8)  + (unsigned char)8);

#ifdef METAKEY
/* #ifndef LATIN1 */
	} else if ((unsigned char)*kbp >= ('a'|0x80) && 
		   (unsigned char)*kbp <= ('z'|0x80)) {
		kbcount--;
		return ((unsigned short int)alt_keys[*kbp++ - ('a'|0x80)]);
	} else if ((unsigned char)*kbp >= ('0'|0x80) && 
		   (unsigned char)*kbp <= ('9'|0x80)) {
		kbcount--;
		return alt_nums[(unsigned char)*kbp++ - ('0'|0x80)];
#endif
	}

        kbcount--;
	
	i=highscan[*kbp] << 8;   /* get scancode */

	/* extended scancodes return 0 for the ascii value */
	if ((unsigned char)*kbp < 0x80) i |= (unsigned char)*kbp;

	kbp++;
	return (i); 
}

/* ReadKeyboard
   returns 1 if a character could be read in buf 
   */
int ReadKeyboard(unsigned int *buf, int wait)
{
	fd_set fds;
	int r;
	static unsigned int aktkey;

	while (!aktkey) {
		if (kbcount == 0 && wait == WAIT) {
			FD_ZERO(&fds);
			FD_SET(kbd_fd, &fds);
			r = select(kbd_fd+1, &fds, NULL, NULL, NULL);
		}
		getKeys();
		if (kbcount == 0 && wait != WAIT) return 0;
		aktkey = convKey();
	}
	*buf = aktkey;
	if (wait != TEST) aktkey = 0;
	return 1;
}


/* ReadString
   reads a string into a buffer
   buf[0] ... length of string
   buf +1 ... string
   */
void ReadString(int max, char *buf)
{
	char ch, *cp = buf +1, *ce = buf + max;
	unsigned int c;

	for (;;) {
		if (ReadKeyboard(&c, WAIT) != 1) continue;
		c &= 0xff;   /* mask out scan code -> makes ASCII */
		if ((unsigned)c >= 128) continue;
		ch = (char)c;
		if (ch >= ' ' && ch <= '~' && cp < ce) {
			*cp++ = ch;
			char_out(ch, screen);
			continue;
		}
		if (ch == '\010' && cp > buf +1) { /* BS */
			cp--;
			char_out('\010', screen);
			char_out(' ', screen);
			char_out('\010', screen);
			continue;
		}
		if (ch == 13) {
			*cp = ch;
			break;
		}
	}
	*buf = (cp - buf) -1; /* length of string */
}

static int fkcmp(const void *a, const void *b)
{
	return strcmp(((struct funkeystruct *)a)->esc, ((struct funkeystruct *)b)->esc);
}

void termioInit()
{
	if (OpenKeyboard() != 0) {
		dbug_printf("ERROR: can't open keyboard\n");
		leavedos(1);
	}
	gettermcap();
	qsort(funkey, FUNKEYS, sizeof(struct funkeystruct), &fkcmp);
        if (ks) tputs(ks, 1, outc); 
}

void termioClose()
{
	CloseKeyboard();
	if (ke) tputs(ke, 1, outc);
}

/**************************************************************
 * this is copied verbatim from the Linux kernel (keyboard.c) *
 **************************************************************/

unsigned int convscanKey(unsigned char scancode)
{
	static unsigned char rep = 0xff;

	fflush(stdout);
	if (scancode == 0xe0)
		set_key_flag(KKF_E0);
	else if (scancode == 0xe1)
		set_key_flag(KKF_E1);

	if (scancode == 0xe0 || scancode == 0xe1)
		return(0);
	/*
	 *  The keyboard maintains its own internal caps lock and num lock
	 *  statuses. In caps lock mode E0 AA precedes make code and E0 2A
	 *  follows break code. In num lock mode, E0 2A precedes make
	 *  code and E0 AA follows break code. We do our own book-keeping,
	 *  so we will just ignore these.
	 */
	if (key_flag(KKF_E0) && (scancode == 0x2a || scancode == 0xaa)) {
		clr_key_flag(KKF_E0);
		clr_key_flag(KKF_E1);
		return(0);
	}
	/*
	 *  Repeat a key only if the input buffers are empty or the
	 *  characters get echoed locally. This makes key repeat usable
	 *  with slow applications and unders heavy loads.
	 */

	rep = scancode;
	queue=0;
	fflush(stdout);
	key_table[scancode](scancode); 
	fflush(stdout);
#if 0
	do_keyboard_interrupt(); 
#endif

	clr_key_flag(KKF_E0);
	clr_key_flag(KKF_E1);

	return(queue);
}


static void ctrl(unsigned int sc)
{
	if (key_flag(KKF_E0))
		set_kbd_flag(EKF_RCTRL);
	else
		set_kbd_flag(EKF_LCTRL);
	set_kbd_flag(KF_CTRL);
}

static void alt(unsigned int sc)
{
	if (key_flag(KKF_E0))
		set_kbd_flag(EKF_RALT);
	else
		set_kbd_flag(EKF_LALT);
	set_kbd_flag(KF_ALT);
}

static void unctrl(unsigned int sc)
{
	if (key_flag(KKF_E0))
		clr_kbd_flag(EKF_RCTRL);
	else
		clr_kbd_flag(EKF_LCTRL);

	if ( !kbd_flag(EKF_LCTRL) && !kbd_flag(EKF_RCTRL) )
	        clr_kbd_flag(KF_CTRL);
}

static void unalt(unsigned int sc)
{
	if (key_flag(KKF_E0))
		clr_kbd_flag(EKF_RALT);
	else 
		clr_kbd_flag(EKF_LALT);

	if (! (kbd_flag(EKF_LALT) && kbd_flag(EKF_RALT)) )
	        clr_kbd_flag(KF_ALT);		
}

static void lshift(unsigned int sc)
{
	set_kbd_flag(KF_LSHIFT);
}

static void unlshift(unsigned int sc)
{
	clr_kbd_flag(KF_LSHIFT);
}

static void rshift(unsigned int sc)
{
	set_kbd_flag(KF_RSHIFT);
}

static void unrshift(unsigned int sc)
{
	clr_kbd_flag(KF_RSHIFT);
}

static void caps(unsigned int sc)
{
  chg_kbd_flag(KF_CAPSLOCK);	/* toggle; this means SET/UNSET */
  set_leds();
}

static void uncaps(unsigned int sc)
{
}

static void sysreq(unsigned int sc)
{
  dbug_printf("Regs requested\n");
  show_regs();
}

static void scroll(unsigned int sc)
{
  chg_kbd_flag(KF_SCRLOCK);
  set_leds();
}

static void num(unsigned int sc)
{
  chg_kbd_flag(KF_NUMLOCK);
  set_leds();
}


set_leds()
{
  unsigned int led_state=0;

  if (kbd_flag(KF_SCRLOCK)) led_state |= (1 << LED_SCRLOCK);
  if (kbd_flag(KF_NUMLOCK)) led_state |= (1 << LED_NUMLOCK);
  if (kbd_flag(KF_CAPSLOCK)) led_state |= (1 << LED_CAPSLOCK);

  ioctl(kbd_fd, KDSETLED, led_state);
}

get_leds()
{
  unsigned int led_state=0;

  ioctl(kbd_fd, KDGETLED, &led_state);

  if  (led_state & (1 << LED_SCRLOCK)) set_kbd_flag(KF_SCRLOCK);
       else clr_kbd_flag(KF_SCRLOCK);
  if  (led_state & (1 << LED_NUMLOCK)) set_kbd_flag(KF_NUMLOCK);
       else clr_kbd_flag(KF_NUMLOCK);
  if  (led_state & (1 << LED_CAPSLOCK)) set_kbd_flag(KF_CAPSLOCK);
       else clr_kbd_flag(KF_CAPSLOCK);
}

/********* start of char maps for U.S. keyboard *********/
static unsigned char key_map[] = {
	  0,   27,  '1',  '2',  '3',  '4',  '5',  '6',
	'7',  '8',  '9',  '0',  '-',  '=',  127,    9,
	'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
	'o',  'p',  '[',  ']',   13,    0,  'a',  's',
	'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
	'\'', '`',    0, '\\',  'z',  'x',  'c',  'v',
	'b',  'n',  'm',  ',',  '.',  '/',    0,  '*',
	  0,   32,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,  '-',    0,    0,    0,  '+',    0,
	  0,    0,    0,    0,    0,    0,  '<',    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0 };

static unsigned char shift_map[] = {
	  0,   27,  '!',  '@',  '#',  '$',  '%',  '^',
	'&',  '*',  '(',  ')',  '_',  '+',  127,    9,
	'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
	'O',  'P',  '{',  '}',   13,    0,  'A',  'S',
	'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
	'"',  '~',  '0',  '|',  'Z',  'X',  'C',  'V',
	'B',  'N',  'M',  '<',  '>',  '?',    0,  '*',
	  0,   32,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,  '-',    0,    0,    0,  '+',    0,
	  0,    0,    0,    0,    0,    0,  '>',    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0 };

static unsigned char alt_map[] = {
	  0,    0,    0,  '@',    0,  '$',    0,    0,
	'{',   '[',  ']', '}', '\\',    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,  '~',   13,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0,    0,    0,    0,    0,    0,  '|',    0,
	  0,    0,    0,    0,    0,    0,    0,    0,
	  0 };

unsigned char num_table[] = "789-456+1230.";
/********* end of char maps for U.S. keyboard *********/



static void do_self(unsigned int sc)
{
	unsigned char ch;

	if (kbd_flag(KF_ALT))
	  {
	    if ((sc >= 2) && (sc <= 0xb))  /* numbers */
	      sc += 0x76;
	    else if (sc == 0xd) sc = 0x83;  /* = */
	    else if (sc == 0xc) sc = 0x82;  /* - */
	    ch = 0;
	  }

	else if (kbd_flag(KF_LSHIFT) || kbd_flag(KF_RSHIFT) ||
		 kbd_flag(KF_CTRL))
		ch = shift_map[sc];
	else
		ch = key_map[sc];

#if 0
	if (ch == 0)
		return;  
#endif

	if (kbd_flag(KF_CTRL) || kbd_flag(KF_CAPSLOCK)) 	/* ctrl or caps */
		if ((ch >= 'a' && ch <= 'z') || (ch >= 224 && ch <= 254))
			ch -= 32;

	if (kbd_flag(KF_CTRL))	/* ctrl */
		ch &= 0x1f;

   put_queue((sc << 8) | ch);
}

static void spacebar(unsigned int sc)
{
  put_queue(0x3920);
}

static void cursor(unsigned int sc)
{
  int old_sc;
 
  old_sc = sc;

  if (sc < 0x47 || sc > 0x53)
    return;
  sc -= 0x47;
  if (sc == 12 && kbd_flag(KF_CTRL) && kbd_flag(KF_ALT)) {
    dos_ctrl_alt_del();
    return;
  }
  if (key_flag(KKF_E0)) {
    cur(old_sc);
    return;
  }
  
  if (kbd_flag(KF_NUMLOCK) || kbd_flag(KF_LSHIFT)
      || kbd_flag(KF_RSHIFT)) {
    put_queue(num_table[sc]);
  } else
    cur(old_sc);
  
  /*  put_queue(sc << 8); */
  dbug_printf("cursor2: 0x%04x\n", sc << 8);
}


static void cur(unsigned int sc)
{
  put_queue(sc << 8);
}

static void backspace(unsigned int sc)
{
  /* should be perfect */
  if (kbd_flag(KF_CTRL))
    put_queue(0x0e7f);
  else if (kbd_flag(KF_ALT))
    put_queue(0x0e00);
  else put_queue(0x0e08);
}

static void tab(unsigned int sc)
{
  if (kbd_flag(KF_CTRL))
    put_queue(0x9400);
  else if (kbd_flag(KF_ALT))
    put_queue(0xa500);
  else if (kbd_flag(KF_LSHIFT) || kbd_flag(KF_RSHIFT))
    put_queue(sc << 8);
  else put_queue(sc << 8 | key_map[sc]);
}

static void func(unsigned int sc)
{
/* should be pretty close to perfect 
 * is the precedence right ? */

  static int fkey_table[]=
    {
      0x3b, 0x3c, 0x3d, 0x3e, 0x41, 0x3f,
      0x40, 0x42, 0x43, 0x44, 0x57, 0x58 
    };

  /* this checks for the VC-switch key sequence */
  if (kbd_flag(EKF_LALT) && !kbd_flag(EKF_RALT) && !kbd_flag(KF_RSHIFT)
      && !kbd_flag(KF_LSHIFT) && !kbd_flag(KF_CTRL))
    {
      clr_kbd_flag(EKF_LALT);
      if (!kbd_flag(EKF_RALT))
	clr_kbd_flag(KF_ALT); 
      ioctl(kbd_fd, VT_ACTIVATE, sc-0x3a);
      return;
    }

  if (kbd_flag(KF_LSHIFT) || kbd_flag(KF_RSHIFT))
    put_queue((sc + 0x19) << 8);
  else if (kbd_flag(KF_CTRL))
    put_queue((sc + 0x23) << 8);
  else if (kbd_flag(KF_ALT))
    put_queue((sc + 0x2d) << 8);
  else
    put_queue(sc << 8);
}

static void slash(unsigned int sc)
{
  if (!key_flag(KKF_E0))
  {
    do_self(sc);
  }
  else put_queue(sc << 8 | '/');
}

static void star(unsigned int sc)
{
	do_self(sc);
}

static void enter(unsigned int sc)
{
  if (kbd_flag(KF_CTRL))
    put_queue(sc << 8 | 0x0a);
  else if (kbd_flag(KF_ALT))
    put_queue(0xa600);
  else
    put_queue(sc << 8 | 0x0d);
}

static void minus(unsigned int sc)
{
	do_self(sc);
}

static void plus(unsigned int sc)
{
	do_self(sc);
}

static void none(unsigned int sc)
{
}


/**************** key-related functions **************/
 void kbd_flags_to_bda()
{
  ignore_segv=1;
   /* this will, of course, generate a SIGSEGV */
   *(unsigned short int *)0x417 = kbd_flags; 
  ignore_segv=0;
}

 void set_kbd_flag(int flag)
{
	kbd_flags |= 1 << flag;
	kbd_flags_to_bda();
}

 void clr_kbd_flag(int flag)
{
	kbd_flags &= ~(1 << flag);
	kbd_flags_to_bda();	
}

 void chg_kbd_flag(int flag)
{
	kbd_flags ^= 1 << flag;
	kbd_flags_to_bda();
}

 int kbd_flag(int flag)
{
	return ((kbd_flags >> flag) & 1);
}


/* these are the KEY flags */
 void set_key_flag(int flag)
{
	key_flags |= 1 << flag;
}

 void clr_key_flag(int flag)
{
	key_flags &= ~(1 << flag);
}

 void chg_key_flag(int flag)
{
	key_flags ^= 1 << flag;
}

 int key_flag(int flag)
{
	return ((key_flags >> flag) & 1);
}
/************* end of key-related functions *************/
