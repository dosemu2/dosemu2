/* dosmemulator, Matthias Lautner 
 * 
 * DANG_BEGIN_MODULE
 *
 * This handles the keyboard.
 *
 * Two keyboard modes are handled 'raw' and 'xlate'. 'Raw' works with
 * codes as sent out by the kernel and 'xlate' uses plain ASCII as used over
 * serial lines. The mapping for different languages & the two ALT-keys is
 * done here, but the definitions are elsewhere. Only the default (US)
 * keymap is stored here.
 *
 * DANG_END_MODULE
 *
 *
 * DANG_BEGIN_CHANGELOG
 * Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1995/02/25 22:38:32 $
 * $Source: /home/src/dosemu0.60/keyboard/RCS/keyboard.c,v $
 * $Revision: 1.3 $
 * $State: Exp $
 * $Log: keyboard.c,v $
 * Revision 1.3  1995/02/25  22:38:32  root
 * *** empty log message ***
 *
 * Revision 1.2  1995/02/25  21:54:17  root
 * *** empty log message ***
 *
 * Revision 1.1  1995/02/05  16:53:37  root
 * Initial revision
 *
 * Revision 1.2  1995/01/14  15:31:03  root
 * New Year checkin.
 *
 * Revision 1.1  1994/11/06  02:36:40  root
 * Initial revision
 *
 * Revision 2.20  1994/11/03  11:43:26  root
 * Checkin Prior to Jochen's Latest.
 *
 * Revision 2.19  1994/10/14  17:58:38  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.18  1994/10/03  00:24:25  root
 * Checkin prior to pre53_25.tgz
 *
 * Revision 2.17  1994/09/28  00:55:59  root
 * Prep for pre53_23.tgz
 *
 * Revision 2.16  1994/09/26  23:10:13  root
 * Prep for pre53_22.
 *
 * Revision 2.15  1994/09/22  23:51:57  root
 * Prep for pre53_21.
 *
 * Revision 2.14  1994/09/20  01:53:26  root
 * Prep for pre53_21.
 *
 * Revision 2.13  1994/09/11  01:01:23  root
 * Prep for pre53_19.
 *
 * Revision 2.12  1994/08/25  00:49:34  root
 * Lutz's STI patches and prep for pre53_16.
 *
 * Revision 2.11  1994/08/17  02:08:22  root
 * Mods to Rain's patches to get all modes back on the road.
 *
 * Revision 2.10  1994/08/14  02:52:04  root
 * Rain's latest CLEANUP and MOUSE for X additions.
 *
 * Revision 2.9  1994/08/11  01:11:34  root
 * Modifications for proper backspace behavior in non-raw mode.
 *
 * Revision 2.8  1994/08/09  01:49:57  root
 * Prep for pre53_11.
 *
 * Revision 2.7  1994/08/01  14:26:23  root
 * Prep for pre53_7  with Markks latest, EMS patch, and Makefile changes.
 *
 * Revision 2.6  1994/07/14  23:19:20  root
 * Markkk's patches.
 *
 * Revision 2.5  1994/07/11  21:04:57  root
 * Latest keycode/terminfo updates by Markkk.
 *
 * Revision 2.4  1994/07/09  14:29:43  root
 * prep for pre53_3.
 *
 * Revision 2.3  1994/07/05  21:59:13  root
 * NCURSES IS HERE.
 *
 * Revision 2.2  1994/06/14  22:00:18  root
 * Alistair's DANG inserted for the first time :-).
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.36  1994/05/30  00:08:20  root
 * Prep for pre51_22 and temp kludge fix for dir a: error.
 *
 * Revision 1.35  1994/05/24  01:23:00  root
 * Lutz's latest, int_queue_run() update.
 *
 * Revision 1.34  1994/05/21  23:39:19  root
 * PRE51_19.TGZ with Lutz's latest updates.
 *
 * Revision 1.33  1994/05/18  00:15:51  root
 * pre15_17.
 *
 * Revision 1.32  1994/05/13  23:20:15  root
 * Pre51_15.
 *
 * Revision 1.31  1994/05/13  17:21:00  root
 * pre51_15.
 *
 * Revision 1.30  1994/05/04  21:56:55  root
 * Prior to Alan's mouse patches.
 *
 * Revision 1.29  1994/04/27  23:39:57  root
 * Lutz's patches to get dosemu up under 1.1.9.
 *
 * Revision 1.28  1994/04/23  20:51:40  root
 * Get new stack over/underflow working in VM86 mode.
 *
 * Revision 1.27  1994/04/20  23:43:35  root
 * pre51_8 out the door.
 *
 * Revision 1.26  1994/04/18  22:52:19  root
 * Ready pre51_7.
 *
 * Revision 1.25  1994/04/13  00:07:09  root
 * Multiple patches from various sources.
 *
 * Revision 1.24  1994/04/07  20:50:59  root
 * More updates.
 *
 * Revision 1.23  1994/04/04  22:51:55  root
 * Patches for PS/2 mice.
 *
 * Revision 1.22  1994/03/18  23:17:51  root
 * Prep for 0.50pl1
 *
 * Revision 1.21  1994/03/13  01:07:31  root
 * Poor attempts to optimize.
 *
 * Revision 1.20  1994/03/10  02:49:27  root
 * Back to SINGLE Process.
 *
 * Revision 1.19  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.18  1994/03/04  00:01:58  root
 * Readying for 0.50
 *
 * Revision 1.17  1994/02/20  15:34:40  root
 * Working on keyboard.
 *
 * DANG_END_CHANGELOG
 */

#ifdef USE_SLANG
#define USE_SLANG_KEYS
#endif

#include <stdio.h>
#include <unistd.h>
#include <linux/utsname.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#ifndef USE_SLANG_KEYS
#include <ncurses.h>
#endif
#include <sys/mman.h>
#include <signal.h>
#include <sys/stat.h>
#include <linux/vt.h>
#include <linux/kd.h>

#include "bios.h"
#include "config.h"
#include "memory.h"
#include "emu.h"
#include "termio.h"
#include "video.h"
#include "mouse.h"
#include "dosio.h"
#include "cpu.h"
#include "keymaps.h"
#include "int.h"
#ifdef NEW_PIC
#include "../timer/pic.h"
#endif
#include "shared.h"
#include "vc.h"


inline void child_set_flags(int);

static void keyboard_handling_init(void);
void clear_raw_mode();
extern void clear_console_video();
extern void clear_consoleX_video();
extern void clear_process_control();
void set_raw_mode();
void get_leds();
void activate(int);
extern int terminal_initialize();
extern void terminal_close();

void convascii(int *);

/* Was a toggle key already port in'd */
static u_char ins_stat = 0;
static u_char  scroll_stat = 0;
static u_char num_stat = 0;
static u_char caps_stat = 0;



unsigned int convscanKey(unsigned char);
static unsigned int queue;

#define put_queue(psc) (queue = psc)

static void sysreq(unsigned int), ctrl(unsigned int),
 alt(unsigned int), Unctrl(unsigned int), unalt(unsigned int), lshift(unsigned int),
 unlshift(unsigned int), rshift(unsigned int), unrshift(unsigned int),
 caps(unsigned int), uncaps(unsigned int), ScrollLock(unsigned int), unscroll(unsigned int),
 num(unsigned int), unnum(unsigned int), unins(unsigned int), do_self(unsigned int),
 cursor(unsigned int), func(unsigned int), slash(unsigned int), star(unsigned int),
 enter(unsigned int), minus(unsigned int), plus(unsigned int), backspace(unsigned int),
 Tab(unsigned int), none(unsigned int), spacebar(unsigned int);
void getKeys();

void child_set_flags(int sc);

void set_kbd_flag(int), clr_kbd_flag(int), chg_kbd_flag(int), child_set_kbd_flag(int),
 child_clr_kbd_flag(int), set_key_flag(int), clr_key_flag(int), chg_key_flag(int);

int kbd_flag(int), child_kbd_flag(int), key_flag(int);

/* initialize these in keyboard_init()! */
unsigned int child_kbd_flags = 0;

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

#define us unsigned short

static int  old_kbd_flags = -1;		/* flags for STDIN before our fcntl */


int kbcount = 0;
unsigned char kbbuf[KBBUF_SIZE], *kbp, erasekey;
static struct termios oldtermios;	/* original terminal modes */

#ifndef USE_SLANG_KEYS
char tc[1024], termcap[1024];
int li, co;			/* lines, columns */

struct funkeystruct {
  unsigned char *esc;
  unsigned char *tce;
  unsigned short code;
};

/* The funkeystruct structures have been moved to termio.h */
/* This is a translation table for esc-prefixed terminal keyboard codes
 * to PC computer keyboard codes.
 */
