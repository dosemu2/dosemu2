/* dosmemulator, Matthias Lautner
 *
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * This handles the keyboard.
 *
 * Two keyboard modes are handled 'raw' and 'xlate'. 'Raw' works with
 * codes as sent out by the kernel and 'xlate' uses plain ASCII as used over
 * serial lines. The mapping for different languages & the two ALT-keys is
 * done here, but the definitions are elsewhere. Only the default (US)
 * keymap is stored here.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 *
 */
#include <features.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#ifdef __linux__  
#if __GLIBC__ > 1
#include <sys/vt.h>
#include <sys/kd.h>
#else            
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/time.h>
#endif
#endif

#include <sys/types.h>
#include <sys/stat.h>
#ifdef __NetBSD__
#include <machine/pcvt_ioctl.h>
#endif

#include "config.h"
#include "emu.h"
#include "port.h"
#include "keyboard.h"
/*#include "timers.h"*/
#include "iodev.h"
#include "memory.h"
#include "termio.h"
#include "shared.h"
#include "dosio.h"
#include "mouse.h"
#include "bios.h"
#include "pic.h"

int    port61 = 0x0e;
static u_short microsoft_port_check = 0;

static unsigned int convscanKey (unsigned char);
void dump_kbuffer (void);
void read_next_scancode_from_queue (void);
void convascii (int *);
unsigned int convscanKey (unsigned char);

/* Was a toggle key already port in'd */
static u_char ins_stat = 0;
static u_char scroll_stat = 0;
static u_char num_stat = 0;
static u_char caps_stat = 0;

static unsigned int queue;
#define put_queue(psc) (queue = psc)

static void sysreq (unsigned int), ctrl (unsigned int), alt (unsigned int),
  Unctrl (unsigned int), unalt (unsigned int), lshift (unsigned int), unlshift (unsigned int),
  rshift (unsigned int), unrshift (unsigned int), caps (unsigned int),
  uncaps (unsigned int), ScrollLock (unsigned int), unscroll (unsigned int),
  num (unsigned int), unnum (unsigned int), unins (unsigned int), do_self (unsigned int),
  cursor (unsigned int), func (unsigned int), slash (unsigned int), star (unsigned int),
  enter (unsigned int), minus (unsigned int), plus (unsigned int), backspace (unsigned int),
  Tab (unsigned int), none (unsigned int), spacebar (unsigned int);

void set_leds_area (void);

void set_kbd_flag (int), clr_kbd_flag (int), chg_kbd_flag (int),
  set_key_flag (int), clr_key_flag (int), chg_key_flag (int);
int kbd_flag (int), key_flag (int);

static int altchar = 0;

