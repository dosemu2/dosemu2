/* dos emulator, Matthias Lautner */
#define TERMIO_C 1
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/11/30 21:26:44 $
 * $Source: /home/src/dosemu0.49pl3/RCS/termio.c,v $
 * $Revision: 1.5 $
 * $State: Exp $
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
#include <sys/stat.h>
#include <linux/vt.h>
#include <linux/kd.h>

#include "config.h"
#include "memory.h"
#include "emu.h"
#include "termio.h"
#include "dosvga.h"
#include "mouse.h"
#include "dosipc.h"

unsigned int convascii(int *);
/* Was a toggle key already port in'd */
u_char ins_stat=0, scroll_stat=0, num_stat=0, caps_stat=0;

/* these are the structures in keymaps.c */
extern unsigned char shift_map[97],
  alt_map[97],
  key_map[97],
  num_table[15]; 

extern struct config_info config;

extern struct screen_stat scr_state;   /* main screen status variables */

extern int sizes;  /* this is DEBUGGING code */
int in_readkeyboard=0;

extern int ignore_segv;
struct sigaction sa;

#define SETSIG(sig, fun)	sa.sa_handler = fun; \
				sa.sa_flags = 0; \
				sa.sa_mask = 0; \
				sigaction(sig, &sa, NULL);

unsigned int convscanKey(unsigned char);
unsigned int queue;

/* as this gives the length of the _BIOS_ keybuffer, you don't
 * want to change this...you risk overwriting important BIOS data
 */
#define KBUFLEN 15

/* unsigned short *Kbuffer=KBDA_ADDR; */
static int Kbuff_next_free = 0, Kbuff_next_avail = 0;

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
 unscroll(unsigned int),
 num(unsigned int),
 unnum(unsigned int),
 unins(unsigned int),
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

 void child_set_flags(int sc);

 void set_kbd_flag(int),
  clr_kbd_flag(int),
  chg_kbd_flag(int),
  child_set_kbd_flag(int),
  child_clr_kbd_flag(int),
  set_key_flag(int),
  clr_key_flag(int),
  chg_key_flag(int);

 int kbd_flag(int),
     child_kbd_flag(int),
     key_flag(int);


/* initialize these in OpenKeyboard! */
unsigned int child_kbd_flags = 0;
unsigned int kbd_flags = 0;
unsigned int key_flags = 0;
int altchar=0;

/* the file descriptor for /dev/mem when mmap'ing the video mem */
int mem_fd=-1; 

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
	none,unnum,unscroll,none,			/* C4-C7 br br br br */
	none,none,none,none,			/* C8-CB br br br br */
	none,none,none,none,			/* CC-CF br br br br */
	none,none,unins,none,			/* D0-D3 br br unins br */
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
    ioc_fd=-1,          /* the dup'd fd for ioctl()'s */
    old_kbd_flags;      /* flags for STDIN before our fcntl */


/* these are in DOSIPC.C */
extern int ipc_fd[2];

int kbcount=0;
unsigned char kbbuf[KBBUF_SIZE], *kbp, erasekey;
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
     *ve,       /* return cursor to normal */
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
	warn("using real found screen sizes: %d x %d!\n", co, li);
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
    v_printf("TERMCAP screen size: %d x %d\n", co, li);
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

static void 
CloseKeyboard(void)
{
  if (kbd_fd != -1)
    {
      if (config.console_keyb)
	clear_raw_mode();
      if (config.console_video)
	clear_console_video();

      if (config.console_keyb || config.console_video)
	clear_process_control();

      fcntl(kbd_fd, F_SETFL, old_kbd_flags);
      ioctl(kbd_fd, TCSETAF, &oldtermio);

      close(kbd_fd);
      kbd_fd = -1;
    }
}