static const struct funkeystruct funkey[] =
{
  {NULL, "kich1", 0x5200},	/* kI     Ins */
  {NULL, "kdch1", 0x5300},	/* kD     Del   0x007F */
  {NULL, "khome", 0x4700},	/* kh     Ho    0x5C00 */
  {NULL, "kend", 0x4f00},	/* kH     End   0x6100 */
  {NULL, "kcuu1", 0x4800},	/* ku     Up */
  {NULL, "kcud1", 0x5000},	/* kd     Dn */
  {NULL, "kcuf1", 0x4d00},	/* kr     Ri */
  {NULL, "kcub1", 0x4b00},	/* kl     Le */
  {NULL, "kpp", 0x4900},	/* kP     PgUp */
  {NULL, "knp", 0x5100},	/* kN     PgDn */
  {NULL, "kf1", 0x3b00},	/* k1     F1 */
  {NULL, "kf2", 0x3c00},	/* k2     F2 */
  {NULL, "kf3", 0x3d00},	/* k3     F3 */
  {NULL, "kf4", 0x3e00},	/* k4     F4 */
  {NULL, "kf5", 0x3f00},	/* k5     F5 */
  {NULL, "kf6", 0x4000},	/* k6     F6 */
  {NULL, "kf7", 0x4100},	/* k7     F7 */
  {NULL, "kf8", 0x4200},	/* k8     F8 */
  {NULL, "kf9", 0x4300},	/* k9     F9 */
  {NULL, "kf10", 0x4400},	/* k0     F10 */
  {NULL, "kf11", 0x8500},	/*        F11 */
  {NULL, "kf12", 0x8600},	/*        F12 */
  {"\033[2~", NULL, 0x5200},	/* Ins */
  {"\033[3~", NULL, 0x5300},	/* Del    Another keyscan is 0x007F */
  {"\033[1~", NULL, 0x4700},	/* Ho     Another keyscan is 0x5c00 */
  {"\033[H", NULL, 0x4700},	/* Ho */
  {"\033[4~", NULL, 0x4f00},	/* End    Another keyscan is 0x6100 */
  {"\033[K", NULL, 0x4f00},	/* End */
  {"\033[5~", NULL, 0x4900},	/* PgUp */
  {"\033[6~", NULL, 0x5100},	/* PgDn */
  {"\033[A", NULL, 0x4800},	/* Up */
  {"\033OA", NULL, 0x4800},	/* Up */
  {"\033[B", NULL, 0x5000},	/* Dn */
  {"\033OB", NULL, 0x5000},	/* Dn */
  {"\033[C", NULL, 0x4d00},	/* Ri */
  {"\033OC", NULL, 0x4d00},	/* Ri */
  {"\033[D", NULL, 0x4b00},	/* Le */
  {"\033OD", NULL, 0x4b00},	/* Le */
  {"\033[[A", NULL, 0x3b00},	/* F1 */
  {"\033[[B", NULL, 0x3c00},	/* F2 */
  {"\033[[C", NULL, 0x3d00},	/* F3 */
  {"\033[[D", NULL, 0x3e00},	/* F4 */
  {"\033[[E", NULL, 0x3f00},	/* F5 */
  {"\033[11~", NULL, 0x3b00},	/* F1 */
  {"\033[12~", NULL, 0x3c00},	/* F2 */
  {"\033[13~", NULL, 0x3d00},	/* F3 */
  {"\033[14~", NULL, 0x3e00},	/* F4 */
  {"\033[15~", NULL, 0x3f00},	/* F5 */
  {"\033[17~", NULL, 0x4000},	/* F6 */
  {"\033[18~", NULL, 0x4100},	/* F7 */
  {"\033[19~", NULL, 0x4200},	/* F8 */
  {"\033[20~", NULL, 0x4300},	/* F9 */
  {"\033[21~", NULL, 0x4400},	/* F10 */
  {"\033[23~", NULL, 0x5400},	/* Shift F1  (F11 acts like Shift-F1) */
  {"\033[24~", NULL, 0x5500},	/* Shift F2  (F12 acts like Shift-F2) */
  {"\033[25~", NULL, 0x5600},	/* Shift F3 */
  {"\033[26~", NULL, 0x5700},	/* Shift F4 */
  {"\033[28~", NULL, 0x5800},	/* Shift F5 */
  {"\033[29~", NULL, 0x5900},	/* Shift F6 */
  {"\033[31~", NULL, 0x5A00},	/* Shift F7 */
  {"\033[32~", NULL, 0x5B00},	/* Shift F8 */
  {"\033[33~", NULL, 0x5C00},	/* Shift F9 */
  {"\033[34~", NULL, 0x5D00},	/* Shift F10 */
  {"\033OQ", NULL, 0x352F},	/* Keypad / */
  {"\033OR", NULL, 0x372A},	/* Keypad * */
  {"\033OS", NULL, 0x4A2D},	/* Keypad - */
  {"\033Ol", NULL, 0x4E2B},	/* Keypad + */
  {"\033On", NULL, 0x5300},	/* Keypad . */
  {"\033Op", NULL, 0x5200},	/* Keypad 0 */
  {"\033Oq", NULL, 0x4F00},	/* Keypad 1 */
  {"\033Or", NULL, 0x5000},	/* Keypad 2 */
  {"\033Os", NULL, 0x5100},	/* Keypad 3 */
  {"\033Ot", NULL, 0x4B00},	/* Keypad 4 */
  {"\033Ou", NULL, 0x4C00},	/* Keypad 5 */
  {"\033Ov", NULL, 0x4D00},	/* Keypad 6 */
  {"\033Ow", NULL, 0x4700},	/* Keypad 7 */
  {"\033Ox", NULL, 0x4800},	/* Keypad 8 */
  {"\033Oy", NULL, 0x4900},	/* Keypad 9 */
  {"\033OM", NULL, 0x1C0D},	/* Keypad Enter */
  {"\033a", NULL, 0x1e00},	/* Alt A */
  {"\033b", NULL, 0x3000},	/* Alt B */
  {"\033c", NULL, 0x2e00},	/* Alt C */
  {"\033d", NULL, 0x2000},	/* Alt D */
  {"\033e", NULL, 0x1200},	/* Alt E */
  {"\033f", NULL, 0x2100},	/* Alt F */
  {"\033g", NULL, 0x2200},	/* Alt G */
  {"\033h", NULL, 0x2300},	/* Alt H */
  {"\033i", NULL, 0x1700},	/* Alt I */
  {"\033j", NULL, 0x2400},	/* Alt J */
  {"\033k", NULL, 0x2500},	/* Alt K */
  {"\033l", NULL, 0x2600},	/* Alt L */
  {"\033m", NULL, 0x3200},	/* Alt M */
  {"\033n", NULL, 0x3100},	/* Alt N */
  {"\033o", NULL, 0x1800},	/* Alt O */
  {"\033p", NULL, 0x1900},	/* Alt P */
  {"\033q", NULL, 0x1000},	/* Alt Q */
  {"\033r", NULL, 0x1300},	/* Alt R */
  {"\033s", NULL, 0x1f00},	/* Alt S */
  {"\033t", NULL, 0x1400},	/* Alt T */
  {"\033u", NULL, 0x1600},	/* Alt U */
  {"\033v", NULL, 0x2f00},	/* Alt V */
  {"\033w", NULL, 0x1100},	/* Alt W */
  {"\033x", NULL, 0x2d00},	/* Alt X */
  {"\033y", NULL, 0x1500},	/* Alt Y */
  {"\033z", NULL, 0x2c00},	/* Alt Z */
  {"\0330", NULL, 0x8100},	/* Alt 0 */
  {"\0331", NULL, 0x7800},	/* Alt 1 */
  {"\0332", NULL, 0x7900},	/* Alt 2 */
  {"\0333", NULL, 0x7a00},	/* Alt 3 */
  {"\0334", NULL, 0x7b00},	/* Alt 4 */
  {"\0335", NULL, 0x7c00},	/* Alt 5 */
  {"\0336", NULL, 0x7d00},	/* Alt 6 */
  {"\0337", NULL, 0x7e00},	/* Alt 7 */
  {"\0338", NULL, 0x7f00},	/* Alt 8 */
  {"\0339", NULL, 0x8000},	/* Alt 9 */
  {"\033`", NULL, 0x2900},	/* Alt ` */
  {"\033-", NULL, 0x8200},	/* Alt - */
  {"\033=", NULL, 0x8300},	/* Alt = */
  {"\033\\", NULL, 0x2B00},	/* Alt \ */
  {"\033;", NULL, 0x2700},	/* Alt ; */
  {"\033'", NULL, 0x2800},	/* Alt ' */
  {"\033,", NULL, 0x3300},	/* Alt , */
  {"\033.", NULL, 0x3400},	/* Alt . */
  {"\033/", NULL, 0x3500},	/* Alt / */
  {"\033\011", NULL, 0xA500},	/* Alt Tab */
  {"\033\015", NULL, 0x1C00},	/* Alt Enter */
  {NULL, NULL, 0}		/* Ending delimiter */
};