typedef void (*fptr) (unsigned int);
static const fptr key_table[] =
{
  none, do_self, do_self, do_self,	/* 00-03 s0 esc 1 2 */
  do_self, do_self, do_self, do_self,	/* 04-07 3 4 5 6 */
  do_self, do_self, do_self, do_self,	/* 08-0B 7 8 9 0 */
  do_self, do_self, backspace, Tab,	/* 0C-0F + ' bs tab */
  do_self, do_self, do_self, do_self,	/* 10-13 q w e r */
  do_self, do_self, do_self, do_self,	/* 14-17 t y u i */
  do_self, do_self, do_self, do_self,	/* 18-1B o p } ^ */
  enter, ctrl, do_self, do_self,/* 1C-1F enter ctrl a s */
  do_self, do_self, do_self, do_self,	/* 20-23 d f g h */
  do_self, do_self, do_self, do_self,	/* 24-27 j k l | */
  do_self, do_self, lshift, do_self,	/* 28-2B { para lshift , */
  do_self, do_self, do_self, do_self,	/* 2C-2F z x c v */
  do_self, do_self, do_self, do_self,	/* 30-33 b n m , */
  do_self, slash, rshift, star,	/* 34-37 . / rshift * */
  alt, spacebar, caps, func,	/* 38-3B alt sp caps f1 */
  func, func, func, func,	/* 3C-3F f2 f3 f4 f5 */
  func, func, func, func,	/* 40-43 f6 f7 f8 f9 */
  func, num, ScrollLock, cursor,/* 44-47 f10 num scr home */
  cursor, cursor, minus, cursor,/* 48-4B up pgup - left */
  cursor, cursor, plus, cursor,	/* 4C-4F n5 right + end */
  cursor, cursor, cursor, cursor,	/* 50-53 dn pgdn ins del */
  sysreq, none, do_self, func,	/* 54-57 sysreq ? < f11 */
  func, none, none, none,	/* 58-5B f12 ? ? ? */
  none, none, none, none,	/* 5C-5F ? ? ? ? */
  none, none, none, none,	/* 60-63 ? ? ? ? */
  none, none, none, none,	/* 64-67 ? ? ? ? */
  none, none, none, none,	/* 68-6B ? ? ? ? */
  none, none, none, none,	/* 6C-6F ? ? ? ? */
  none, none, none, none,	/* 70-73 ? ? ? ? */
  none, none, none, none,	/* 74-77 ? ? ? ? */
  none, none, none, none,	/* 78-7B ? ? ? ? */
  none, none, none, none,	/* 7C-7F ? ? ? ? */
  none, none, none, none,	/* 80-83 ? br br br */
  none, none, none, none,	/* 84-87 br br br br */
  none, none, none, none,	/* 88-8B br br br br */
  none, none, none, none,	/* 8C-8F br br br br */
  none, none, none, none,	/* 90-93 br br br br */
  none, none, none, none,	/* 94-97 br br br br */
  none, none, none, none,	/* 98-9B br br br br */
  none, Unctrl, none, none,	/* 9C-9F br unctrl br br */
  none, none, none, none,	/* A0-A3 br br br br */
  none, none, none, none,	/* A4-A7 br br br br */
  none, none, unlshift, none,	/* A8-AB br br unlshift br */
  none, none, none, none,	/* AC-AF br br br br */
  none, none, none, none,	/* B0-B3 br br br br */
  none, none, unrshift, none,	/* B4-B7 br br unrshift br */
  unalt, none, uncaps, none,	/* B8-BB unalt br uncaps br */
  none, none, none, none,	/* BC-BF br br br br */
  none, none, none, none,	/* C0-C3 br br br br */
  none, unnum, unscroll, none,	/* C4-C7 br br br br */
  none, none, none, none,	/* C8-CB br br br br */
  none, none, none, none,	/* CC-CF br br br br */
  none, none, unins, none,	/* D0-D3 br br unins br */
  none, none, none, none,	/* D4-D7 br br br br */
  none, none, none, none,	/* D8-DB br ? ? ? */
  none, none, none, none,	/* DC-DF ? ? ? ? */
  none, none, none, none,	/* E0-E3 e0 e1 ? ? */
  none, none, none, none,	/* E4-E7 ? ? ? ? */
  none, none, none, none,	/* E8-EB ? ? ? ? */
  none, none, none, none,	/* EC-EF ? ? ? ? */
  none, none, none, none,	/* F0-F3 ? ? ? ? */
  none, none, none, none,	/* F4-F7 ? ? ? ? */
  none, none, none, none,	/* F8-FB ? ? ? ? */
  none, none, none, none	/* FC-FF ? ? ? ? */
};


void
keybuf_clear (void)
{
  WRITE_WORD (KBD_HEAD, READ_WORD (KBD_START));
  WRITE_WORD (KBD_TAIL, READ_WORD (KBD_START));
  dump_kbuffer ();
}

/* InsKeyboard
 *  returns 1 if a character could be inserted into Kbuffer
 */
static int
InsKeyboard (unsigned short scancode)
{
  unsigned short nextpos;

  /* First of all compute the position of the new tail pointer */
  if ((nextpos = READ_WORD (KBD_TAIL) + 2) >= READ_WORD (KBD_END))
    nextpos = READ_WORD (KBD_START);
  if (nextpos == READ_WORD (KBD_HEAD))
    {				/* queue full ? */
      if (scancode == 0x2e03)
	{
	  keybuf_clear ();
	  k_printf ("KBD: Buffer cleared for ctrl-break\n");
	  WRITE_BYTE (0x471, 0x80);	/* ctrl-break flag */
	  WRITE_WORD (BIOS_DATA_PTR (READ_WORD (KBD_START)), 0);
	  return (1);
	}
      return (0);
    }

  WRITE_WORD (BIOS_DATA_PTR (READ_WORD (KBD_TAIL)), scancode);
  WRITE_WORD (KBD_TAIL, nextpos);

  return 1;
}

void
dump_kbuffer (void)
{
  int i;
  unsigned short *ptr = BIOS_DATA_PTR (READ_WORD (KBD_START));

  k_printf ("KEYBUFFER DUMP: 0x%02x 0x%02x\n",
	    READ_WORD (KBD_HEAD) - READ_WORD (KBD_START), READ_WORD (KBD_TAIL) - READ_WORD (KBD_START));
  for (i = 0; i < 16; i++)
    k_printf ("%04x ", (unsigned int) READ_WORD (ptr + i));
  k_printf ("\n");
}

/***************************************************************
 * this was copied verbatim from the Linux kernel (keyboard.c) *
 * and then modified unscrupously by hackers on the team       *
 ***************************************************************/

static unsigned char resetid = 0;
static unsigned char firstid = 0;

