/* dos emulator, Matthias Lautner */
#define TERMIO_C 1
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/02/16 00:21:29 $
 * $Source: /usr/src/dos/RCS/termio.c,v $
 * $Revision: 1.17 $
 * $State: Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
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
#include <sys/stat.h>
#include <linux/vt.h>
#include <linux/kd.h>

#include "emu.h"
#include "termio.h"
#include "dosvga.h"

/* these are the structures in keymaps.c */
extern unsigned char shift_map[97],
  alt_map[97],
  key_map[97],
  num_table[15]; 

/* flags from dos.c */
extern int console_video,
           console_keyb,
  	   vga;

extern int sizes;  /* this is DEBUGGING code */
int in_readkeyboard=0;

unsigned char old_modecr,
         new_modecr;  /* VGA mode control register */
unsigned short int vga_start;  /* start of video mem */

extern int ignore_segv;
struct sigaction sa;

#define SETSIG(sig, fun)	sa.sa_handler = fun; \
				sa.sa_flags = 0; \
				sa.sa_mask = 0; \
				sigaction(sig, &sa, NULL);

unsigned int convscanKey(unsigned char);
unsigned int queue;

#define KBUFLEN 16

#ifdef OLD_KEYBUFFER
static unsigned short Kbuffer[KBUFLEN];
static int Kbuff_next_free = 0, Kbuff_next_avail = 0;
#else
unsigned short *Kbuffer=KBDA_ADDR;
static int Kbuff_next_free = 0, Kbuff_next_avail = 0;
#endif

int lastscan=0;

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
int altchar=0;

int vt_allow,      /* allow VC switches now? */
    vt_requested;  /* was one requested in a disallowed state? */

/* the file descriptor for /dev/mem when mmap'ing the video mem */
int mem_fd=-1; 
int video_ram_mapped=0;   /* flag for whether the video ram is mapped */

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

#define us unsigned short

int kbd_fd=-1,		/* the fd for the keyboard */
    old_kbd_flags,      /* flags for STDIN before our fcntl */
    console_no;		/* if console_keyb, the number of the console */

int kbcount;
unsigned char kbbuf[50], *kbp, erasekey;
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

/* this is DEBUGGING code! */
int   li2, co2;

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

/* this might need changing per country, like the RAW keyboards, but I
 * don't think so.  I think that it'll make every keyboard look like
 * a U.S. keyboard to DOS, which maybe "keyb" does anyway.  Sorry
 * it's so ugly.
 */

/* this is a table of scancodes, indexed by the ASCII value of the character
 * to be completed */

unsigned char highscan[256] = {
0,0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0xe,0x0f,0x24,0x25,0x2e,0x1c,  /* 0-0xd */
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

int outc(int c)
{
  write(STDERR_FILENO, (char *)&c, 1);
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
    /* this is DEBUGGING code! */
    if (sizes)
      {
	warn("using real found screen sizes!\n");
	co2 = co;
	li2 = li;
      } else {
	v_printf("using 80x25 screen size no matter what\n");
	co2=80;
	li2=25;
      }
    v_printf("SCREEN SIZE----co: %d, li: %d, CO: %d, LI: %d\n",
	     co, li, co2, li2);
  }
  if(tgetent(termcap, getenv("TERM")) != 1)
  {
    error("ERROR: no termcap \n");
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
    error("ERROR: unknown window sizes \n");
    leavedos(1);
  }
  for (fkp=funkey; fkp < &funkey[FUNKEYS]; fkp++) {
	fkp->esc = tgetstr(fkp->tce, &tp);
	if (!fkp->esc) error("ERROR: can't get termcap %s\n", fkp->tce);
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
	      error("ERROR: STDIN not a console-can't do console modes!\n");
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

#ifdef MDA_VIDEO
	    dbug_printf("video = MDA\n");
#endif

	    set_console_video();
	    ioctl(kbd_fd, VT_ACTIVATE, other_no);
	    ioctl(kbd_fd, VT_ACTIVATE, console_no);
	  }
	dbug_printf("$Header: /usr/src/dos/RCS/termio.c,v 1.17 1993/02/16 00:21:29 root Exp $\n");
	return 0;
}

forbid_switch()
{
  vt_allow=0;
}

allow_switch()
{
  /* v_printf("allow_switch called\n"); */
  vt_allow=1;
  if (vt_requested)
    {
      v_printf("clearing old vt request\n");
      release_vt(0);
    }
  /* else v_printf("allow_switch finished\n"); */
}
      