/* This is a translation table for single character high ASCII characters
 * used by ALT keypresses in Xterms, into PC computer keyboard codes.
 */
static const struct funkeystruct xfunkey[] =
{
  {"\341", NULL, 0x1e00},	/* Alt A */
  {"\342", NULL, 0x3000},	/* Alt B */
  {"\343", NULL, 0x2e00},	/* Alt C */
/*  {"\344", NULL, 0x2000}, *//* Alt D *//* This is not (always) true */
  {"\345", NULL, 0x1200},	/* Alt E */
  {"\346", NULL, 0x2100},	/* Alt F */
  {"\347", NULL, 0x2200},	/* Alt G */
  {"\350", NULL, 0x2300},	/* Alt H */
  {"\351", NULL, 0x1700},	/* Alt I */
  {"\352", NULL, 0x2400},	/* Alt J */
  {"\353", NULL, 0x2500},	/* Alt K */
  {"\354", NULL, 0x2600},	/* Alt L */
  {"\355", NULL, 0x3200},	/* Alt M */
  {"\356", NULL, 0x3100},	/* Alt N */
  {"\357", NULL, 0x1800},	/* Alt O */
  {"\360", NULL, 0x1900},	/* Alt P */
  {"\361", NULL, 0x1000},	/* Alt Q */
  {"\362", NULL, 0x1300},	/* Alt R */
  {"\363", NULL, 0x1f00},	/* Alt S */
  {"\364", NULL, 0x1400},	/* Alt T */
  {"\365", NULL, 0x1600},	/* Alt U */
/*  {"\366", NULL, 0x2f00}, *//* Alt V *//* This is not (always) true */
  {"\367", NULL, 0x1100},	/* Alt W */
  {"\370", NULL, 0x2d00},	/* Alt X */
  {"\371", NULL, 0x1500},	/* Alt Y */
  {"\372", NULL, 0x2c00},	/* Alt Z */
  {"\260", NULL, 0x8100},	/* Alt 0 */
  {"\261", NULL, 0x7800},	/* Alt 1 */
  {"\262", NULL, 0x7900},	/* Alt 2 */
  {"\263", NULL, 0x7a00},	/* Alt 3 */
  {"\264", NULL, 0x7b00},	/* Alt 4 */
  {"\265", NULL, 0x7c00},	/* Alt 5 */
  {"\266", NULL, 0x7d00},	/* Alt 6 */
  {"\267", NULL, 0x7e00},	/* Alt 7 */
  {"\270", NULL, 0x7f00},	/* Alt 8 */
  {"\271", NULL, 0x8000},	/* Alt 9 */
  {"\340", NULL, 0x2900},	/* Alt ` */
  {"\255", NULL, 0x8200},	/* Alt - */
  {"\275", NULL, 0x8300},	/* Alt = */
/*  {"\334", NULL, 0x2B00}, *//* Alt \ *//* This is not (always) true */
  {"\273", NULL, 0x2700},	/* Alt ; */
  {"\247", NULL, 0x2800},	/* Alt ' */
  {"\254", NULL, 0x3300},	/* Alt , */
  {"\256", NULL, 0x3400},	/* Alt . */
  {"\257", NULL, 0x3500},	/* Alt / */
  {"\333", NULL, 0x1A00},	/* Alt [ */
  {"\335", NULL, 0x1B00},	/* Alt ] */
  {"\233", NULL, 0x0100},	/* Alt Esc */
  {"\377", NULL, 0x0E00},	/* Alt Backspace */
  {"\211", NULL, 0xA500},	/* Alt Tab */
  {"\215", NULL, 0x1C00},	/* Alt Enter */

  {"\344", NULL, 0x0084},	/* Umlaut ae */
  {"\366", NULL, 0x0094},	/* Umlaut oe */
  {"\374", NULL, 0x0081},	/* Umlaut ue */
  {"\337", NULL, 0x00e1},	/* Umlaut sz */
  {"\304", NULL, 0x008E},	/* Umlaut Ae */
  {"\326", NULL, 0x0099},	/* Umlaut Oe */
  {"\334", NULL, 0x009A},	/* Umlaut Ue */

  {NULL, NULL, 0}		/* Ending delimiter */
};

/* this table is used by convKey() to give the int16 functions the
 * correct scancode in the high byte of the returned key (AH)
 * this might need changing per country, like the RAW keyboards, but I
 * don't think so.  I think that it'll make every keyboard look like
 * a U.S. keyboard to DOS, which maybe "keyb" does anyway.  Sorry
 * it's so ugly.
 * this is a table of scancodes, indexed by the ASCII value of the character
 * to be completed
 */
unsigned char highscan[256] =
{
  0, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0xe, 0x0f, 0x24, 0x25, 0x2e, 0x1c,	/* 0-0xd */
0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c,	/* -> 0x1a */
  1, 0x2b, 0, 7, 0xc,		/* ASCII 1b-1F */
  0x39, 2, 0x28, 4, 5, 6, 8, 0x28, 0xa, 0xb, 9, 0xd, 0x33, 0x0c, 0x34, 0x35,	/* -> 2F */
  0x0b, 2, 3, 4, 5, 6, 7, 8, 9, 0xa,	/* numbers 0, 1-9; ASCII 0x30-0x39 */
  0x27, 0x27, 0x33, 0xd, 0x34, 0x35, 3,	/* ASCII 0x3A-0x40  */
  0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31,	/* CAP LETERS A-N */
  0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c,	/* CAP O-Z last ASCII 0x5a */
  0x1a, 0x2b, 0x1b, 7, 0x0c, 0x29,	/* ASCII 0x5b-0x60 */
/* 0x61 - 0x7a on next 2 lines */
  0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18,	/* lower a-o */
  0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c,	/* lowercase p-z */
  0x1a, 0x2b, 0x1b, 0x29, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* ASC 0x7b-0x8f */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* ASC 0x90-0x9f */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* ASC 0xa0-0xaf */
0x81, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0, 0, 0, 0, 0, 0,	/* 0xb0-0xbf */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* ASC 0xc0-0xcf */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* ASC 0xd0-0xdf */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* ASC 0xe0-0xef */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	/* ASC 0xf0-0xff */
};

#endif /* NOT USE_SLANG_KEYS */

void
keyboard_close(void)
{
  if (kbd_fd != -1) {

    if (config.console_keyb) {
      k_printf("KBD: keyboard_close:clear raw keyb\n");
      clear_raw_mode();
    }
    if (config.console_video) {
      k_printf("KBD: keyboard_close:clear console video\n");
      clear_console_video();
    }
    if (config.usesX) {
      v_printf("CloseKeyboard:clear console video\n");
      clear_consoleX_video();
    }

    if ((config.console_keyb || config.console_video) && !config.usesX) {
      k_printf("KBD: keyboard_close: clear process control\n");
      clear_process_control();
    }

    fcntl(kbd_fd, F_SETFL, old_kbd_flags);
    tcsetattr(kbd_fd, TCSANOW, &oldtermios);

    close(kbd_fd);
    kbd_fd = -1;
  }
}

static struct termios save_termios;

static void
print_termios(struct termios term)
{
  k_printf("KBD: TERMIOS Structure:\n");
  k_printf("KBD: 	c_iflag=%x\n", term.c_iflag);
  k_printf("KBD: 	c_oflag=%x\n", term.c_oflag);
  k_printf("KBD: 	c_cflag=%x\n", term.c_cflag);
  k_printf("KBD: 	c_lflag=%x\n", term.c_lflag);
  k_printf("KBD: 	c_line =%x\n", term.c_line);
}

/*
 * DANG_BEGIN_FUNCTION keyboard_init
 *
 * description:
 *  Initialize the keyboard to DOSEMU deafaults plus those requested
 *  in the configs if allowable.
 *
 * DANG_END_FUNCTION
 */