static unsigned int
convscanKey (unsigned char scancode)
{
  static unsigned char rep = 0xff;

  k_printf ("KBD: convscanKey scancode = 0x%04x\n", scancode);

  if (scancode == 0xe0)
    {
      set_key_flag (KKF_E0);
      set_key_flag (KKF_FIRSTID);
      set_key_flag (KKF_READID);
      resetid = 1;
      firstid = 0;
    }
  else if (scancode == 0xe1)
    {
      set_key_flag (KKF_E1);
      set_key_flag (KKF_FIRSTID);
      set_key_flag (KKF_READID);
      resetid = 1;
      firstid = 0;
    }

  if (scancode == 0xe0 || scancode == 0xe1)
    return (0);
  /*
	 *  The keyboard maintains its own internal caps lock and num lock
	 *  statuses. In caps lock mode E0 AA precedes make code and E0 2A
	 *  follows break code. In num lock mode, E0 2A precedes make
	 *  code and E0 AA follows break code. We do our own book-keeping,
	 *  so we will just ignore these.
	 */
  if (key_flag (KKF_E0) && (scancode == 0x2a || scancode == 0xaa))
    {
      clr_key_flag (KKF_E0);
      clr_key_flag (KKF_E1);
      clr_key_flag (KKF_FIRSTID);
      resetid = 0;
      firstid = 0;
      return (0);
    }
  /*
	 *  Repeat a key only if the input buffers are empty or the
	 *  characters get echoed locally. This makes key repeat usable
	 *  with slow applications and unders heavy loads.
	 */

  rep = scancode;
  queue = 0;
  (*key_table[scancode])(scancode);

  k_printf ("KBD: resetid = %d firstid = %d\n", resetid, firstid);
  if (resetid)
    {
      if (firstid)
	clr_key_flag (KKF_FIRSTID);
      clr_key_flag (KKF_READID);
      resetid = firstid = 0;
    }

  clr_key_flag (KKF_E0);
  clr_key_flag (KKF_E1);

  return (queue);
}

static void
ctrl (unsigned int sc)
{
  if (key_flag (KKF_E0))
    {
      set_key_flag (KKF_RCTRL);
    }
  else
    set_kbd_flag (EKF_LCTRL);

  set_kbd_flag (KF_CTRL);
}

static void
alt (unsigned int sc)
{
  if (key_flag (KKF_E0))
    {
      set_key_flag (KKF_RALT);
    }
  else
    set_kbd_flag (EKF_LALT);

  set_kbd_flag (KF_ALT);
}

static void
Unctrl (unsigned int sc)
{
  if (key_flag (KKF_E0))
    {
      clr_key_flag (KKF_RCTRL);
    }
  else
    clr_kbd_flag (EKF_LCTRL);

  if (!kbd_flag (EKF_LCTRL) && !key_flag (KKF_RCTRL))
    clr_kbd_flag (KF_CTRL);
}

static void
unalt (unsigned int sc)
{
  /*  if (!resetid) { */
  if (key_flag (KKF_E0))
    {
      clr_key_flag (KKF_RALT);
    }
  else
    clr_kbd_flag (EKF_LALT);

  if (!(kbd_flag (EKF_LALT) && key_flag (KKF_RALT)))
    {
      clr_kbd_flag (KF_ALT);

      /* this is for hold-alt-and-keypad entry method */
      if (altchar)
	{
	  /* perhaps anding with 0xff is incorrect.  how about
		     * filtering out anything > 255?
		     */
	  put_queue (altchar & 0xff);	/* just the ASCII */
	}
      altchar = 0;
    }
  /* } */
}

static void
lshift (unsigned int sc)
{
  set_kbd_flag (KF_LSHIFT);
}

static void
unlshift (unsigned int sc)
{
  if (!resetid)
    {
      clr_kbd_flag (KF_LSHIFT);
    }
}

static void
rshift (unsigned int sc)
{
  set_kbd_flag (KF_RSHIFT);
}

static void
unrshift (unsigned int sc)
{
  if (!resetid)
    {
      clr_kbd_flag (KF_RSHIFT);
    }
}

static void
caps (unsigned int sc)
{
  if (kbd_flag (KKF_RCTRL) && kbd_flag (EKF_LCTRL))
    {
      keyboard_mouse = keyboard_mouse ? 0 : 1;
      m_printf ("MOUSE: toggled keyboard mouse %s\n",
		keyboard_mouse ? "on" : "off");
      return;
    }
  else
    {
      set_kbd_flag (EKF_CAPSLOCK);
      if (!caps_stat)
	{
	  if (keepkey)
	    {
	      chg_kbd_flag (KF_CAPSLOCK);	/* toggle; this means SET/UNSET */
	    }
	  set_leds_area ();
	  caps_stat = 1;
	}
    }
}

static void
uncaps (unsigned int sc)
{
  if (!resetid)
    {
      clr_kbd_flag (EKF_CAPSLOCK);
      caps_stat = 0;
    }
}

static void
sysreq (unsigned int sc)
{
  g_printf ("Regs requested: SYSREQ\n");
  show_regs (__FILE__, __LINE__);
}