void acquire_vt(int sig)
{
  int kb_mode;

  SETSIG(SIG_ACQUIRE, acquire_vt);

  /* v_printf("acquire_vt() called!\n"); */
  if (do_ioctl(kbd_fd, VT_RELDISP, VT_ACKACQ)) /* switch acknowledged */
    v_printf("VT_RELDISP failed (or was queued)!\n");

  if (console_video)
    {
      get_video_ram(WAIT /*NOWAIT*/);
      if (vga) set_dos_video();
    }
}

set_dos_video()
{
  /* turn blinking attr bit -> a background color bit */

  v_printf("Setting DOS video: gfx_mode: %d modecr = 0x%x\n", gfx_mode,
	   new_modecr);

  if (ioperm(0x3da,1,1) || ioperm(0x3c0,1,1) || ioperm(0x3d4,2,1))
    {
      error("ERROR: Can't get I/O permissions!\n");
      return (-1);
    }

  port_in(0x3da);
  port_out(0x10+32, 0x3c0);
  port_out(new_modecr, 0x3c0); 

  ioperm(0x3da,1,0);
  ioperm(0x3c0,1,0);
  ioperm(0x3d4,2,0);

#ifdef EXPERIMENTAL_GFX
  if (gfx_mode != TEXT)
    {
      v_printf("VGA setmodeing gfx to %d\n", gfx_mode);      
      vga_setmode(gfx_mode); 
    }
#endif
}

set_linux_video()
{
  /* return vga card to Linux normal setup */

  /* v_printf("Setting Linux video: modecr = 0x%x\n", old_modecr); */

  if (ioperm(0x3da,1,1) || ioperm(0x3c0,1,1) || ioperm(0x3d4,2,1))
    {
      error("ERROR: Can't get I/O permissions!\n");
      return (-1);
    }

  port_in(0x3da);
  port_out(0x10+32, 0x3c0);
  port_out(old_modecr, 0x3c0); 

  ioperm(0x3da,1,0);
  ioperm(0x3c0,1,0);
  ioperm(0x3d4,2,0);


#ifdef EXPERIMENTAL_GFX
  if (gfx_mode != TEXT)
    {
      v_printf("set_linux_video(): setmodeing back to TEXT\n");
      vga_setmode(TEXT); 
    }
#endif
}

void release_vt(int sig)
{
  SETSIG(SIG_RELEASE, release_vt);
  /* v_printf("release_vt() called!\n"); */

  if (! vt_allow)
    {
      v_printf("disallowed vt switch!\n");
      vt_requested=1;
      return;
    }

  if (console_video)
    {
      if (vga) set_linux_video();
      put_video_ram();
    }

  if (do_ioctl(kbd_fd, VT_RELDISP, 1))       /* switch ok by me */
    v_printf("VT_RELDISP failed!\n");
  else
    v_printf("VT_RELDISP succeeded!\n");
}

get_video_ram(int waitflag)
{
  char *graph_mem;
  void (*oldhandler)();
  int tmp;

#if 1
  console_video=0;
  if (waitflag == WAIT)
    {
      v_printf("get_video_ram WAITING\n");
      /* wait until our console is current */
      oldhandler=signal(SIG_ACQUIRE, SIG_IGN);
      do
	{
	  if (tmp=do_ioctl(kbd_fd, VT_WAITACTIVE, console_no) < 0)
	    printf("WAITACTIVE gave %d. errno: %d\n", tmp,errno);
	  else break;
	} while (errno == EINTR);
  SETSIG(SIG_ACQUIRE, acquire_vt);
    }
  console_video=1;
#endif

  if (gfx_mode == TEXT)
    {
      if (video_ram_mapped)
	memcpy((caddr_t)SCRN_BUF_ADDR, (caddr_t)VIRT_TEXT_BASE, TEXT_SIZE);

      v_printf("non-gfx mode get_video_mem\n");
      graph_mem = (char *)mmap((caddr_t)VIRT_TEXT_BASE, 
			       TEXT_SIZE,
			       PROT_READ|PROT_WRITE,
			       MAP_SHARED|MAP_FIXED,
			       mem_fd, 
			       PHYS_TEXT_BASE);
  
      if ((long)graph_mem < 0) {
	error("ERROR: mmap error in get_video_ram (text)\n");
	return (1);
      }
      else v_printf("CONSOLE VIDEO address: 0x%x 0x%x 0x%x\n", graph_mem,
		    PHYS_TEXT_BASE, VIRT_TEXT_BASE);
      if (video_ram_mapped)
	memcpy((caddr_t)VIRT_TEXT_BASE, (caddr_t)SCRN_BUF_ADDR, TEXT_SIZE);
      else video_ram_mapped=1;
    }
  else /* gfx_mode */ 
    {
#ifdef EXPERIMENTAL_GFX
      v_printf("gfx mode get_video_ram\n");

      /* memcpy((caddr_t)SCRN_BUF_ADDR, (caddr_t)GRAPH_BASE, GRAPH_SIZE); */

      v_printf("non-gfx mode get_video_mem\n");
      graph_mem = (char *)mmap((caddr_t)GRAPH_BASE, 
			       GRAPH_SIZE,
			       PROT_READ|PROT_WRITE,
			       MAP_SHARED|MAP_FIXED,
			       mem_fd, 
			       GRAPH_BASE);
  
      if ((long)graph_mem < 0) {
	error("ERROR: mmap error in get_video_ram (gfx)\n");
	return (1);
      }
      else v_printf("CONSOLE VGA address: 0x%x 0x%x\n", graph_mem,
		    GRAPH_BASE);

      /* memcpy((caddr_t)GRAPH_BASE, (caddr_t)SCRN_BUF_ADDR, GRAPH_SIZE); */
      /* video_ram_mapped=1; */
#else
      error("graphics get_video_ram() without gfx support compiled in!\n");
#endif
    }
}