int
keyboard_init(void)
{
  struct termios newtermio;	/* new terminal modes */
  struct stat chkbuf;
  int major, minor;

  if (!config.X) {
     if (config.usesX) {
	kbd_fd = dup(keypipe);
     }
     else {
	kbd_fd = STDIN_FILENO;
     }

     if (kbd_fd < 0) {
	error("ERROR: Couldn't duplicate STDIN !\n");
	return -1;
     }
     
     old_kbd_flags = fcntl(kbd_fd, F_GETFL);
     fcntl(kbd_fd, F_SETFL, O_RDONLY | O_NONBLOCK);
     
     if (use_sigio && !config.kbd_tty) {
       k_printf("KBD: Using SIGIO\n");
       add_to_io_select(kbd_fd, 1);
     } else {
       k_printf("KBD: Not using SIGIO\n");
       add_to_io_select(kbd_fd, 0);
     }

     fstat(kbd_fd, &chkbuf);
     major = chkbuf.st_rdev >> 8;
     minor = chkbuf.st_rdev & 0xff;
  } else
     major = minor = 0;

  if (!config.usesX) {
    /* console major num is 4, minor 64 is the first serial line */
    if ((major == 4) && (minor < 64))
      scr_state.console_no = minor;	/* get minor number */
    else {
      if (config.console_keyb || config.console_video)
	error("ERROR: STDIN not a console-can't do console modes!\n");
      scr_state.console_no = 0;
      config.console_keyb = 0;
      config.console_video = 0;
      config.mapped_bios = 0;
      config.vga = 0;
      config.graphics = 0;
      if (config.speaker == SPKR_NATIVE)
        config.speaker = SPKR_EMULATED;
    }
  }
  else {
    scr_state.console_no = 0;
  }

  if (!config.X && tcgetattr(kbd_fd, &oldtermios) < 0) {
    error("ERROR: Couldn't tcgetattr(STDIN,...) !\n");
    /* close(kbd_fd); kbd_fd = -1; return -1; <------ XXXXXXX Remove this */
  }

/*
 * DANG_BEGIN_REMARK
 *
 *  Code is called at start up to set up the terminal line for non-raw
 *  mode.
 *
 * DANG_END_REMARK
 */
  if (!config.X) {
     newtermio = oldtermios;
     newtermio.c_iflag &= (ISTRIP | IGNBRK | IXON | IXOFF);	/* (IXON|IXOFF|IXANY|ISTRIP|IGNBRK);*/
     /* newtermio.c_oflag &= ~OPOST;
     newtermio.c_cflag &= ~(HUPCL); */
     newtermio.c_cflag &= ~(CLOCAL | CSIZE | PARENB);
     newtermio.c_cflag |= CS8;
     newtermio.c_lflag &= 0;                  /* ISIG */
     newtermio.c_cc[VMIN] = 1;
     newtermio.c_cc[VTIME] = 0;
     erasekey = newtermio.c_cc[VERASE];
     cfgetispeed(&newtermio);
     cfgetospeed(&newtermio);
     if (tcsetattr(kbd_fd, TCSANOW, &newtermio) < 0) {
	error("ERROR: Couldn't tcsetattr(STDIN,TCSANOW,...) !\n");
     }
  }

  WRITE_WORD(KBDFLAG_ADDR, 0);
  /*kbd_flags = 0;*/
  child_kbd_flags = 0;
  WRITE_WORD(KEYFLAG_ADDR, 0);
  /*key_flags = 0;*/

  keyboard_handling_init();

  dbug_printf("TERMIO: $Header: /home/src/dosemu0.60/keyboard/RCS/keyboard.c,v 1.3 1995/02/25 22:38:32 root Exp root $\n");

  return 0;
}

static void
clear_raw_mode(void)
{
#ifdef X_SUPPORT
   if (config.X)
      return;
#endif
  do_ioctl(kbd_fd, KDSKBMODE, K_XLATE);
  if (tcsetattr(kbd_fd, TCSAFLUSH, &save_termios) < 0) {
    k_printf("KBD: Resetting Keyboard to K_XLATE mode failed.\n");
    return;
  }
}

static void
set_raw_mode(void)
{
#ifdef X_SUPPORT
   if (config.X)
      return;
#endif
  k_printf("KBD: Setting keyboard to RAW mode\n");
  if (!config.console_video)
    fprintf(stderr, "\nKBD: Entering RAW mode for DOS!\n");
  do_ioctl(kbd_fd, KDSKBMODE, K_RAW);
  tty_raw(kbd_fd);
}

static int
tty_raw(int fd)
{
  struct termios buf;

#ifdef X_SUPPORT
   if (config.X)
      return 0;
#endif
  if (tcgetattr(fd, &save_termios) < 0)
    return (-1);

  buf = save_termios;

  buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  buf.c_iflag &= ~(IMAXBEL | IGNBRK | IGNCR | IGNPAR | BRKINT | INLCR | ICRNL | INPCK | ISTRIP | IXON | IUCLC | IXANY | IXOFF | IXON);
  buf.c_cflag &= ~(CSIZE | PARENB);
  buf.c_oflag &= ~(OCRNL | OLCUC | ONLCR | OPOST);
  buf.c_cflag |= CS8;
  buf.c_cc[VMIN] = 1;
  buf.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0) {
    k_printf("KBD: Setting to RAW mode failed.\n");
    return (-1);
  }
  if (tcgetattr(fd, &buf) < 0) {
    k_printf("Termios ERROR\n");
  }
  k_printf("KBD: Setting TERMIOS Structure.\n");
  print_termios(buf);

  return (0);
}



#ifdef USE_SLANG_KEYS
#include "slang-keyboard.c"
#endif




void
getKeys(void)
{
  int cc;

  if (config.console_keyb) {	/* Keyboard at the console */
    if (scr_state.current != 1) {
	k_printf("KBD: Not current screen!\n");
	return;
    }
    kbcount = 0;
    kbp = kbbuf;

    cc = RPT_SYSCALL(read(kbd_fd, &kbp[kbcount], KBBUF_SIZE - 1));
    k_printf("KBD: cc found %d characters (Raw)\n", cc);
    if (cc == -1)
      return;

    if (cc > 0) {
      int i;

      for (i = 0; i < cc; i++) {
	k_printf("KEY: readcode: %d \n", kbp[kbcount + i]);
	child_set_flags(kbp[kbcount + i]);
	add_scancode_to_queue(kbp[kbcount + i]);
	k_printf("KBD: cc pushing %d'th character\n", i);
      }
    }
  }
  else {			/* Not keyboard at the console (not config.console_keyb) */

#ifdef USE_SLANG_KEYS
     do_slang_getkeys ();
#else
    if (kbcount == 0)
      kbp = kbbuf;
    else if (kbp > &kbbuf[(KBBUF_SIZE * 3) / 5]) {
      memmove(kbbuf, kbp, kbcount);
      kbp = kbbuf;
    }

    cc = RPT_SYSCALL(read(kbd_fd, &kbp[kbcount], KBBUF_SIZE - kbcount - 1));
    k_printf("KBD: cc found %d characters (Xlate)\n", cc);
    if (cc == -1)
      return;

    if (cc > 0) {
      if (kbp + kbcount + cc > &kbbuf[KBBUF_SIZE])
	error("ERROR: getKeys() has overwritten the buffer!\n");
      kbcount += cc;
      while (cc) {
	k_printf("KBD: Converting cc=%d\n", cc);
	convascii(&cc);
	k_printf("KBD: Converted\n");
      }
    }
#endif  /* NOT SLANG */
  }
}

void
child_set_flags(int sc)
{
  switch (sc) {
  case 0xe0:
  case 0xe1:
    child_set_kbd_flag(4);
    return;
  case 0x2a:
    if (!child_kbd_flag(4))
      child_set_kbd_flag(1);
    child_clr_kbd_flag(4);
    return;
  case 0x36:
    child_set_kbd_flag(1);
    child_clr_kbd_flag(4);
    return;
  case 0x1d:
  case 0x10:
    child_set_kbd_flag(2);
    child_clr_kbd_flag(4);
    return;
  case 0x38:
    if (!child_kbd_flag(4))
      child_set_kbd_flag(3);
    child_clr_kbd_flag(4);
    return;
  case 0xaa:
    if (!child_kbd_flag(4))
      child_clr_kbd_flag(1);
    child_clr_kbd_flag(4);
    return;
  case 0xb6:
    child_clr_kbd_flag(4);
    child_clr_kbd_flag(1);
    return;
  case 0x9d:
  case 0x90:
    child_clr_kbd_flag(4);
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
    child_clr_kbd_flag(4);
    if ( !config.X && 
	 (child_kbd_flag(3) && child_kbd_flag(2) && !child_kbd_flag(1)) ) {
      int fnum = sc - 0x3a;

      k_printf("KDB: Doing VC switch\n");
      if (fnum > 10)
	fnum -= 0x12;		/* adjust if f11 or f12 */

      /* can't just do the ioctl() here, as ReadKeyboard will probably have
 * been called from a signal handler, and ioctl() is not reentrant.
 * hence the delay until out of the signal handler...
 */
      child_clr_kbd_flag(3);
      child_clr_kbd_flag(2);
      activate(fnum);
      return;
    }
    return;
  case 0x51:
    if ( !config.X &&
	 (child_kbd_flag(2) && child_kbd_flag(3) && !child_kbd_flag(1)) ) {
      dbug_printf("ctrl-alt-pgdn\n");
      leavedos(42);
    }
    child_clr_kbd_flag(4);
    return;
  default:
    child_clr_kbd_flag(4);
    return;
  }
}