static void
ScrollLock (unsigned int sc)
{
  if (key_flag (KKF_E0))
    {
      k_printf ("KBD: ctrl-break!\n");
      WRITE_BYTE (0x471, 0x80);	/* ctrl-break flag */
      WRITE_WORD (KBD_HEAD, READ_WORD (KBD_START));
      WRITE_WORD (KBD_TAIL, READ_WORD (KBD_START));
      WRITE_WORD (BIOS_DATA_PTR (READ_WORD (KBD_START)), 0);
      return;
    }
  else if (kbd_flag (KKF_RCTRL))
    show_ints (0, 0x33);
  else if (kbd_flag (KKF_RALT))
    show_regs (__FILE__, __LINE__);
  else if (kbd_flag (KF_RSHIFT))
    {
      warn ("timer int 8 requested...\n");
      pic_request (PIC_IRQ0);
    }
  else if (kbd_flag (KF_LSHIFT))
    {
      warn ("keyboard int 9 requested...\n");
      dump_kbuffer ();
    }
  else
    {
      set_kbd_flag (EKF_SCRLOCK);
      if (!scroll_stat)
	{
	  if (keepkey)
	    {
	      chg_kbd_flag (KF_SCRLOCK);
	    }
	  set_leds_area ();
	  scroll_stat = 1;
	}
    }
}

static void
unscroll (unsigned int sc)
{
  clr_kbd_flag (EKF_SCRLOCK);
  scroll_stat = 0;
}

static void
num (unsigned int sc)
{
  static int lastpause = 0;

  if (kbd_flag (EKF_LCTRL))
    {
      k_printf ("KBD: PAUSE!\n");
      if (lastpause)
	{
	  I_printf ("KBD: waking server up!\n");
#if 0
	  dos_unpause ();
#endif
	  lastpause = 0;
	}
      else
	{
	  I_printf ("KBD: putting server to sleep!\n");
#if 0
	  dos_pause ();
#endif
	  lastpause = 1;
	}
    }
  else
    {
      set_kbd_flag (EKF_NUMLOCK);
      if (!num_stat)
	{
	  k_printf ("KBD: NUMLOCK!\n");
	  if (keepkey)
	    {
	      chg_kbd_flag (KF_NUMLOCK);
	    }
	  k_printf ("KBD: kbd=%d\n", kbd_flag (KF_NUMLOCK));
	  set_leds_area ();
	  num_stat = 1;
	}
    }
}

static void
unnum (unsigned int sc)
{
  num_stat = 0;
  clr_kbd_flag (EKF_NUMLOCK);
}

void
get_leds()
{
  unsigned int led_state = 0;

  do_ioctl(kbd_fd, KDGETLED, (int) &led_state);

  if (led_state & (1 << LED_SCRLOCK)) {
    set_kbd_flag(KF_SCRLOCK);
  }
  else {
    clr_kbd_flag(KF_SCRLOCK);
  }
  if (led_state & (1 << LED_NUMLOCK)) {
    set_kbd_flag(KF_NUMLOCK);
  }
  else {
    clr_kbd_flag(KF_NUMLOCK);
  }
  if (led_state & (1 << LED_CAPSLOCK)) {
    set_kbd_flag(KF_CAPSLOCK);
  }
  else {
    clr_kbd_flag(KF_CAPSLOCK);
  }
  k_printf("KBD: GET LEDS key 96 0x%02x, 97 0x%02x, kbc1 0x%02x, kbc2 0x%02x\n",
	   READ_BYTE(0x496), READ_BYTE(0x497), READ_BYTE(0x417), READ_BYTE(0x418));
}

void
keyboard_flags_init(void){
    WRITE_WORD(KBDFLAG_ADDR, 0);
    WRITE_WORD(KEYFLAG_ADDR, 0);
    get_leds();
    set_key_flag(KKF_KBD102);
}

void
set_leds_area ()
{
  if (kbd_flag (KF_SCRLOCK))
    {
      set_key_flag (KKF_SCRLOCK);
    }
  else
    clr_key_flag (KKF_SCRLOCK);
  if (kbd_flag (KF_NUMLOCK))
    {
      set_key_flag (KKF_NUMLOCK);
    }
  else
    clr_key_flag (KKF_NUMLOCK);
  if (kbd_flag (KF_CAPSLOCK))
    {
      set_key_flag (KKF_CAPSLOCK);
    }
  else
    clr_key_flag (KKF_CAPSLOCK);

  k_printf ("KBD: SET_LEDS_AREA() called\n");
  if(config.console_keyb){
	set_leds();
  }
}

static char accent = 0;
static short accent_sc = 0;
static short very_next_scancode = 0;