static int 
OpenKeyboard(void)
{
	struct termio	newtermio;	/* new terminal modes */
	struct stat chkbuf;
	unsigned int i;
	int major,minor;

	kbd_fd = dup(STDIN_FILENO);
	ioc_fd = dup(STDIN_FILENO);

	old_kbd_flags = fcntl(kbd_fd, F_GETFL);
	fcntl(kbd_fd, F_SETFL, O_RDONLY | O_NONBLOCK);
	fcntl(ioc_fd, F_SETFL, O_WRONLY | O_NONBLOCK);

	if (kbd_fd < 0)
		return -1;

	scr_state.vt_allow=0;
	scr_state.vt_requested=0;
	scr_state.mapped=0;
	scr_state.pageno=0;
	scr_state.virt_address=PAGE_ADDR(0);
	
	fstat(kbd_fd, &chkbuf);
	major=chkbuf.st_rdev >> 8;
	minor=chkbuf.st_rdev & 0xff;

	/* console major num is 4, minor 64 is the first serial line */
	if ((major == 4) && (minor < 64))
	    scr_state.console_no=minor;  /* get minor number */
	else
	  {
	    if (config.console_keyb || config.console_video) 
	      error("ERROR: STDIN not a console-can't do console modes!\n");
	    scr_state.console_no=0;
	    config.console_keyb=0;
	    config.console_video=0;
	    config.vga=0;
	    config.graphics=0;
	    if (config.speaker == SPKR_NATIVE) config.speaker=SPKR_EMULATED;
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

	if (config.console_keyb || config.console_video)
	  set_process_control();

	if (config.console_keyb)
	  {
	    set_raw_mode();
	    kbd_flags=0;
	    child_kbd_flags=0;
	    get_leds(); 
	    key_flags=0;
	    *KEYFLAG_ADDR=0;
	    set_key_flag(KKF_KBD102);
	  }

	if (config.console_video)
	    set_console_video();

	dbug_printf("$Header: /home/src/dosemu0.49pl3/RCS/termio.c,v 1.5 1993/11/30 21:26:44 root Exp root $\n");

	return 0;
}

clear_raw_mode()
{
 do_ioctl(ioc_fd, KDSKBMODE, K_XLATE);
}

set_raw_mode()
{
  k_printf("Setting keyboard to RAW mode\n");
  if (!config.console_video) fprintf(stderr, "\nEntering RAW mode for DOS!\n");
  do_ioctl(ioc_fd, KDSKBMODE, K_RAW);
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
  int cc;
  int tmp;

  if (config.console_keyb) {
    kbcount=0;
  }

  if (kbcount == 0) {
    kbp = kbbuf;
  } else if (kbp > &kbbuf[(KBBUF_SIZE * 3) / 5]) {
    memmove(kbbuf, kbp, kbcount);
    kbp = kbbuf;
  }

  /* IPC change here!...was read(kbd_fd... */
  cc = read(kbd_fd, &kbp[0 + kbcount], KBBUF_SIZE);

k_printf(" cc found %d characters\n", cc);

#if AJT
  if (cc>0 && config.console_keyb)
    {
      int i;
      for (i=0;i<cc;i++){
	child_set_flags(kbp[kbcount+i]);
	parent_setscan(kbp[kbcount+i]);
      }
    }
  else 
  {
    if (cc > 0) {
      if (kbp+kbcount+cc > &kbbuf[KBBUF_SIZE])
        error("ERROR: getKeys() has overwritten the buffer!\n");
      kbcount += cc;
    }
    while (cc) {
  	  convascii(&cc);
    }
  }
#endif
  
}

void child_set_flags(int sc) {
	switch (sc) {
	case 0xe0:
	case 0xe1:
		child_set_kbd_flag(4);
		return;
	case 0x2a:
		if (child_kbd_flag(4))
			child_clr_kbd_flag(4);
		else
			child_set_kbd_flag(1);
		return;
	case 0x36:
		child_set_kbd_flag(1);
		return;
	case 0x1d:
	case 0x10:
		child_set_kbd_flag(2);
		return;
	case 0x38:
		if (!child_kbd_flag(4))
		  child_set_kbd_flag(3);
		return;
	case 0xaa:
		if (child_kbd_flag(4))
			child_clr_kbd_flag(4);
		else
			child_clr_kbd_flag(1);
		return;
	case 0xb6:
		child_clr_kbd_flag(1);
		return;
	case 0x9d:
	case 0x90:
		child_clr_kbd_flag(2);
		return;
	case 0xb8:
		child_clr_kbd_flag(3);
		child_clr_kbd_flag(4);
		return;
	case 0x3b:
	case 0x3c:
	case 0x3d:
	case 0x3e:
	case 0x3f:
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x57:
	case 0x58:
		if (
			 child_kbd_flag(3) &&
			!child_kbd_flag(2) &&
			!child_kbd_flag(1) 
		   ) {
		  int fnum=sc-0x3a;
		  k_printf("Doing VC switch\n");
		  if (fnum > 10) fnum -= 0x12;   /* adjust if f11 or f12 */

/* can't just do the ioctl() here, as PollKeyboard will probably have
 * been called from a signal handler, and ioctl() is not reentrant.
 * hence the delay until out of the signal handler...
 */
		  activate(fnum);
		  return;
		}
		return;
	case 0x51:
		if (
			 child_kbd_flag(2) &&
			 child_kbd_flag(3) &&
			!child_kbd_flag(1)
		   ) {
		  dbug_printf("ctrl-alt-pgdn\n");
		  ipc_wakeparent();
		  ipc_send2parent(DMSG_EXIT);
		}
		return;
	default:
		child_clr_kbd_flag(4);
		return;
	}
}

unsigned int convascii(int *cc) {

/* get here only if in cooked mode (i.e. K_XLATE) */
	int i;
	struct timeval scr_tv;
	struct funkeystruct *fkp;
	fd_set fds;
	if (*kbp == '\033') {
		int ccc;
		in_readkeyboard=1;
		if (kbcount == 1) {
		char contin;
		do {
			contin=kbcount;
			scr_tv.tv_sec = 0;
			scr_tv.tv_usec = 100000;
			FD_ZERO(&fds);

			/* IPC change here! */
			FD_SET(kbd_fd, &fds);
			RPT_SYSCALL( select(kbd_fd+1, &fds, NULL, NULL,
					   &scr_tv) );
			ccc = read(kbd_fd, &kbp[0 + kbcount], KBBUF_SIZE - kbcount);
			if (ccc > 0) {
			  kbcount += ccc;
			  *cc += ccc;
			}
		} while (contin!=kbcount);

		if (kbcount == 1) {
			kbcount--;
			(*cc)--; 
                        parent_setscan((highscan[*kbp] << 8 ) +
                                     (unsigned char)*kbp++);
			return;
		}

	}
#define LATIN1 1
#define METAKEY 1
#ifdef LATIN1
	k_printf("latin1 extended keysensing\n");
	if (islower((unsigned char)kbp[1])) {
		kbcount -= 2;
		*cc -= 2;
		kbp++;
		parent_setscan( alt_keys[*kbp++ - 'a']);
		return;
	} else if (isdigit((unsigned char)kbp[1])) {
		kbcount -= 2;
		*cc -= 2;
		kbp++;
		parent_setscan( alt_nums[*kbp++ - '0']);
		return;
	}
#endif
	fkp = funkey;

	for (i=1;;) {
	  if (fkp->esc == NULL || 
	      (unsigned char)fkp->esc[i] < kbp[i]) {
	    if (++fkp >= &funkey[FUNKEYS])
	      break;
	  } else if ((unsigned char)fkp->esc[i] == kbp[i]) {
	    if (fkp->esc[++i] == '\0') {
	      kbcount -= i;
	      *cc -= i;
	      kbp += i;
	      parent_setscan( fkp->code);
	      return;
	    }
	    if (kbcount <= i) {
	      char contin;
	      do {
			contin=kbcount;
			scr_tv.tv_sec = 0;
			scr_tv.tv_usec = 100000;
			FD_ZERO(&fds);

			/* IPC change here! */
			FD_SET(kbd_fd, &fds);
			RPT_SYSCALL( select(kbd_fd+1, &fds, NULL, NULL,
					   &scr_tv) );
			ccc = read(kbd_fd, &kbp[0 + kbcount], KBBUF_SIZE - kbcount);
			if (ccc > 0) {
			  kbcount += ccc;
			  *cc += ccc;
			}
	      } while (contin!=kbcount);
	      if (kbcount <= i) {
		break;
	      }
	    }
	  } else {
	    break;
	  }
	}
	in_readkeyboard=0;
	/* end of if (*kbp == '\033')... */
	} else if (*kbp == erasekey) {
		kbcount--;
	        (*cc)--;
		kbp++;
		parent_setscan(((unsigned char)highscan[8] << 8)  + (unsigned char)8);
		return;
#ifdef METAKEY
/* #ifndef LATIN1 */
	} else if ((unsigned char)*kbp >= ('a'|0x80) && 
		   (unsigned char)*kbp <= ('z'|0x80)) {
		kbcount--;
	        (*cc)--;
		parent_setscan((unsigned short int)alt_keys[*kbp++ - ('a'|0x80)]);
		return;
	} else if ((unsigned char)*kbp >= ('0'|0x80) && 
		   (unsigned char)*kbp <= ('9'|0x80)) {
		kbcount--;
	        (*cc)--;
		parent_setscan( alt_nums[(unsigned char)*kbp++ - ('0'|0x80)]);
		return;
	#endif
	}


	i=highscan[*kbp] << 8;   /* get scancode */

/* extended scancodes return 0 for the ascii value */
        if ((unsigned char)*kbp < 0x80) i |= (unsigned char)*kbp;

	parent_setscan(i); 

	kbcount--;
	(*cc)--;
	kbp++;
}

/* InsKeyboard
   returns 1 if a character could be inserted into Kbuffer
   */
int InsKeyboard (unsigned short scancode)
{
	int n;
	unsigned short *Kbuffer = KBDA_ADDR;

/* Some major hacking going on here, too many '/2' staments etc.. */

	/* read the BDA pointers */
	Kbuff_next_avail = *(unsigned short *)0x41a - 0x1e;
	Kbuff_next_free = *(unsigned short *)0x41c - 0x1e;

        n = ((Kbuff_next_free/2)+1) % (KBUFLEN+1);
	if (n == (Kbuff_next_avail/2))
		return 0;
	Kbuffer[Kbuff_next_free/2] = scancode;
	Kbuff_next_free = n*2;

	ignore_segv++;
	/* these are the offsets from 0x400 to the head & tail */
	*(unsigned short *)0x41a = 0x1e + Kbuff_next_avail;
	*(unsigned short *)0x41c = 0x1e + Kbuff_next_free;
	ignore_segv--;

	/* dump_kbuffer(); */
	return 1;
}

/* static */ unsigned int convKey(int scancode)
{
	int i;
	struct timeval scr_tv;
	struct funkeystruct *fkp;
	fd_set fds;
	unsigned int kbd_mode;

	k_printf("convKey = 0x%04x\n", scancode);

	if (scancode == 0) return 0;

	if ( config.console_keyb )
	  {
		unsigned int tmpcode = 0;
	
		lastscan=scancode;
		tmpcode = convscanKey(scancode);

		return tmpcode;
	  }
}

dump_kbuffer()
{
  int i;
  unsigned short *Kbuffer = KBDA_ADDR;
  
  k_printf("KEYBUFFER DUMP: 0x%02x 0x%02x\n", 
	   *(us *)0x41a-0x1e, *(us *)0x41c-0x1e);
  for (i=0;i<16;i++)
    k_printf("%04x ", Kbuffer[i]);
  k_printf("\n");
}

void keybuf_clear(void)
{
  ignore_segv++;

  Kbuff_next_free = Kbuff_next_avail = 0;
  *(unsigned short *)0x41a = 0x1e + Kbuff_next_avail;
  *(unsigned short *)0x41c = 0x1e + Kbuff_next_free;

  ignore_segv--;
  dump_kbuffer();
}

/* PollKeyboard
   returns 1 if a character was found at poll
   */
int PollKeyboard (void)
{
  unsigned int key;
  int count=0;

  if (in_readkeyboard) 
    {
      error("ERROR: Polling while in_readkeyboard!!!!!\n");
      return 0;
    }

  if (CReadKeyboard(&key, POLL))
  {
    if (key == 0) k_printf("Snatched scancode from me!\n");
    else
      {
	if (! InsKeyboard(key)) 
	  {
	    error("PollKeyboard could not insert key!\n");
	    outc('\007');  /* bell */
	  }
	count++;  /* whether or not the key is put into the buffer,
		   * throw it away :-( */
      }
  }
  if (count) return 1;
  else return 0;
}


int PollKeyboard2 (void)
{
  unsigned int key;
  int count=0;

  if (CReadKeyboard(&key, POLL))
  {
    if (key == 0) k_printf("Snatched scancode from me!\n");
    else
      {
	if (! InsKeyboard(key)) 
	  {
	    error("PollKeyboard could not insert key!\n");
	    outc('\007');  /* bell */
	  }
	count++;  /* whether or not the key is put into the buffer,
		   * throw it away :-( */
      }
  }
  if (count) return 1;
  else return 0;
}


int CReadKeyboard(unsigned int *buf, int wait)
{
	struct ipcpkt  pkt;
	unsigned short *Kbuffer=KBDA_ADDR;
	in_readkeyboard=1;


	/* XXX - need semaphores here to keep child process 
	 * out of keyboard buffer...
	 */

	/* read the BDA pointers */
	Kbuff_next_avail = *(unsigned short *)0x41a - 0x1e;
	Kbuff_next_free = *(unsigned short *)0x41c - 0x1e;

	if (Kbuff_next_free != Kbuff_next_avail)
	{
	  *buf = (int) (Kbuffer[Kbuff_next_avail/2]);
	  if (wait != TEST) 
	      Kbuff_next_avail = (((Kbuff_next_avail/2) + 1) % (KBUFLEN+1))*2; 

	  ignore_segv++;
	  /* update the BDA pointers */
	  *(unsigned short *)0x41a = 0x1e + Kbuff_next_avail;
	  *(unsigned short *)0x41c = 0x1e + Kbuff_next_free;
	  ignore_segv--;

	  in_readkeyboard=0;
	  return 1;
	}
	else if (wait == TEST || wait == NOWAIT) {
	  in_readkeyboard=0;
 	  return 0;
	}

	error("IPC/KBD: (par) sending request message\n");
	ipc_send2child(DMSG_READKEY);

	/* got here if no key in keybuffer and not TEST */
	error("IPC/KBD: (par) waiting for key\n");
	{
	  extern u_short sent_key;
	  /* loop until the DMSG_READKEY request has been answered */
	  sent_key=0;

  	  while (sent_key == 0)
	    usleep(100);
	  *buf = sent_key;
	  sent_key=0;
	  error("IPC/KBD: (par) got key (sent_key) 0x%04x\n", *buf);
	}
	in_readkeyboard=0;
	return 1;
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

	k_printf("Doing get keys\n");
	getKeys();
	k_printf("Returning from ReadKeyboard\n");
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
		if (CReadKeyboard(&c, WAIT) != 1) continue;
		c &= 0xff;   /* mask out scan code -> makes ASCII */
		/* I'm not entirely sure why Matthias did this */
		/* if ((unsigned)c >= 128) continue; */
		ch = (char)c;
		if (ch >= ' ' && /* ch <= '~' && */ cp < ce) {
			*cp++ = ch;
			char_out(ch, SCREEN, ADVANCE);
			continue;
		}
		if (ch == '\010' && cp > buf +1) { /* BS */
			cp--;
			char_out('\010', SCREEN, ADVANCE);
			char_out(' ', SCREEN, ADVANCE);
			char_out('\010', SCREEN, ADVANCE);
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
	tputs(cl, 1, outc);
}

/**************************************************************
 * this is copied verbatim from the Linux kernel (keyboard.c) *
 **************************************************************/

static unsigned char resetid=0;
static unsigned char firstid=0;

unsigned int convscanKey(unsigned char scancode)
{
 	static unsigned char rep = 0xff;

	k_printf("convscanKey scancode = 0x%04x\n", scancode);

	if (scancode == 0xe0){
		set_key_flag(KKF_E0);
		set_key_flag(KKF_FIRSTID);
		set_key_flag(KKF_READID);
		resetid=1;
		firstid=0;
	}
	else if (scancode == 0xe1){
		set_key_flag(KKF_E1);
		set_key_flag(KKF_FIRSTID);
		set_key_flag(KKF_READID);
		resetid=1;
		firstid=0;
	}

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
		resetid=0;
		firstid=0;
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

	k_printf("resetid = %d firstid = %d\n", resetid, firstid);
	if (resetid) {
		if (firstid) clr_key_flag(KKF_FIRSTID);
		clr_key_flag(KKF_READID);
		resetid=firstid=0;
	}

	clr_key_flag(KKF_E0);
	clr_key_flag(KKF_E1);

	return(queue);
}


static void ctrl(unsigned int sc)
{
  if (key_flag(KKF_E0)) {
    set_key_flag(KKF_RCTRL);
  }
  else
    set_kbd_flag(EKF_LCTRL);
  
  set_kbd_flag(KF_CTRL);
}

static void alt(unsigned int sc)
{
  if (key_flag(KKF_E0)) {
    set_key_flag(KKF_RALT);
  }
  else
    set_kbd_flag(EKF_LALT);
  
  set_kbd_flag(KF_ALT);
}

static void unctrl(unsigned int sc)
{
	if (key_flag(KKF_E0)) {
		clr_key_flag(KKF_RCTRL);
	      }
	else
		clr_kbd_flag(EKF_LCTRL);

	if ( !kbd_flag(EKF_LCTRL) && !key_flag(KKF_RCTRL) )
	        clr_kbd_flag(KF_CTRL);
}

static void unalt(unsigned int sc)
{
/*  if (!resetid) { */
	if (key_flag(KKF_E0)) {
		clr_key_flag(KKF_RALT);
	      }
	else 
		clr_kbd_flag(EKF_LALT);

	if (! (kbd_flag(EKF_LALT) && key_flag(KKF_RALT)) )
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
  /* } */
}

static void lshift(unsigned int sc)
{
	set_kbd_flag(KF_LSHIFT);
}

static void unlshift(unsigned int sc)
{
  if (!resetid) {
	clr_kbd_flag(KF_LSHIFT);
  }
}

static void rshift(unsigned int sc)
{
	set_kbd_flag(KF_RSHIFT);
}

static void unrshift(unsigned int sc)
{
  if (!resetid) {
	clr_kbd_flag(KF_RSHIFT);
  }
}

static void caps(unsigned int sc)
{
  if (kbd_flag(KKF_RCTRL) && kbd_flag(EKF_LCTRL))
    {
      keyboard_mouse = keyboard_mouse ? 0 : 1;
      m_printf("MOUSE: toggled keyboard mouse %s\n", 
	       keyboard_mouse ? "on" : "off");
      return;
    }
  else {
    set_kbd_flag(EKF_CAPSLOCK);
    if (!caps_stat) {
      chg_kbd_flag(KF_CAPSLOCK);	/* toggle; this means SET/UNSET */
      set_leds();
      caps_stat = 1;
    }
  }
}

static void uncaps(unsigned int sc)
{
  if (!resetid) {
    clr_kbd_flag(EKF_CAPSLOCK);
    caps_stat = 0;
  }
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
      ignore_segv++;
      *(unsigned char *)0x471 = 0x80;  /* ctrl-break flag */
      *(us *)0x41a = 0x1e;	/* key buf start ofs */
      *(us *)0x41c = 0x1e;	/* key buf end ofs */
      *(us *)0x41e = 0;		/* put 0 word in buffer */
      ignore_segv--;
      Kbuff_next_free = Kbuff_next_avail =0;  /* clear our buffer */

      ipc_send2parent(DMSG_CTRLBRK);
      ipc_wakeparent();

      return;
    }
  else if (kbd_flag(KKF_RCTRL))
    show_ints(0,0x33);
  else if (kbd_flag(KKF_RALT))
    show_regs();
  else if (kbd_flag(KF_RSHIFT))
    {
      warn("timer int 8 requested...\n");
      ipc_wakeparent();
      ipc_send2parent(DMSG_INT8);
    }
  else if (kbd_flag(KF_LSHIFT))
    {
      warn("keyboard int 9 requested...\n");
      dump_kbuffer();
      ipc_wakeparent();
      ipc_send2parent(DMSG_INT9);
    }
  else {
    set_kbd_flag(EKF_SCRLOCK);
    if (!scroll_stat) {
      chg_kbd_flag(KF_SCRLOCK);
      set_leds();
      scroll_stat=1;
    }
  }
}

static void unscroll(unsigned int sc){
  clr_kbd_flag(EKF_SCRLOCK);
  scroll_stat=0;
}

static void num(unsigned int sc)
{
  static int lastpause=0;

  if (kbd_flag(EKF_LCTRL)) {
    k_printf("PAUSE!\n");
    if (lastpause) {
      I_printf("IPC: waking parent up!\n");
      dos_unpause();
      lastpause=0;
    }
    else {
      I_printf("IPC: putting parent to sleep!\n");
      dos_pause();
      lastpause=1;
    }
  }
  else {
    set_kbd_flag(EKF_NUMLOCK);
    if (!num_stat) {
      k_printf("NUMLOCK!\n");
      chg_kbd_flag(KF_NUMLOCK);
      set_leds();
      num_stat=1;
    }
  }
}

static void unnum(unsigned int sc) {
  num_stat=0;
  clr_kbd_flag(EKF_NUMLOCK);
}

set_leds()
{
  unsigned int led_state=0;

  if (kbd_flag(KF_SCRLOCK)) {
	led_state |= (1 << LED_SCRLOCK);
	set_key_flag(KKF_SCRLOCK);
  }
  if (kbd_flag(KF_NUMLOCK)) {
	led_state |= (1 << LED_NUMLOCK);
	set_key_flag(KKF_NUMLOCK);
  }
  if (kbd_flag(KF_CAPSLOCK)) {
	led_state |= (1 << LED_CAPSLOCK);
	set_key_flag(KKF_CAPSLOCK);
  }

  do_ioctl(ioc_fd, KDSETLED, led_state);
}

get_leds()
{
  unsigned int led_state=0;

  do_ioctl(ioc_fd, KDGETLED, (int)&led_state);

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
          /* On a german keyboard the Left-Alt- (Alt-) and the Right-Alt-
             (Alt-Gr-) Keys are different. The keys pressed with AltGr
             return the codes defined in keymaps.c in the alt_map.
             Pressed with the Left-Alt-Key they return the normal symbol
             with the alt-modifier. I've tested this with the 4DOS-Alias-
             Command.                  hein@tlaloc.in.tu-clausthal.de       */
#ifdef KBD_GR_LATIN1                   /* Only valid for GR_LATIN1-keyboard */
	     if (kbd_flag(EKF_LALT))    /* Left-Alt-Key pressed ?            */
	       ch = alt_map[0];         /* Return Key with Alt-modifier      */
	     else                       /* otherwise (this is Alt-Gr)        */
	#endif                                 /* or no GR_LATIN1-keyboard          */
	      ch = alt_map[sc];        /* Return key from alt_map           */
	    if ((sc >= 2) && (sc <= 0xb))  /* numbers */
	      sc += 0x76;
	    else if (sc == 0xd) sc = 0x83;  /* = */
	    else if (sc == 0xc) sc = 0x82;  /* - */
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

unsigned short shift_cursor[] = {
  0x4737,0x4838,0x4939,0x4a2d,0x4b34,0x0000,0x4d36,0x4e2b,0x4f31,
  0x5032,0x5133,0x5230,0x532e
  };

static void unins(unsigned int sc) {
  ins_stat=0;
}

static void cursor(unsigned int sc)
{
  int old_sc;
 
  old_sc = sc;

  if (sc < 0x47 || sc > 0x53)
    return;

  /* do dos_ctrl_alt_del on C-A-Del and C-A-PGUP */
  if (kbd_flag(KF_CTRL) && kbd_flag(KF_ALT)) 
  {
    if (sc == 0x53 /*del*/ || sc == 0x49 /*pgup*/)
      dos_ctrl_alt_del();
    if (sc == 0x51) /*pgdn*/
      {
	leavedos(0);
      }
    /* if the arrow keys, or home end, do keyboard mouse */
  }

  if ((keyboard_mouse) && (sc == 0x50 || sc == 0x4b || sc == 0x48 || 
			   sc == 0x4d || sc == 0x47 || sc == 0x4f))
    {
      mouse_keyboard(sc);
      return;
    }

  if (sc == 0x52){
	if (!ins_stat) {
	  chg_kbd_flag(KF_INSERT);
	  ins_stat=1;
	}
	else return;
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
    put_queue((old_sc << 8) | num_table[sc]);
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
  int fnum=sc-0x3a;
  if (fnum > 10) fnum -= 0x12;   /* adjust if f11 or f12 */

  /* this checks for the VC-switch key sequence */
  if (kbd_flag(EKF_LALT) && !key_flag(KKF_RALT) && !kbd_flag(KF_RSHIFT)
      && !kbd_flag(KF_LSHIFT) && !kbd_flag(KF_CTRL))
    {
      clr_kbd_flag(EKF_LALT);
      if (!key_flag(KKF_RALT))
	clr_kbd_flag(KF_ALT); 

      return;
    }

/* FCH (Fkey CHoose):   returns a if n is f11 or f12, else it returns b
 * PC scancodes for fkeys are "orthogonal" except F11 and F12.
 */

#define FCH(n,a,b) ((n <= 10) ? a : b)

  if (kbd_flag(KF_LSHIFT) || kbd_flag(KF_RSHIFT))
    put_queue( (sc + FCH(fnum,0x19,0x30)) << 8);

  else if (kbd_flag(KF_CTRL))
    put_queue( (sc + FCH(fnum,0x23,0x32)) << 8);

  else if (kbd_flag(KF_ALT))
    put_queue( (sc + FCH(fnum,0x2d,0x34)) << 8);

  else
    put_queue( FCH(fnum,sc,sc+0x2e) << 8);
}


int activate(int con_num)
{
  if (in_ioctl)
    {
      k_printf("can't ioctl for activate, in a signal handler\n");
      do_ioctl(ioc_fd, VT_ACTIVATE, con_num); 
    }
  else 
      do_ioctl(ioc_fd, VT_ACTIVATE, con_num); 
}


int 
do_ioctl(int fd, int req, int param3)
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
   /* XXX - we ignore any changes a user program has made...this
    *       isn't good.  however, we can't let users mess around
    *       with our ALT flags, as we use those to change consoles.
    *       should change later to allow DOS changes of anything but
    *       RALT...
    */

   *(KBDFLAG_ADDR) = kbd_flags; 
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

/* These are added to allow the CHILD process to keep its own flags on keyboard
   status */

 void child_set_kbd_flag(int flag)
{
	child_kbd_flags |= 1 << flag;
}

 void child_clr_kbd_flag(int flag)
{
	child_kbd_flags &= ~(1 << flag);
}

 int child_kbd_flag(int flag)
{
	return ((child_kbd_flags >> flag) & 1);
}

/* these are the KEY flags */
 void set_key_flag(int flag)
{
	key_flags |= (1 << flag);
	*KEYFLAG_ADDR |= (1 << flag);
}

 void clr_key_flag(int flag)
{
	key_flags &= ~(1 << flag);
	*KEYFLAG_ADDR &= ~(1 << flag);
}

 void chg_key_flag(int flag)
{
	key_flags ^= 1 << flag;
	*KEYFLAG_ADDR ^= (1 << flag);
}

 int key_flag(int flag)
{
	return ((key_flags >> flag) & 1);
}
/************* end of key-related functions *************/

#undef TERMIO_C