#ifndef USE_SLANG_KEYS
/*
 * DANG_BEGIN_FUNCTION convascii
 *
 * arguments:
 *  cc - count of characters on queue
 *
 * description:
 *  For dealing with translating esc character sequences received 
 * in XLATE mode. Every effort is made to give the characters a time 
 * slice of approx. 3/4 of a second to arrive at DOSEMU.
 * Also handles alt-keys and other character translations.
 *
 * DANG_END_FUNCTION
 *
 */
static void
convascii(int *cc)
{
  /* get here only if in cooked mode (i.e. K_XLATE) */
  int i;
  static struct timeval scr_tv, t_start, t_end;
  static struct funkeystruct *fkp;
  static fd_set fds;
  static int handles;
  static long t_dif;

  if (*kbp == '\033') {
    int ccc;

    fkp = funkey;
    i = 1;
    k_printf("About to do\n");
    do {
#define WAITTIME 750000L
      if (kbcount == i) {
	FD_ZERO(&fds);
	FD_SET(kbd_fd, &fds);
	scr_tv.tv_sec = 0L;
	scr_tv.tv_usec = WAITTIME;
	handles = 0;
	gettimeofday(&t_start, NULL);
	while ((int) select(kbd_fd + 1, &fds, NULL, NULL, &scr_tv) < (int) 1) {
	  gettimeofday(&t_end, NULL);
	  t_dif = ((t_end.tv_sec * 1000000 + t_end.tv_usec) -
		   (t_start.tv_sec * 1000000 + t_start.tv_usec));
	  if (t_dif >= WAITTIME || errno != EINTR)
	    break;
	  scr_tv.tv_sec = 0L;
	  scr_tv.tv_usec = WAITTIME - t_dif;
	}
      }
      ccc = RPT_SYSCALL(read(kbd_fd, &kbp[kbcount], KBBUF_SIZE - kbcount - 1));
      if (ccc > 0) {
	kbcount += ccc;
	*cc += ccc;
      }
      if (fkp->esc == NULL || (unsigned char) fkp->esc[i] < kbp[i]) {
	fkp++;
	if (!fkp->code) {
	  break;
	}
      }
      else if ((unsigned char) fkp->esc[i] == kbp[i]) {
	if (fkp->esc[++i] == '\0') {
	  kbcount -= i;
	  *cc -= i;
	  kbp += i;
	  add_scancode_to_queue(fkp->code);
	  return;
	}
      }
      else break;
    } while (i < kbcount);

    if (kbcount == 1) {
      kbcount--;
      (*cc)--;
      add_scancode_to_queue((highscan[*kbp] << 8) + (unsigned char) *kbp++);
      return;
    }

  }				/* if (*kbp == '\033') */

  else if (*kbp >= 128) {
    for (fkp = xfunkey; fkp->code; fkp++) {
      if ((unsigned char) (*kbp) == fkp->esc[0]) {
	add_scancode_to_queue(fkp->code);
	break;
      }
    }
    kbcount--;
    (*cc)--;
    kbp++;
    return;
  }				/* if (*kbp >= 128) */

  else if (*kbp == erasekey) {
    kbcount--;
    (*cc)--;
    kbp++;
    add_scancode_to_queue(((unsigned char) highscan[8] << 8) + (unsigned char) 8);
    return;
  }

  i = highscan[*kbp] << 8;	/* get scancode */

  /* extended scancodes return 0 for the ascii value */
  if ((unsigned char) *kbp < 0x80)
    i |= (unsigned char) *kbp;

  add_scancode_to_queue(i);
  kbcount--;
  (*cc)--;
  kbp++;
}
#endif /* NOT USE_SLANG_KEYS */

/* InsKeyboard
 *  returns 1 if a character could be inserted into Kbuffer
 */
static int
InsKeyboard(unsigned short scancode)
{
  unsigned short nextpos;

  /* First of all compute the position of the new tail pointer */
  if ((nextpos = READ_WORD(KBD_TAIL) + 2) >= READ_WORD(KBD_END))
    nextpos = READ_WORD(KBD_START);
  if (nextpos == READ_WORD(KBD_HEAD)){	/* queue full ? */
     if(scancode == 0x2e03) {
        WRITE_BYTE(0x471, 0x80);    /* ctrl-break flag */
        WRITE_WORD(KBD_HEAD, READ_WORD(KBD_START));
        WRITE_WORD(KBD_TAIL, READ_WORD(KBD_START));
        WRITE_WORD(BIOS_DATA_PTR(READ_WORD(KBD_START)), 0);
        k_printf("KBD: cntrl-C received\n");
     	return (1);
     }
     putchar('\007');
     return (0);
  }

  WRITE_WORD(BIOS_DATA_PTR(READ_WORD(KBD_TAIL)), scancode);
  WRITE_WORD(KBD_TAIL, nextpos);

  return 1;
}

/* Translate a scan code to a scancode|character combo */
static  unsigned int
convKey(int scancode)
{

  k_printf("KBD: convKey = 0x%04x\n", scancode);

  if (scancode == 0)
    return 0;

  if (config.console_keyb || config.X) {
    unsigned int tmpcode = 0;

/*    *LASTSCAN_ADD = scancode; */
    WRITE_WORD(LASTSCAN_ADD, scancode);
    tmpcode = convscanKey(scancode);
    return tmpcode;
  }
  return 0;
}

void
dump_kbuffer(void)
{
  int i;
  /* unsigned short *ptr = BIOS_DATA_PTR(KBD_Start); */
  unsigned short *ptr = BIOS_DATA_PTR(READ_WORD(KBD_START));

  k_printf("KEYBUFFER DUMP: 0x%02x 0x%02x\n",
/*	   KBD_Head - KBD_Start, KBD_Tail - KBD_Start);*/
	   READ_WORD(KBD_HEAD) - READ_WORD(KBD_START), READ_WORD(KBD_TAIL) - READ_WORD(KBD_START));
  for (i = 0; i < 16; i++)
    /* k_printf("%04x ", ptr[i]); */
    k_printf("%04x ", (unsigned int) READ_WORD(ptr + i));
  k_printf("\n");
}

void
keybuf_clear(void)
{
  ignore_segv++;
/*  KBD_Head = KBD_Tail = KBD_Start;*/
  WRITE_WORD(KBD_HEAD, READ_WORD(KBD_START));
  WRITE_WORD(KBD_TAIL, READ_WORD(KBD_START));
  ignore_segv--;
  dump_kbuffer();
}

#ifndef USE_SLANG_KEYS
static int
fkcmp(const void *a, const void *b)
{
  return strcmp(((struct funkeystruct *) a)->esc, ((struct funkeystruct *) b)->esc);
}
#endif

static void
keyboard_handling_init(void)
{
#ifndef USE_SLANG_KEYS
  int numkeys;
  struct funkeystruct *fkp;
#endif
  scr_state.current = 1;

  if (config.console_keyb) {
    set_raw_mode();
    get_leds();
    set_key_flag(KKF_KBD102);
  }

  if (config.X)
     return;
#ifndef USE_SLANG_KEYS
  setupterm(NULL, 1, (int *) 0);

  /* This won't work with NCURSES version 1.8. */
  /* These routines have been tested with NCURSES version 1.8.5 */
  /* Can someone make this compatible with 1.8? */
  for (fkp = funkey; fkp->code; fkp++) {
    if (fkp->tce != NULL) {
      fkp->esc = tigetstr(fkp->tce);
      k_printf("TERMINFO string %s = %s\n", fkp->tce, fkp->esc);
      /*if (!fkp->esc) error("ERROR: can't get terminfo %s\n", fkp->tce);*/
    }
  }
  numkeys = 0;
  for (fkp = funkey; fkp->code; fkp++) 
    numkeys++;
  qsort(funkey, numkeys, sizeof(struct funkeystruct), &fkcmp);
#else
  if (-1 == init_slang_keymaps ())
    {
       error ("ERROR: Unable to initialize S-Lang keymaps.\n");
       leavedos (31);
    }
#endif
}

/***************************************************************
 * this was copied verbatim from the Linux kernel (keyboard.c) *
 * and then modified unscrupously by hackers on the team       *
 ***************************************************************/