static void
do_self (unsigned int sc)
{
  unsigned char ch;
  int out_accent;
  int i;

  if (kbd_flag (KF_ALT))
    {
      /* On a german keyboard the Left-Alt- (Alt-) and the Right-Alt-
       (Alt-Gr-) Keys are different. The keys pressed with AltGr
       return the codes defined in keymaps.c in the alt_map.
       Pressed with the Left-Alt-Key they return the normal symbol
       with the alt-modifier. I've tested this with the 4DOS-Alias-
       Command.                  hein@tlaloc.in.tu-clausthal.de       */
      if (config.keyboard == KEYB_DE_LATIN1)
	{
	  if (kbd_flag (EKF_LALT))	/* Left-Alt-Key pressed ?            */
	    ch = config.alt_map[0];	/* Return Key with Alt-modifier      */
	  else			/* otherwise (this is Alt-Gr)        */
	    ch = config.alt_map[sc];	/* Return key from alt_map          */
	}
      else			/* or no DE_LATIN1-keyboard          */
	ch = config.alt_map[sc];/* Return key from alt_map           */

      if ((sc >= 2) && (sc <= 0xb))	/* numbers */
	sc += 0x76;
      else if (sc == 0xd)
	sc = 0x83;		/* = */
      else if (sc == 0xc)
	sc = 0x82;		/* - */
    }

  else if (kbd_flag (KF_LSHIFT) || kbd_flag (KF_RSHIFT) ||
	   kbd_flag (KF_CTRL))
    ch = config.shift_map[sc];
  else
    ch = config.key_map[sc];


  /* check for a dead key */
  for (i = 0; dead_key_table[i] != 0; i++) {
     if (ch == dead_key_table[i]) {
	if (accent != ch) {
	   k_printf("KBD: dead key accent %d\n", ch);
	   accent=ch;
	   accent_sc = sc;
	   return;
	}
     }
  }


  if (accent) {   /* translate dead keys */
     out_accent = 0;
     if(ch == ' ')
       /* dead-key + space becomes lone accent */
       ch = accent;
     for (i = 0; dos850_dead_map[i].d_key != 0; i++) {
	if (accent == dos850_dead_map[i].d_key &&
	    dos850_dead_map[i].in_key == ch) {

	    k_printf("KBD: map accent %d/key %d to %d\n", accent, ch,
		     dos850_dead_map[i].out_key);
	    ch = dos850_dead_map[i].out_key;
	    sc = 0; /* keyb.exe uses 0 for dead keys */
            break;
	}
	if (accent == dos850_dead_map[i].d_key &&
	    dos850_dead_map[i].in_key == accent) {

	    k_printf("KBD: map accent %d/key %d to %d\n", accent, ch,
		     dos850_dead_map[i].out_key);
	    out_accent = dos850_dead_map[i].out_key;
	}
     }
     /* For an unknown 'dead key' sequence, generate accent+following character
      * as two characters, as real DOS does */
     if(sc && out_accent)
       very_next_scancode = (accent_sc << 8) | out_accent;
     accent = 0;
  }
  if (kbd_flag (KF_CTRL) || kbd_flag (KF_CAPSLOCK))
    if ((ch >= 'a' && ch <= 'z') || (ch >= 224 && ch <= 254))
      ch -= 32;

  if (kbd_flag (KF_CTRL))	/* ctrl */
    ch &= 0x1f;

  k_printf ("KBD: sc=%02x, ch=%02x\n", sc, ch);

  put_queue ((sc << 8) | ch);
}

static void
spacebar (unsigned int sc)
{
  if(accent)
    do_self(sc);
  else
    put_queue (0x3920);
}

/* 0x47-0x53, indexed by sc-0x47 , goes like:
 * home, up, pgup, kp -, left, kp 5, right, kp +, end, down, pgdn, ins, del
 */
unsigned short ctrl_cursor[] =
{
  0x7700, 0x8d00, 0x8400, 0x8e00, 0x7300, 0x8f00, 0x7400,
  0, 0x7500, 0x9100, 0x7600, 0x9200, 0x9300
};

unsigned short alt_cursor[] =
{
  0x9700, 0x9800, 0x9900, 0x4a00, 0x9b00, 0, 0x9d00, 0x4e00, 0x9f00, 0xa000,
  0xa100, 0xa200, 0xa300
};

unsigned short shift_cursor[] =
{
  0x4737, 0x4838, 0x4939, 0x4a2d, 0x4b34, 0x0000, 0x4d36, 0x4e2b, 0x4f31,
  0x5032, 0x5133, 0x5230, 0x532e
};

static void
unins (unsigned int sc)
{
  ins_stat = 0;
}