put_video_ram()
{
  if (gfx_mode == TEXT)
    {
      v_printf("put_video_ram (text mode) called\n"); 
      memcpy((caddr_t)SCRN_BUF_ADDR, (caddr_t)VIRT_TEXT_BASE, TEXT_SIZE);
      munmap((caddr_t)VIRT_TEXT_BASE, TEXT_SIZE);
      memcpy((caddr_t)VIRT_TEXT_BASE, (caddr_t)SCRN_BUF_ADDR, TEXT_SIZE);
    }
  else 
    {
#ifdef EXPERIMENTAL_GFX
      v_printf("put_video_ram (gfx mode) called\n");
      munmap((caddr_t)GRAPH_BASE, GRAPH_SIZE);
      v_printf("put_video_ram (gfx mode) finished\n");
#else
      error("graphics put_video_ram() w/out gfx support compiled in!\n");
#endif
    }
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

  if (do_ioctl(kbd_fd, VT_SETMODE, (int)&vt_mode))
    v_printf("initial VT_SETMODE failed!\n");
}

clear_process_control()
{
 struct vt_mode vt_mode;

 vt_mode.mode = VT_AUTO;
 do_ioctl(kbd_fd, VT_SETMODE, (int)&vt_mode);
 SETSIG(SIG_RELEASE, SIG_IGN);
 SETSIG(SIG_ACQUIRE, SIG_IGN);
}

open_kmem()
{
    if ((mem_fd = open("/dev/mem", O_RDWR) ) < 0) {
	error("ERROR: can't open /dev/mem \n");
	return (-1);
    }
}