static unsigned char resetid = 0;
static unsigned char firstid = 0;

static unsigned int
convscanKey(unsigned char scancode)
{
  static unsigned char rep = 0xff;

  k_printf("KBD: convscanKey scancode = 0x%04x\n", scancode);

  if (scancode == 0xe0) {
    set_key_flag(KKF_E0);
    set_key_flag(KKF_FIRSTID);
    set_key_flag(KKF_READID);
    resetid = 1;
    firstid = 0;
  }
  else if (scancode == 0xe1) {
    set_key_flag(KKF_E1);
    set_key_flag(KKF_FIRSTID);
    set_key_flag(KKF_READID);
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
  if (key_flag(KKF_E0) && (scancode == 0x2a || scancode == 0xaa)) {
    clr_key_flag(KKF_E0);
    clr_key_flag(KKF_E1);
    clr_key_flag(KKF_FIRSTID);
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
  key_table[scancode] (scancode);

  k_printf("KBD: resetid = %d firstid = %d\n", resetid, firstid);
  if (resetid) {
    if (firstid)
      clr_key_flag(KKF_FIRSTID);
    clr_key_flag(KKF_READID);
    resetid = firstid = 0;
  }

  clr_key_flag(KKF_E0);
  clr_key_flag(KKF_E1);

  return (queue);
}

static void
ctrl(unsigned int sc)
{
  if (key_flag(KKF_E0)) {
    set_key_flag(KKF_RCTRL);
  }
  else
    set_kbd_flag(EKF_LCTRL);

  set_kbd_flag(KF_CTRL);
}

static void
alt(unsigned int sc)
{
  if (key_flag(KKF_E0)) {
    set_key_flag(KKF_RALT);
  }
  else
    set_kbd_flag(EKF_LALT);

  set_kbd_flag(KF_ALT);
}

static void
Unctrl(unsigned int sc)
{
  if (key_flag(KKF_E0)) {
    clr_key_flag(KKF_RCTRL);
  }
  else
    clr_kbd_flag(EKF_LCTRL);

  if (!kbd_flag(EKF_LCTRL) && !key_flag(KKF_RCTRL))
    clr_kbd_flag(KF_CTRL);
}

static void
unalt(unsigned int sc)
{
  /*  if (!resetid) { */
  if (key_flag(KKF_E0)) {
    clr_key_flag(KKF_RALT);
  }
  else
    clr_kbd_flag(EKF_LALT);

  if (!(kbd_flag(EKF_LALT) && key_flag(KKF_RALT))) {
    clr_kbd_flag(KF_ALT);

    /* this is for hold-alt-and-keypad entry method */
    if (altchar) {
      /* perhaps anding with 0xff is incorrect.  how about
		     * filtering out anything > 255?
		     */
      put_queue(altchar & 0xff);/* just the ASCII */
    }
    altchar = 0;
  }
  /* } */
}

static void
lshift(unsigned int sc)
{
  set_kbd_flag(KF_LSHIFT);
}

static void
unlshift(unsigned int sc)
{
  if (!resetid) {
    clr_kbd_flag(KF_LSHIFT);
  }
}

static void
rshift(unsigned int sc)
{
  set_kbd_flag(KF_RSHIFT);
}

static void
unrshift(unsigned int sc)
{
  if (!resetid) {
    clr_kbd_flag(KF_RSHIFT);
  }
}

static void
caps(unsigned int sc)
{
  if (kbd_flag(KKF_RCTRL) && kbd_flag(EKF_LCTRL)) {
    keyboard_mouse = keyboard_mouse ? 0 : 1;
    m_printf("MOUSE: toggled keyboard mouse %s\n",
	     keyboard_mouse ? "on" : "off");
    return;
  }
  else {
    set_kbd_flag(EKF_CAPSLOCK);
    if (!caps_stat) {
      if (keepkey) {
	chg_kbd_flag(KF_CAPSLOCK);	/* toggle; this means SET/UNSET */
      }
      set_leds();
      caps_stat = 1;
    }
  }
}

static void
uncaps(unsigned int sc)
{
  if (!resetid) {
    clr_kbd_flag(EKF_CAPSLOCK);
    caps_stat = 0;
  }
}

static void
sysreq(unsigned int sc)
{
  g_printf("Regs requested: SYSREQ\n");
  show_regs(__FILE__, __LINE__);
}

static void
ScrollLock(unsigned int sc)
{
  if (key_flag(KKF_E0)) {
    k_printf("KBD: ctrl-break!\n");
    ignore_segv++;
#if 0
    /* *(unsigned char *) 0x471 = 0x80; */	/* ctrl-break flag */
    WRITE_BYTE(0x471, 0x80); 	/* ctrl-break flag */
    KBD_Head = KBD_Tail = KBD_Start;
    /* *((unsigned short *) (BIOS_DATA_PTR(KBD_Start))) = 0;*/
    WRITE_WORD(BIOS_DATA_PTR(KBD_Start), 0);
#endif
    WRITE_BYTE(0x471, 0x80); 	/* ctrl-break flag */
    WRITE_WORD(KBD_HEAD, READ_WORD(KBD_START));
    WRITE_WORD(KBD_TAIL, READ_WORD(KBD_START));
    WRITE_WORD(BIOS_DATA_PTR(READ_WORD(KBD_START)), 0);

    ignore_segv--;
    return;
  }
  else if (kbd_flag(KKF_RCTRL))
    show_ints(0, 0x33);
  else if (kbd_flag(KKF_RALT))
    show_regs(__FILE__, __LINE__);
  else if (kbd_flag(KF_RSHIFT)) {
    warn("timer int 8 requested...\n");
#ifdef NEW_PIC
    pic_request(PIC_IRQ0);
#else
    do_hard_int(8);
#endif
  }
  else if (kbd_flag(KF_LSHIFT)) {
    warn("keyboard int 9 requested...\n");
    dump_kbuffer();
  }
  else {
    set_kbd_flag(EKF_SCRLOCK);
    if (!scroll_stat) {
      if (keepkey) {
	chg_kbd_flag(KF_SCRLOCK);
      }
      set_leds();
      scroll_stat = 1;
    }
  }
}

static void
unscroll(unsigned int sc)
{
  clr_kbd_flag(EKF_SCRLOCK);
  scroll_stat = 0;
}

static void
num(unsigned int sc)
{
  static int lastpause = 0;

  if (kbd_flag(EKF_LCTRL)) {
    k_printf("KBD: PAUSE!\n");
    if (lastpause) {
      I_printf("IPC: waking parent up!\n");
#if 0
      dos_unpause();
#endif
      lastpause = 0;
    }
    else {
      I_printf("IPC: putting parent to sleep!\n");
#if 0
      dos_pause();
#endif
      lastpause = 1;
    }
  }
  else {
    set_kbd_flag(EKF_NUMLOCK);
    if (!num_stat) {
      k_printf("KBD: NUMLOCK!\n");
      if (keepkey) {
	chg_kbd_flag(KF_NUMLOCK);
      }
      k_printf("KBD: kbd=%d\n", kbd_flag(KF_NUMLOCK));
      set_leds();
      num_stat = 1;
    }
  }
}

static void
unnum(unsigned int sc)
{
  num_stat = 0;
  clr_kbd_flag(EKF_NUMLOCK);
}

void
set_leds()
{
  unsigned int led_state = 0;

  if (kbd_flag(KF_SCRLOCK)) {
    led_state |= (1 << LED_SCRLOCK);
    set_key_flag(KKF_SCRLOCK);
  }
  else
    clr_key_flag(KKF_SCRLOCK);
  if (kbd_flag(KF_NUMLOCK)) {
    led_state |= (1 << LED_NUMLOCK);
    set_key_flag(KKF_NUMLOCK);
  }
  else
    clr_key_flag(KKF_NUMLOCK);
  if (kbd_flag(KF_CAPSLOCK)) {
    led_state |= (1 << LED_CAPSLOCK);
    set_key_flag(KKF_CAPSLOCK);
  }
  else
    clr_key_flag(KKF_CAPSLOCK);

  k_printf("KBD: SET_LEDS() called\n");
  do_ioctl(kbd_fd, KDSETLED, led_state);
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
/*	   *(u_char *) 0x496, *(u_char *) 0x497, *(u_char *) 0x417, *(u_char *) 0x418);*/
	   READ_BYTE(0x496), READ_BYTE(0x497), READ_BYTE(0x417), READ_BYTE(0x418));
}

static void
do_self(unsigned int sc)
{
  unsigned char ch;

  if (kbd_flag(KF_ALT)) {
    /* On a german keyboard the Left-Alt- (Alt-) and the Right-Alt-
             (Alt-Gr-) Keys are different. The keys pressed with AltGr
             return the codes defined in keymaps.c in the alt_map.
             Pressed with the Left-Alt-Key they return the normal symbol
             with the alt-modifier. I've tested this with the 4DOS-Alias-
             Command.                  hein@tlaloc.in.tu-clausthal.de       */
    if (config.keyboard == KEYB_DE_LATIN1) {
      if (kbd_flag(EKF_LALT))	/* Left-Alt-Key pressed ?            */
	ch = config.alt_map[0];	/* Return Key with Alt-modifier      */
      else			/* otherwise (this is Alt-Gr)        */
	ch = config.alt_map[sc];/* Return key from alt_map          */
    }
    else			/* or no DE_LATIN1-keyboard          */
      ch = config.alt_map[sc];	/* Return key from alt_map           */

    if ((sc >= 2) && (sc <= 0xb))	/* numbers */
      sc += 0x76;
    else if (sc == 0xd)
      sc = 0x83;		/* = */
    else if (sc == 0xc)
      sc = 0x82;		/* - */
  }

  else if (kbd_flag(KF_LSHIFT) || kbd_flag(KF_RSHIFT) ||
	   kbd_flag(KF_CTRL))
    ch = config.shift_map[sc];
  else
    ch = config.key_map[sc];

#if 0
  if (ch == 0)
    return;
#endif

  if (kbd_flag(KF_CTRL) || kbd_flag(KF_CAPSLOCK))
    if ((ch >= 'a' && ch <= 'z') || (ch >= 224 && ch <= 254))
      ch -= 32;

  if (kbd_flag(KF_CTRL))	/* ctrl */
    ch &= 0x1f;

  k_printf("KBD: sc=%02x, ch=%02x\n", sc, ch);

  put_queue((sc << 8) | ch);
}

static void
spacebar(unsigned int sc)
{
  put_queue(0x3920);
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
unins(unsigned int sc)
{
  ins_stat = 0;
}

static void
cursor(unsigned int sc)
{
  int old_sc;

  old_sc = sc;

  if (sc < 0x47 || sc > 0x53)
    return;

  /* do dos_ctrl_alt_del on C-A-Del and C-A-PGUP */
  if (kbd_flag(KF_CTRL) && kbd_flag(KF_ALT)) {
    if (sc == 0x53 /*del*/  || sc == 0x49 /*pgup*/ )
      dos_ctrl_alt_del();
    if (sc == 0x51) {		/*pgdn*/
      k_printf("KBD: ctrl-alt-pgdn taking her down!\n");
      leavedos(0);
    }
    /* if the arrow keys, or home end, do keyboard mouse */
  }

  if ((keyboard_mouse) && (sc == 0x50 || sc == 0x4b || sc == 0x48 ||
			   sc == 0x4d || sc == 0x47 || sc == 0x4f)) {
    mouse_keyboard(sc);
    return;
  }

  if (sc == 0x52) {
    if (!ins_stat) {
      chg_kbd_flag(KF_INSERT);
      ins_stat = 1;
    }
    else
      return;
  }

  sc -= 0x47;

  /* ENHANCED CURSOR KEYS:  only ctrl and alt may modify them.
   */
  if (key_flag(KKF_E0)) {
    if (kbd_flag(KF_ALT))
      put_queue(alt_cursor[sc]);
    else if (kbd_flag(KF_CTRL))
      put_queue(ctrl_cursor[sc]);
    else
      put_queue(old_sc << 8);
    return;
  }

  /* everything below this must be a keypad key, as the check for KKF_E0
 * above filters out enhanced cursor (gray) keys
 */

  /* this is the hold-alt-and-type numbers thing */
  if (kbd_flag(KF_ALT)) {
    int digit;

    if ((digit = config.num_table[sc] - '0') <= 9)	/* is a number */
      altchar = altchar * 10 + digit;
  }
  else if (kbd_flag(KF_CTRL))
    put_queue(ctrl_cursor[sc]);
  else if (kbd_flag(KF_NUMLOCK) || kbd_flag(KF_LSHIFT)
	   || kbd_flag(KF_RSHIFT))
    put_queue((old_sc << 8) | config.num_table[sc]);
  else
    put_queue(old_sc << 8);
}

static void
backspace(unsigned int sc)
{
  /* should be perfect */
  if (kbd_flag(KF_CTRL))
    put_queue(0x0e7f);
  else if (kbd_flag(KF_ALT))
    put_queue(0x0e00);
  else
    put_queue(0x0e08);
}

static void
Tab(unsigned int sc)
{
  if (kbd_flag(KF_CTRL))
    put_queue(0x9400);
  else if (kbd_flag(KF_ALT))
    put_queue(0xa500);
  else if (kbd_flag(KF_LSHIFT) || kbd_flag(KF_RSHIFT))
    put_queue(sc << 8);
  else
    put_queue(sc << 8 | config.key_map[sc]);
}

static void
func(unsigned int sc)
{
  int fnum = sc - 0x3a;

  if (fnum > 10)
    fnum -= 0xc;		/* adjust if f11 or f12 */

  /* this checks for the VC-switch key sequence */
  if (kbd_flag(EKF_LALT) && !key_flag(KKF_RALT) && !kbd_flag(KF_RSHIFT)
      && !kbd_flag(KF_LSHIFT) && kbd_flag(EKF_LCTRL)) {
    clr_kbd_flag(EKF_LALT);
    clr_kbd_flag(KF_ALT);
    clr_kbd_flag(EKF_LCTRL);
    clr_kbd_flag(KF_CTRL);
    return;
  }

  /* FCH (Fkey CHoose):   returns a if n is f11 or f12, else it returns b
 * PC scancodes for fkeys are "orthogonal" except F11 and F12.
*/

#define FCH(n,a,b) ((n <= 10) ? a : b)

  k_printf("KBD: sc=%x, fnum=%x\n", sc, fnum);

  if (kbd_flag(KF_LSHIFT) || kbd_flag(KF_RSHIFT))
    put_queue((sc + FCH(fnum, 0x19, 0x30)) << 8);

  else if (kbd_flag(KF_CTRL))
    put_queue((sc + FCH(fnum, 0x23, 0x32)) << 8);

  else if (kbd_flag(KF_ALT))
    put_queue((sc + FCH(fnum, 0x2d, 0x34)) << 8);

  else
    put_queue(FCH(fnum, sc, (sc + 0x2e)) << 8);
}

static void
slash(unsigned int sc)
{
  if (!key_flag(KKF_E0)) {
    do_self(sc);
  }
  else
    put_queue(sc << 8 | '/');
}

static void
star(unsigned int sc)
{
  do_self(sc);
}

static void
enter(unsigned int sc)
{
  if (kbd_flag(KF_CTRL))
    put_queue(sc << 8 | 0x0a);
  else if (kbd_flag(KF_ALT))
    put_queue(0xa600);
  else
    put_queue(sc << 8 | 0x0d);
}

static void
minus(unsigned int sc)
{
  do_self(sc);
}

static void
plus(unsigned int sc)
{
  do_self(sc);
}

static void
none(unsigned int sc)
{
}

/**************** key-related functions **************/
/* XXX - we  no longer ignore any changes a user program has made...this
 *       is good.  Now, we can let users mess around
 *       with our ALT flags, as we use no longer use those to change consoles.
 */

void
set_kbd_flag(int flag)
{
  WRITE_WORD(KBDFLAG_ADDR, READ_WORD(KBDFLAG_ADDR) | (1 << flag) );
  /*kbd_flags |= (1 << flag);*/
}

void
clr_kbd_flag(int flag)
{
  WRITE_WORD(KBDFLAG_ADDR, READ_WORD(KBDFLAG_ADDR) & ~(1 << flag) );
  /*kbd_flags &= ~(1 << flag);*/
}

void
chg_kbd_flag(int flag)
{
  WRITE_WORD(KBDFLAG_ADDR, READ_WORD(KBDFLAG_ADDR) ^ (1 << flag) );
  /*kbd_flags ^= (1 << flag);*/
}

int
kbd_flag(int flag)
{
  return ((READ_WORD(KBDFLAG_ADDR) >> flag) & 1);
  /*return ((kbd_flags >> flag) & 1);*/
}

/* These are added to allow the CHILD process to keep its own flags on keyboard
   status */

void
child_set_kbd_flag(int flag)
{
  child_kbd_flags |= (1 << flag);
}

void
child_clr_kbd_flag(int flag)
{
  child_kbd_flags &= ~(1 << flag);
}

int
child_kbd_flag(int flag)
{
  return ((child_kbd_flags >> flag) & 1);
}

/* these are the KEY flags */
void
set_key_flag(int flag)
{
  WRITE_WORD(KEYFLAG_ADDR, READ_WORD(KEYFLAG_ADDR) | (1 << flag));
  /*key_flags |= (1 << flag);*/
}

void
clr_key_flag(int flag)
{
  WRITE_WORD(KEYFLAG_ADDR, READ_WORD(KEYFLAG_ADDR) & ~(1 << flag));
  /*key_flags &= ~(1 << flag);*/
}

void
chg_key_flag(int flag)
{
  WRITE_WORD(KEYFLAG_ADDR, READ_WORD(KEYFLAG_ADDR) ^ (1 << flag));
  /*key_flags ^= (1 << flag);*/
}

int
key_flag(int flag)
{
  return( (READ_WORD(KEYFLAG_ADDR) >> flag) & 1 );
  /*return ((key_flags >> flag) & 1);*/
}

/************* end of key-related functions *************/

#define SCANQ_LEN 100
static u_short *scan_queue;
static int *scan_queue_start;
static int *scan_queue_end;

void shared_keyboard_init(void) {
  (u_char *)scan_queue = shared_qf_memory+SHARED_KEYBOARD_OFFSET + 8;
  (u_char *)scan_queue_start = shared_qf_memory+SHARED_KEYBOARD_OFFSET;
  (u_char *)scan_queue_end = shared_qf_memory+SHARED_KEYBOARD_OFFSET + 4;
  *scan_queue_start = 0;
  *scan_queue_end = 0;
}

#ifdef NEW_PIC
/* This is used to run the keyboard interrupt */
void
do_irq1(void) {
   read_next_scancode_from_queue();
  /* reschedule if queue not empty */
  if (*scan_queue_start!=*scan_queue_end) { 
    k_printf("KBD: Requesting next keyboard interrupt startstop %d:%d\n", 
      *scan_queue_start, *scan_queue_end);
    pic_request(PIC_IRQ1); 
  /*  *LASTSCAN_ADD=1; */
    keys_ready = 0;
  }
   do_irq(); /* do dos interrupt */
   keys_ready = 0;	/* flag *LASTSCAN_ADDR empty	*/
}
#endif

void
scan_to_buffer(void) {
  k_printf("scan_to_buffer LASTSCAN_ADD = 0x%04x\n", READ_WORD(LASTSCAN_ADD));
  set_keyboard_bios();
  insert_into_keybuffer();
}

void
add_scancode_to_queue(u_short scan)
{
  k_printf("DOS got set scan %04x, startq=%d, endq=%d\n", scan, *scan_queue_start, *scan_queue_end);
  scan_queue[*scan_queue_end] = scan;
  *scan_queue_end = (*scan_queue_end + 1) % SCANQ_LEN;
  if (config.keybint) {
    k_printf("Hard queue\n");
#ifdef NEW_PIC
    pic_request(PIC_IRQ1);
#else
    do_hard_int(9);
#endif
  }
  else {
    k_printf("NOT Hard queue\n");
    read_next_scancode_from_queue();
    scan_to_buffer();
#ifdef NEW_PIC
 /*   *LASTSCAN_ADD=1; */
    keys_ready = 0;
#endif
  }
  add_key_depress(scan );

}

void
add_key_depress(u_short scan){
  if (!config.console_keyb && !config.X) {
    static u_short tmp_scan;
    tmp_scan = scan & 0xff00;
    if (tmp_scan < 0x8000 && tmp_scan > 0) {
      k_printf("Adding Key-Release\n");
      add_scancode_to_queue(scan | 0x8000);
    }
  }
}

void
keyboard_flags_init(void){
}

static int lastchr;
static int inschr;

void
set_keyboard_bios(void)
{
  int scan;

  if (config.console_keyb) {
    if (config.keybint) {
      keepkey = 1;
      inschr = convKey(HI(ax));
    }
    else
/*      inschr = convKey(*LASTSCAN_ADD);*/
      inschr = convKey(READ_WORD(LASTSCAN_ADD));

#ifdef NEW_PIC
 /*     *LASTSCAN_ADD=1;   /* flag character as read */
      keys_ready = 0;	/* flag character as read	*/
#endif
  } else if (config.X) {
	  if (config.keybint) {
		  keepkey = 1;
		  if (config.X_keycode)
			  inschr = convKey(HI(ax));
		  else {
			  /* check, if we have to store the key in the keyboard buffer */
			  if ((scan=convKey(HI(ax))) || ((lastchr>>8) && !(lastchr&0x80)) )
				  /* xdos gives us the char-code, so use it.
					  We'd get into trouble, if we would try to convert the
					  scan code into a char while using a non-US keyboard! */
				  inschr = (lastchr>>8) | (scan&0xff00);
			  else
				  inschr = 0;
		  }
	  } else
	    if(config.X_keycode)
		  /* inschr = convKey(*LASTSCAN_ADD); */
		  inschr = convKey(READ_WORD(LASTSCAN_ADD));
	    else {
		  /* if(*LASTSCAN_ADD & 0x80) */
		  if(READ_WORD(LASTSCAN_ADD) & 0x80)
			inschr = 0x0;
		  else
		  	/* inschr = *LASTSCAN_ADD >> 8; */
		  	inschr = READ_WORD(LASTSCAN_ADD) >> 8;
		  k_printf("KBD: non-keybint X inschr = 0x%x\n", inschr);
	    }
#ifdef NEW_PIC
/*                  *LASTSCAN_ADD=1;   /* flag character as read */
                  keys_ready = 0;	/* flag character as read	*/
#endif
  } else {
    inschr = lastchr;
    if (inschr > 0x7fff) {
      inschr = 0;
    }
  }

/*  k_printf("set keybaord bios inschr=0x%04x, lastchr = 0x%04x, *LASTSCAN_ADD = 0x%04x\n", inschr, lastchr, *LASTSCAN_ADD); */
  k_printf("set keybaord bios inschr=0x%04x, lastchr = 0x%04x, *LASTSCAN_ADD = 0x%04x\n", inschr, lastchr, READ_WORD(LASTSCAN_ADD));
  k_printf("MOVING   key 96 0x%02x, 97 0x%02x, kbc1 0x%02x, kbc2 0x%02x\n",
/*	   *(u_char *)KEYFLAG_ADDR , *(u_char *)(KEYFLAG_ADDR +1), *(u_char *)KBDFLAG_ADDR, *(u_char *)(KBDFLAG_ADDR+1));*/
	   READ_BYTE(KEYFLAG_ADDR), READ_BYTE(KEYFLAG_ADDR +1),
           READ_BYTE(KBDFLAG_ADDR), READ_BYTE(KBDFLAG_ADDR+1));

}

void
insert_into_keybuffer(void)
{
  /* int15 fn=4f will reset CF if scan key is not to be used */
/*  if (!config.keybint || LWORD(eflags) & CF) */
  if (!config.keybint || READ_FLAGS() & CF)
    keepkey = 1;
  else
    keepkey = 0;

  k_printf("KBD: Finishing up call\n");

  if (inschr && keepkey) {
    k_printf("IPC/KBD: (child) putting key in buffer\n");
    if (InsKeyboard(inschr))
      dump_kbuffer();
    else
      error("ERROR: InsKeyboard could not put key into buffer!\n");
  }
}

void
read_next_scancode_from_queue(void)
{
#ifndef NEW_PIC
  keys_ready = 0;
#else
  if(!keys_ready) { /* make sure last character has been read */
#endif
  if (*scan_queue_start != *scan_queue_end) {
    keys_ready = 1;
    lastchr = scan_queue[*scan_queue_start];
    if (*scan_queue_start != *scan_queue_end)
      *scan_queue_start = (*scan_queue_start + 1) % SCANQ_LEN;
    if (!config.console_keyb && !config.X) {
       /* *LASTSCAN_ADD = lastchr >> 8; */
       WRITE_WORD(LASTSCAN_ADD, lastchr >> 8);
    }
    else {
       /* *LASTSCAN_ADD = lastchr; */
       WRITE_WORD(LASTSCAN_ADD, lastchr);
    }
  }
#ifdef NEW_PIC
  }
#endif
  else
    k_printf("Parent Nextscan Key not Read!\n");
  k_printf("Parent Nextscan key 96 0x%02x, 97 0x%02x, kbc1 0x%02x, kbc2 0x%02x\n",
	   *(u_char *) 0x496, *(u_char *) 0x497, *(u_char *) 0x417, *(u_char *) 0x418);
/*  k_printf("start=%d, end=%d, LASTSCAN=%x\n", *scan_queue_start, *scan_queue_end, *LASTSCAN_ADD); */
  k_printf("start=%d, end=%d, LASTSCAN=%x\n", *scan_queue_start, *scan_queue_end, READ_WORD(LASTSCAN_ADD));
}