static void
cursor (unsigned int sc)
{
  int old_sc;

  old_sc = sc;

  if (sc < 0x47 || sc > 0x53)
    return;

  /* leave dosemu on C-A-PGDN */
  if (kbd_flag (KF_CTRL) && kbd_flag (KF_ALT))
    {
#if 0	/* C-A-D, C-A-PgUp are disabled */
      if (sc == 0x53 /*del*/  || sc == 0x49 /*pgup*/ )
	dos_ctrl_alt_del ();
#endif
      if (sc == 0x51)
	{			/*pgdn*/
	  k_printf ("KBD: ctrl-alt-pgdn taking her down!\n");
	  leavedos (0);
	}
    }

  /* if the arrow keys, or home end, do keyboard mouse */
  if ((keyboard_mouse) && (sc == 0x50 || sc == 0x4b || sc == 0x48 ||
			   sc == 0x4d || sc == 0x47 || sc == 0x4f))
    {
      mouse_keyboard (sc);
      return;
    }

  if (sc == 0x52)
    {
      if (!ins_stat)
	{
	  chg_kbd_flag (KF_INSERT);
	  ins_stat = 1;
	}
      else
	return;
    }

  sc -= 0x47;

  /* ENHANCED CURSOR KEYS:  only ctrl and alt may modify them.
   */
  if (key_flag (KKF_E0))
    {
      if (kbd_flag (KF_ALT))
	put_queue (alt_cursor[sc]);
      else if (kbd_flag (KF_CTRL))
	put_queue (ctrl_cursor[sc]);
      else
      if (key_flag(KKF_E0))
        put_queue (old_sc << 8 | 0xe0);
      else
	put_queue (old_sc << 8);
      return;
    }

  /* everything below this must be a keypad key, as the check for KKF_E0
   * above filters out enhanced cursor (gray) keys
   */

  /* this is the hold-alt-and-type numbers thing */
  if (kbd_flag (KF_ALT))
    {
      int digit;

      if ((digit = config.num_table[sc] - '0') <= 9)	/* is a number */
	altchar = altchar * 10 + digit;
    }
  else if (kbd_flag (KF_CTRL))
    put_queue (ctrl_cursor[sc]);
  else if (kbd_flag (KF_NUMLOCK) || kbd_flag (KF_LSHIFT)
	   || kbd_flag (KF_RSHIFT))
    put_queue ((old_sc << 8) | config.num_table[sc]);
  else
    put_queue (old_sc << 8);
}

static void
backspace (unsigned int sc)
{
  /* should be perfect */
  if (kbd_flag (KF_CTRL))
    put_queue (0x0e7f);
  else if (kbd_flag (KF_ALT))
    put_queue (0x0e00);
  else
    put_queue (0x0e08);
}

static void
Tab (unsigned int sc)
{
  if (kbd_flag (KF_CTRL))
    put_queue (0x9400);
  else if (kbd_flag (KF_ALT))
    put_queue (0xa500);
  else if (kbd_flag (KF_LSHIFT) || kbd_flag (KF_RSHIFT))
    put_queue (sc << 8);
  else
    put_queue (sc << 8 | config.key_map[sc]);
}

static void
func (unsigned int sc)
{
  int fnum = sc - 0x3a;

  if (fnum > 10)
    fnum -= 0xc;		/* adjust if f11 or f12 */

  /* this checks for the VC-switch key sequence */
  if (kbd_flag (EKF_LALT) && !key_flag (KKF_RALT) && !kbd_flag (KF_RSHIFT)
      && !kbd_flag (KF_LSHIFT) && kbd_flag (EKF_LCTRL))
    {
      clr_kbd_flag (EKF_LALT);
      clr_kbd_flag (KF_ALT);
      clr_kbd_flag (EKF_LCTRL);
      clr_kbd_flag (KF_CTRL);
      return;
    }

  /* FCH (Fkey CHoose):   returns a if n is f11 or f12, else it returns b
 * PC scancodes for fkeys are "orthogonal" except F11 and F12.
 */

#define FCH(n,a,b) ((n <= 10) ? a : b)

  k_printf ("KBD: sc=%x, fnum=%x\n", sc, fnum);

  if (kbd_flag (KF_LSHIFT) || kbd_flag (KF_RSHIFT))
    put_queue ((sc + FCH (fnum, 0x19, 0x30)) << 8);

  else if (kbd_flag (KF_CTRL))
    put_queue ((sc + FCH (fnum, 0x23, 0x32)) << 8);

  else if (kbd_flag (KF_ALT))
    put_queue ((sc + FCH (fnum, 0x2d, 0x34)) << 8);

  else
    put_queue (FCH (fnum, sc, (sc + 0x2e)) << 8);
}

static void
slash (unsigned int sc)
{
  if (!key_flag (KKF_E0))
    {
      do_self (sc);
    }
  else
    put_queue (sc << 8 | '/');
}

static void
star (unsigned int sc)
{
  do_self (sc);
}

static void
enter (unsigned int sc)
{
  if (kbd_flag (KF_CTRL))
    put_queue (sc << 8 | 0x0a);
  else if (kbd_flag (KF_ALT))
    put_queue (0xa600);
  else
#if 0 /* Not sure why numeric keypad was thought to be e00d? */
    if (key_flag(KKF_E0))
      put_queue (0xe00d);
    else
#endif
      put_queue (sc << 8 | 0x0d);
}

static void
minus (unsigned int sc)
{
  do_self (sc);
}

static void
plus (unsigned int sc)
{
  do_self (sc);
}

static void
none (unsigned int sc)
{
}

/**************** key-area-related functions **************/
/* we  no longer ignore any changes a user program has made...this
 * is good.  Now, we can let users mess around
 * with our ALT flags, as we use no longer use those to change consoles.
 */

void
set_kbd_flag (int flag)
{
  WRITE_WORD (KBDFLAG_ADDR, READ_WORD (KBDFLAG_ADDR) | (1 << flag));
}