set_console_video()
{
    int i;

    /* clear Linux's (unmapped) screen */
    tputs(cl, 1, outc);
    v_printf("set_console_video called\n");
   
    forbid_switch();
    get_video_ram(WAIT);

    if (vga)
      {
	int permtest;

	permtest = ioperm(0x3d4, 2, 1);  /* get 0x3d4 and 0x3d5 */
	permtest |= ioperm(0x3da, 1, 1);
	permtest |= ioperm(0x3c0, 2, 1);  /* get 0x3c0 and 0x3c1 */

	if (permtest)
	  {
	    error("ERROR: can't get I/O permissions: vga disabled!\n");
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
	    
	    v_printf("MODECR: old=0x%x...ORIG: 0x%x\n",
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
  v_printf("clear_console_video called\n");
  if (vga)
    {
#if 0
      int tmp=gfx_mode;
      gfx_mode=TEXT;
      set_linux_video();
      gfx_mode=tmp;
      if (gfx_mode != TEXT)
	{
	  v_printf("clear_console_video restoring screen...\n");
	  vga_setmode(TEXT);
	}
#else
      set_linux_video();
#endif
    }
  put_video_ram();		/* unmap the screen */

  show_cursor();		/* restore the cursor */
}

clear_raw_mode()
{
 do_ioctl(kbd_fd, KDSKBMODE, K_XLATE);
}

set_raw_mode()
{
  k_printf("Setting keyboard to RAW mode\n");
  if (!console_video) fprintf(stderr, "\nEntering RAW mode for DOS!\n");
  do_ioctl(kbd_fd, KDSKBMODE, K_RAW);
}


void map_bios(void)
{
  char *video_bios_mem, *system_bios_mem;

  g_printf("map_bios called\n");

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
    error("ERROR: mmap error in map_bios\n");
    return;
  }
  else g_printf("VIDEO BIOS address: 0x%x\n", video_bios_mem);

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
    error("ERROR: mmap error in map_bios\n");
    return;
  }
  else g_printf("SYSTEM BIOS address: 0x%x\n", system_bios_mem);
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
	int tmp;

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

	if (kbcount == 0) return 0;

	if (console_keyb)
	  {
#ifdef CHECK_RAW
	    do_ioctl(kbd_fd, KDGKBMODE, &kbd_mode);   /* get kb mode */
	    if (kbd_mode == K_RAW)
	      {
#endif
		unsigned char scancode = *kbp & 0xff;
		unsigned int tmpcode = 0;
	
		lastscan=scancode;
		tmpcode = convscanKey(scancode);
		kbp++;
		kbcount--;
		return tmpcode;
#ifdef CHECK_RAW
	      }
	    else goto xlate;
#endif
	  }

	xlate:
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
		k_printf("latin1 extended keysensing\n");
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

/* InsKeyboard
   returns 1 if a character could be inserted into Kbuffer
   */
int InsKeyboard (unsigned short scancode)
{
	int n;

	/* read the BDA pointers */
	Kbuff_next_avail = *(unsigned short *)0x41a - 0x1e;
	Kbuff_next_free = *(unsigned short *)0x41c - 0x1e;

        n = (Kbuff_next_free+1) % KBUFLEN;
	if (n == Kbuff_next_avail)
		return 0;
	Kbuffer[Kbuff_next_free] = scancode;
	Kbuff_next_free = n;

	/* these are the offsets from 0x400 to the head & tail */
	*(unsigned short *)0x41a = 0x1e + Kbuff_next_avail;
	*(unsigned short *)0x41c = 0x1e + Kbuff_next_free;

	dump_kbuffer();
	return 1;
}

dump_kbuffer()
{
  int i;
  
  k_printf("KEYBUFFER DUMP: 0x%02x 0x%02x\n", 
	   *(us *)0x41a-0x1e, *(us *)0x41c-0x1e);
  for (i=0;i<16;i++)
    k_printf("%04x ", Kbuffer[i]);
  k_printf("\n");
}

void keybuf_clear(void)
{
  Kbuff_next_free = Kbuff_next_free = 0;
  *(unsigned short *)0x41a = 0x1e + Kbuff_next_avail;
  *(unsigned short *)0x41c = 0x1e + Kbuff_next_free;
  dump_kbuffer();
}

/* PollKeyboard
   returns 1 if a character was found at poll
   */
int PollKeyboard (void)
{
  unsigned int key;
  int count=0;

  if (in_readkeyboard) error("ERROR: Polling while in_readkeyboard!!!!!\n");

  if (ReadKeyboard(&key, POLL))
  {
    k_printf("found key in PollKeyboard: 0x%04x\n", key);
    if (key == 0)
      {
	k_printf("Snatched scancode from me!\n");
      }
    else
      {
	if (! InsKeyboard(key)) error("PollKeyboard could not insert key!\n");
	count++;
      }
  }
  if (count) return 1;
  else return 0;
}

/* ReadKeyboard
   returns 1 if a character could be read in buf 
   */
int ReadKeyboard(unsigned int *buf, int wait)
{
	fd_set fds;
	int r;
	static unsigned int aktkey;

	/* this if for PollKeyboard...it works like NOWAIT, except
         * that the KeyBuffer is not to be consulted first (will cause
         * an infinite loop, as PollKeyboard stuffs things into the 
	 * buffer. This should be perhaps broken into 2 functions,
         * one of which does the key-grabbing, and one of which calls
         * the former and consults the buffer, too.
         */

	in_readkeyboard=1;

	/* read the BDA pointers */
	Kbuff_next_avail = *(unsigned short *)0x41a - 0x1e;
	Kbuff_next_free = *(unsigned short *)0x41c - 0x1e;

	if ((wait != POLL) && (Kbuff_next_free != Kbuff_next_avail))
	{
	  *buf = (int) (Kbuffer[Kbuff_next_avail]);
	  if (wait != TEST) 
	      Kbuff_next_avail = (Kbuff_next_avail + 1) % KBUFLEN;

	  /* update the BDA pointers */
	  *(unsigned short *)0x41a = 0x1e + Kbuff_next_avail;
	  *(unsigned short *)0x41c = 0x1e + Kbuff_next_free;

	  in_readkeyboard=0;
	  return 1;
	}

	while (!aktkey) {
		if (kbcount == 0 && wait == WAIT) {
		        in_readkeyboard=1;
		        FD_ZERO(&fds);
			FD_SET(kbd_fd, &fds);
			r = select(kbd_fd+1, &fds, NULL, NULL, NULL);
		}
		getKeys();
		if (kbcount == 0 && wait != WAIT) 
		  {
		    in_readkeyboard=0;
		    return 0;
		  }
		aktkey = convKey();
		/* if console keyboard, lastscan is set in
		 * convKey()
		 */
		if (!console_keyb) lastscan = aktkey >> 8;
	}
	*buf = aktkey;
	if (wait != TEST) aktkey = 0;
	in_readkeyboard=0;
	return 1;
}


/* ReadString
   reads a string into a buffer
   buf[0] ... length of string
   buf +1 ... string
   */
void ReadString(int max, unsigned char *buf)
{
	unsigned char ch, *cp = buf +1, *ce = buf + max;
	unsigned int c;
	int tmp;

	for (;;) {
		if (ReadKeyboard(&c, WAIT) != 1) continue;
		c &= 0xff;   /* mask out scan code -> makes ASCII */
		/* I'm not entirely sure why Matthias did this */
		/* if ((unsigned)c >= 128) continue; */
		ch = (char)c;
		if (ch >= ' ' && /* ch <= '~' && */ cp < ce) {
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
		if (ch == 3) {
		  k_printf("READSTRING ctrl-c\n");
		  *cp = ch;
		  *buf = (cp - buf) -1; /* length of string */
		  dos_ctrlc();
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
		error("ERROR: can't open keyboard\n");
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
	key_table[scancode](scancode); 

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
	  {
	        clr_kbd_flag(KF_ALT);		

		/* this is for hold-alt-and-keypad entry method */
		if (altchar)
		  {
		    /* perhaps anding with 0xff is incorrect.  how about
		     * filtering out anything > 255?
		     */
		     put_queue(altchar&0xff);  /* just the ASCII */
		  }
		altchar=0;
	  }
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
  g_printf("Regs requested: SYSREQ\n");
  show_regs();
}

static void scroll(unsigned int sc)
{
  if (key_flag(KKF_E0))
    {
      k_printf("ctrl-break!\n");
      *(unsigned char *)0x471 = 0x80;  /* ctrl-break flag */
      *(us *)0x41a = 0x1e;	/* key buf start ofs */
      *(us *)0x41c = 0x1e;	/* key buf end ofs */
      *(us *)0x41e = 0;		/* put 0 word in buffer */
      Kbuff_next_free = Kbuff_next_avail;  /* clear our buffer */      
      do_int(0x1b);
      return;
    }
  else if (kbd_flag(KF_CTRL))
      show_ints(0x30);
  else if (kbd_flag(KF_ALT))
    show_regs();
  else if (kbd_flag(KF_RSHIFT))
    {
      warn("timer int 8 requested...\n");
      do_int(8);
    }
  else if (kbd_flag(KF_LSHIFT))
    {
      warn("keyboard int 9 requested...\n");
      dump_kbuffer();
      do_int(9);
    }
  else {
    chg_kbd_flag(KF_SCRLOCK);
    set_leds();
  }
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

  do_ioctl(kbd_fd, KDSETLED, led_state);
}

get_leds()
{
  unsigned int led_state=0;

  do_ioctl(kbd_fd, KDGETLED, (int)&led_state);

  if  (led_state & (1 << LED_SCRLOCK)) set_kbd_flag(KF_SCRLOCK);
       else clr_kbd_flag(KF_SCRLOCK);
  if  (led_state & (1 << LED_NUMLOCK)) set_kbd_flag(KF_NUMLOCK);
       else clr_kbd_flag(KF_NUMLOCK);
  if  (led_state & (1 << LED_CAPSLOCK)) set_kbd_flag(KF_CAPSLOCK);
       else clr_kbd_flag(KF_CAPSLOCK);
}

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

	if (kbd_flag(KF_CTRL) || kbd_flag(KF_CAPSLOCK))
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

/* 0x47-0x53, indexed by sc-0x47 , goes like:
 * home, up, pgup, kp -, left, kp 5, right, kp +, end, down, pgdn, ins, del
 */
unsigned short ctrl_cursor[] = {
  0x7700, 0x8d00, 0x8400, 0x8e00, 0x7300, 0x8f00,0x7400,
  0, 0x7500, 0x9100, 0x7600, 0x9200, 0x9300
  };

unsigned short alt_cursor[] = {
  0x9700,0x9800,0x9900,0x4a00,0x9b00,0,0x9d00,0x4e00,0x9f00,0xa000,
  0xa100,0xa200,0xa300
  };


static void cursor(unsigned int sc)
{
  int old_sc;
 
  old_sc = sc;

  if (sc < 0x47 || sc > 0x53)
    return;

  /* do dos_ctrl_alt_del on C-A-Del and C-A-PGUP */
  if (kbd_flag(KF_CTRL) && kbd_flag(KF_ALT)) {
    if (sc == 0x53 /*del*/ || sc == 0x49 /*pgup*/)
      dos_ctrl_alt_del();
    if (sc == 0x51) /*pgdn*/
      {
	dbug_printf("ctrl-alt-pgdn\n");
	leavedos(1);
      }
  }

  sc -= 0x47;

  /* ENHANCED CURSOR KEYS:  only ctrl and alt may modify them.
   */
  if (key_flag(KKF_E0)) {
    if (kbd_flag(KF_ALT))
      put_queue(alt_cursor[sc]);
    else if (kbd_flag(KF_CTRL))
      put_queue(ctrl_cursor[sc]);
    else put_queue(old_sc << 8);
    return;
  }

/* everything below this must be a keypad key, as the check for KKF_E0
 * above filters out enhanced cursor (gray) keys 
 */

/* this is the hold-alt-and-type numbers thing */  
  if (kbd_flag(KF_ALT))
    {
      int digit;

      if ((digit=num_table[sc]-'0') <= 9)      /* is a number */
	  altchar = altchar*10 + digit;
    }
  else if (kbd_flag(KF_CTRL))
    put_queue(ctrl_cursor[sc]);
  else if (kbd_flag(KF_NUMLOCK) || kbd_flag(KF_LSHIFT)
      || kbd_flag(KF_RSHIFT)) 
    put_queue(num_table[sc]);
  else
    put_queue(old_sc << 8);
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

      /* can't just do the ioctl() here, as PollKeyboard will probably have
       * been called from a signal handler, and ioctl() is not reentrant.
       * hence the delay until out of the signal handler...
       */
      activate(sc-0x3a);

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

int activate(int con_num)
{
  if (in_ioctl)
    {
      k_printf("can't ioctl for activate, in a signal handler\n");
      /* queue_ioctl(kbd_fd, VT_ACTIVATE, con_num); */
      do_ioctl(kbd_fd, VT_ACTIVATE, con_num); 
    }
  else 
      do_ioctl(kbd_fd, VT_ACTIVATE, con_num); 
}

int do_ioctl(int fd, int req, int param3)
{
  int tmp;

  if (in_sighandler && in_ioctl)
    {
      k_printf("do_ioctl(): in ioctl %d 0x%04x 0x%04x.\nqueuing: %d 0x%04x 0x%04x\n",
	       curi.fd, curi.req, curi.param3, fd, req, param3);
      queue_ioctl(fd,req,param3);
      errno=EDEADLOCK;
#ifdef SYNC_ALOT
      fflush(stdout);
      sync();  /* for safety */
#endif
      return -1;
    }
  else
    {
      in_ioctl=1;
      curi.fd=fd;
      curi.req=req;
      curi.param3=param3;
      tmp=ioctl(fd,req,param3);
      in_ioctl=0;
      if (iq.queued)
	{
	  k_printf("detected queued ioctl in do_ioctl(): %d 0x%04x 0x%04x\n",
		   iq.fd, iq.req, iq.param3);
	}
      return tmp;
    }
}

int queue_ioctl(int fd, int req, int param3)
{
  if (iq.queued)
    {
      error("ioctl already queued: %d 0x%04x 0x%04x\n", iq.fd, iq.req,
	    iq.param3);
      return 1;
    }
  iq.fd=fd;
  iq.req=req;
  iq.param3=param3;
  iq.queued=1;

  return 0; /* success */
}

void do_queued_ioctl(void)
{
  if (iq.queued)
    {
      iq.queued=0;
      do_ioctl(iq.fd, iq.req, iq.param3);
    }
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
#undef TERMIO_C