void
clr_kbd_flag (int flag)
{
  WRITE_WORD (KBDFLAG_ADDR, READ_WORD (KBDFLAG_ADDR) & ~(1 << flag));
}

void
chg_kbd_flag (int flag)
{
  WRITE_WORD (KBDFLAG_ADDR, READ_WORD (KBDFLAG_ADDR) ^ (1 << flag));
}

int
kbd_flag (int flag)
{
  return ((READ_WORD (KBDFLAG_ADDR) >> flag) & 1);
}

/* These are added to allow DOSEMU process to keep its own flags
   on keyboard status */

/* these are the KEY flags */
void
set_key_flag (int flag)
{
  WRITE_WORD (KEYFLAG_ADDR, READ_WORD (KEYFLAG_ADDR) | (1 << flag));
}

void
clr_key_flag (int flag)
{
  WRITE_WORD (KEYFLAG_ADDR, READ_WORD (KEYFLAG_ADDR) & ~(1 << flag));
}

void
chg_key_flag (int flag)
{
  WRITE_WORD (KEYFLAG_ADDR, READ_WORD (KEYFLAG_ADDR) ^ (1 << flag));
}

int
key_flag (int flag)
{
  return ((READ_WORD (KEYFLAG_ADDR) >> flag) & 1);
}

/************* end of key-related functions *************/

#define SCANQ_LEN 1000
static u_short *scan_queue;
static int *scan_queue_start;
static int *scan_queue_end;
static int next_scancode;

void
shared_keyboard_init (void)
{
  (u_char *) scan_queue = shared_qf_memory + SHARED_KEYBOARD_OFFSET + 8;
  (u_char *) scan_queue_start = shared_qf_memory + SHARED_KEYBOARD_OFFSET;
  (u_char *) scan_queue_end = shared_qf_memory + SHARED_KEYBOARD_OFFSET + 4;
  *scan_queue_start = 0;
  *scan_queue_end = 0;
}

/* This is used to run the keyboard interrupt */
void
do_irq1 (void)
{
  unsigned short nextpos;

  /* reschedule if queue not empty */
  if (*scan_queue_start != *scan_queue_end)
    {
      k_printf ("KBD: Requesting next keyboard interrupt startstop %d:%d\n",
		*scan_queue_start, *scan_queue_end);
      pic_request (PIC_IRQ1);
#if 0
      keys_ready = 0;
#endif
    }

  if ((nextpos = READ_WORD (KBD_TAIL) + 2) >= READ_WORD (KBD_END))
    nextpos = READ_WORD (KBD_START); /*process overflow of kbd-queue*/
  if (nextpos != READ_WORD (KBD_HEAD))
    {  
      read_next_scancode_from_queue ();
  if (keys_ready) do_irq ();			/* do dos interrupt */
  keys_ready = 0;
}
  else /* delay processing */
    {
    }
}


void
set_keyboard_bios (void)
{

  if (config.keybint)
    {
      next_scancode = convscanKey(HI (ax));
    }
  else
    next_scancode = convscanKey(next_scancode);
  keys_ready = 0;		/* flag character as read	*/
  k_printf ("set keyboard bios next_scancode = 0x%04x\n", next_scancode);
}

void
insert_into_keybuffer (void)
{
  /* int15 fn=4f will reset CF if scan key is not to be used */
  /*  if (!config.keybint || LWORD(eflags) & CF) */
  if (!config.keybint || READ_FLAGS () & CF)
    keepkey = 1;
  else
    keepkey = 0;

  if (very_next_scancode && keepkey)
    {
      k_printf ("KBD: putting key in buffer\n");
      if (InsKeyboard (very_next_scancode))
	dump_kbuffer ();
      else
	error ("InsKeyboard could not put key into buffer!\n");
      very_next_scancode=0;
    }

  if (next_scancode && keepkey)
    {
      k_printf ("KBD: putting key in buffer\n");
      if (InsKeyboard (next_scancode))
	dump_kbuffer ();
      else
	error ("InsKeyboard could not put key into buffer!\n");
    }
}
void
scan_to_buffer (void)
{
  k_printf ("scan_to_buffer LASTSCAN_ADD = 0x%04x\n", READ_WORD (LASTSCAN_ADD));
  set_keyboard_bios ();
  insert_into_keybuffer ();
}

void
add_scancode_to_queue (u_short scan)
{
  k_printf ("KBD: Adding scancode to scan_queue: scan %04x, startq=%d, endq=%d\n", scan, *scan_queue_start, *scan_queue_end);

  scan_queue[*scan_queue_end] = scan;
  *scan_queue_end = (*scan_queue_end + 1) % SCANQ_LEN;
  if (config.keybint)
    {
      k_printf ("Hard queue\n");
      pic_request (PIC_IRQ1);
    }
  else
    {
      k_printf ("NOT Hard queue\n");
      read_next_scancode_from_queue ();
      scan_to_buffer ();
      keys_ready = 0;
    }
}


void
read_next_scancode_from_queue (void)
{
  if (!keys_ready)
    {				/* make sure last character has been read */
      if (*scan_queue_start != *scan_queue_end)
	{
	  keys_ready = 1;
	  next_scancode = scan_queue[*scan_queue_start];
	  if (*scan_queue_start != *scan_queue_end)
	    *scan_queue_start = (*scan_queue_start + 1) % SCANQ_LEN;
	  WRITE_WORD (LASTSCAN_ADD, next_scancode);
	}
    }
  else
    k_printf ("KBD: read_next_scancode key not Read!\n");

  k_printf ("KBD: Nextscan key 96 0x%02x, 97 0x%02x, kbc1 0x%02x, kbc2 0x%02x\n",
	    *(u_char *) 0x496, *(u_char *) 0x497, *(u_char *) 0x417, *(u_char *) 0x418);
  k_printf ("start=%d, end=%d, LASTSCAN=%x\n", *scan_queue_start, *scan_queue_end, READ_WORD (LASTSCAN_ADD));
}

Bit8u keyb_io_read(ioport_t port)
{
  int r=0;
  switch (port) {
  case 0x60:
    if (keys_ready) microsoft_port_check = 0;
    k_printf("direct 8042 0x60 read1: 0x%02x microsoft=%d\n",
        *LASTSCAN_ADD, microsoft_port_check);
    if (microsoft_port_check)
      r = microsoft_port_check;
    else
      r = *LASTSCAN_ADD;
      keys_ready = 0;
    break;
  case 0x61:
    if (config.speaker == SPKR_NATIVE)
      r = port61 = safe_port_in_byte(0x61);
    break;
  case 0x64:
    /* low bit set = sc ready */
    r = 0x1c | (keys_ready || microsoft_port_check ? 1 : 0);
    k_printf("direct 8042 0x64 status check: keys_ready=%d, microsoft=%d\n",
      keys_ready, microsoft_port_check);
    break;
  default: 
    k_printf("KBD: Unsupported read from port 0x%x\n", port); 
  }
  k_printf("KBD: 0x%x inb = 0x%x\n", port, r);
  return r;
}

void keyb_io_write(ioport_t port, Bit8u byte)
{
  k_printf("KBD: 0x%x outb = 0x%x\n", port, byte);
  switch (port) {
  case 0x60:
    microsoft_port_check = 1;
    if (byte < 0xf0) {
      microsoft_port_check = 0xfe;
    }
    else {
      microsoft_port_check = 0xfa;
    }
    break;
  case 0x61:
    port61 = byte & 0x0f;
    switch (config.speaker) {
      case SPKR_NATIVE:
        safe_port_out_byte(0x61, byte & 0x03);
        break;
      case SPKR_EMULATED:
        if (((byte & 3) == 3) && (pit[2].mode == 2 || pit[2].mode == 3)) {
          i_printf("PORT: emulated beep!\n");
          putchar('\007');
        }
        break;
    }
  case 0x64:
    k_printf("KBD: No action for port 0x64\n");
    break;
  default: 
    k_printf("KBD: Unsupported write from to port 0x%x of 0x%x\n",
	port, byte); 
  }
}

void keyb_init(void)
{
#ifdef NEW_PORT_CODE
  emu_iodev_t  io_device;

  /* 8042 keyboard controller */
  io_device.read_portb   = keyb_io_read;
  io_device.write_portb  = keyb_io_write;
  io_device.read_portw   = NULL;
  io_device.write_portw  = NULL;
  io_device.read_portd   = NULL;
  io_device.write_portd  = NULL;
  io_device.handler_name = "8042 Keyboard data";
  io_device.start_addr   = 0x0060;
  io_device.end_addr     = 0x0060;
  io_device.irq          = 1;
  io_device.fd           = -1;
  port_register_handler(io_device, 0);

  io_device.handler_name = "8042 Keyboard command";
  io_device.start_addr   = 0x0064;
  io_device.end_addr     = 0x0064;
  io_device.irq 	 = EMU_NO_IRQ;
  port_register_handler(io_device, 0);

  /* with OLD_KEYBOARD_CODE, speaker goes to keyb_io */
  io_device.handler_name = "Speaker port";
  io_device.start_addr   = 0x0061;
  io_device.end_addr     = 0x0061;
  port_register_handler(io_device, config.speaker==SPKR_NATIVE? PORT_FAST:0);
#endif
}

void keyb_reset(void)
{
#if 0
  microsoft_port_check = 0;
  keyb_ctrl_port61     = 0x0e;
  keyb_ctrl_port60     = -1;
  keyb_ctrl_mode60     = 0;
  keyb_ctrl_scanmap    = 1;
  keyb_ctrl_typematic  = 0x23;
  keyb_ctrl_enable     = 1;
  keyb_ctrl_isdata     = 0;
  keyb_ctrl_clearbuf();
#endif
}
