/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <slang.h>
#include "emu.h"
#include "timers.h"
#include "keymaps.h"
#include "keyb_clients.h"
#include "keyboard.h"
#include "utilities.h"
#include "video.h"

#ifndef VOID
#  define VOID void
#endif

#define KBBUF_SIZE 80

static int kbcount = 0;
static Bit8u kbbuf[KBBUF_SIZE];
static Bit8u *kbp;

static int save_kbd_flags = -1;	/* saved flags for STDIN before our fcntl */
static struct termios save_termios;
static unsigned char erasekey;

#ifndef SLANG_VERSION
# define SLANG_VERSION 1
#endif

/* unsigned char DOSemu_Slang_Escape_Character = 30; */

char *DOSemu_Keyboard_Keymap_Prompt = NULL;
int DOSemu_Terminal_Scroll = 0;
int DOSemu_Slang_Show_Help = 0;
int DOSemu_Slang_Got_Terminfo = 0;

/*
 * The goal of the routines here is simple: to allow SHIFT, ALT, and
 * CONTROL keys to be used with a remote terminal.  The way this is
 * accomplished is simple:  There are 4 keymaps: normal, shift, control,
 * and alt.  The user must a key or keysequence that invokes on of these
 * keymaps.  The default keymap is the normal one.  When one of the other
 * keymaps is invoked via the special key defined for it, the new keymap
 * is only in effect for the next character at which point the normal one
 * becomes active again.
 * 
 * The normal keymap is fairly robust.  It contains mappings for all the
 * ascii characters (0-26, 28-256).  The escape character itself (27) is
 * included only in the sence that it is the prefix character for arrow
 * keys, function keys, etc...
 */

/*
 * The keymaps are simply a mapping from a character sequence to
 * scan/ascii codes.
 */

typedef struct {
  unsigned char keystr[10];
  unsigned long scan_code;
}
Keymap_Scan_Type;

#define SHIFT_MASK			0x00010000
#define CTRL_MASK			0x00020000
#define ALT_MASK			0x00040000
#define ALTGR_MASK			0x00080000
#define STICKY_SHIFT_MASK		0x00100000
#define STICKY_CTRL_MASK		0x00200000
#define STICKY_ALT_MASK			0x00400000
#define STICKY_ALTGR_MASK		0x00800000
#define KEYPAD_MASK			0x01000000

#define ALT_KEY_SCAN_CODE		0x80000000
#define STICKY_ALT_KEY_SCAN_CODE	0x80000001
#define SHIFT_KEY_SCAN_CODE		0x80000002
#define STICKY_SHIFT_KEY_SCAN_CODE	0x80000003
#define CTRL_KEY_SCAN_CODE		0x80000004
#define STICKY_CTRL_KEY_SCAN_CODE	0x80000005
#define ALTGR_KEY_SCAN_CODE		0x80000006
#define STICKY_ALTGR_KEY_SCAN_CODE	0x80000007

#define SCROLL_UP_SCAN_CODE		0x80000020
#define SCROLL_DOWN_SCAN_CODE		0x80000021
#define REDRAW_SCAN_CODE		0x80000022
#define SUSPEND_SCAN_CODE		0x80000023
#define HELP_SCAN_CODE			0x80000024
#define RESET_SCAN_CODE			0x80000025
#define SET_MONO_SCAN_CODE		0x80000026
#define KEYPAD_KEY_SCAN_CODE		0x80000027

/* Keyboard maps for non-terminfo keys */

static Keymap_Scan_Type Dosemu_defined_fkeys[] =
{
/* These are the F1 - F12 keys for terminals without them. */
  {"^@1", KEY_F1 },			/* F1 */
  {"^@2", KEY_F2 },			/* F2 */
  {"^@3", KEY_F3 },			/* F3 */
  {"^@4", KEY_F4 },			/* F4 */
  {"^@5", KEY_F5 },			/* F5 */
  {"^@6", KEY_F6 },			/* F6 */
  {"^@7", KEY_F7 },			/* F7 */
  {"^@8", KEY_F8 },			/* F8 */
  {"^@9", KEY_F9 },			/* F9 */
  {"^@0", KEY_F10 },			/* F10 */
  {"^@-", KEY_F11 },			/* F11 */
  {"^@=", KEY_F12 },			/* F12 */
  {"\033\033", KEY_ESC },		/* ESC */
  {"", 0}
};

static Keymap_Scan_Type Generic_backspace[] =
{
  {"^H",   KEY_BKSP },			/* Backspace */
  {"\177", KEY_BKSP },
  {"", 0}
};

static Keymap_Scan_Type Meta_ALT[] =
{
/* Alt keys (high bit set) */
  {"\233", KEY_ESC         | ALT_MASK },	/* Alt Esc */
#if 0
  {"\377", KEY_BKSP        | ALT_MASK },	/* Alt Backspace */
#endif
  {"\361", KEY_Q           | ALT_MASK },	/* Alt Q */
  {"\367", KEY_W           | ALT_MASK },	/* Alt W */
  {"\345", KEY_E           | ALT_MASK },	/* Alt E */
  {"\362", KEY_R           | ALT_MASK },	/* Alt R */
  {"\364", KEY_T           | ALT_MASK },	/* Alt T */
  {"\371", KEY_Y           | ALT_MASK },	/* Alt Y */
  {"\365", KEY_U           | ALT_MASK },	/* Alt U */
  {"\351", KEY_I           | ALT_MASK },	/* Alt I */
  {"\357", KEY_O           | ALT_MASK },	/* Alt O */
  {"\360", KEY_P           | ALT_MASK },	/* Alt P */
  {"\333", KEY_LBRACK      | ALT_MASK },	/* Alt [ */
  {"\335", KEY_RBRACK      | ALT_MASK },	/* Alt ] */
  {"\215", KEY_RETURN      | ALT_MASK },	/* Alt Enter */
  {"\341", KEY_A           | ALT_MASK },	/* Alt A */
  {"\363", KEY_S           | ALT_MASK },	/* Alt S */
/* {"\344", KEY_D           | ALT_MASK }, */	/* Alt D *//* This is not (always) true */
  {"\346", KEY_F           | ALT_MASK },	/* Alt F */
  {"\347", KEY_G           | ALT_MASK },	/* Alt G */
  {"\350", KEY_H           | ALT_MASK },	/* Alt H */
  {"\352", KEY_J           | ALT_MASK },	/* Alt J */
  {"\353", KEY_K           | ALT_MASK },	/* Alt K */
  {"\354", KEY_L           | ALT_MASK },	/* Alt L */
  {"\273", KEY_SEMICOLON   | ALT_MASK },	/* Alt ; */
  {"\247", KEY_APOSTROPHE  | ALT_MASK },	/* Alt ' */
  {"\340", KEY_GRAVE       | ALT_MASK },	/* Alt ` */
/* {"\334", KEY_BACKSLASH   | ALT_MASK }, *//* Alt \ *//* This is not (always) true */
  {"\372", KEY_Z           | ALT_MASK },	/* Alt Z */
  {"\370", KEY_X           | ALT_MASK },	/* Alt X */
  {"\343", KEY_C           | ALT_MASK },	/* Alt C */
/* {"\366", KEY_V           | ALT_MASK }, */	/* Alt V *//* This is not (always) true */
  {"\342", KEY_B           | ALT_MASK },	/* Alt B */
  {"\356", KEY_N           | ALT_MASK },	/* Alt N */
  {"\355", KEY_M           | ALT_MASK },	/* Alt M */
  {"\254", KEY_COMMA       | ALT_MASK },	/* Alt , */
  {"\256", KEY_PERIOD      | ALT_MASK },	/* Alt . */
  {"\257", KEY_SLASH       | ALT_MASK },	/* Alt / */
  {"\261", KEY_1           | ALT_MASK },	/* Alt 1 */
  {"\262", KEY_2           | ALT_MASK },	/* Alt 2 */
  {"\263", KEY_3           | ALT_MASK },	/* Alt 3 */
  {"\264", KEY_4           | ALT_MASK },	/* Alt 4 */
  {"\265", KEY_5           | ALT_MASK },	/* Alt 5 */
  {"\266", KEY_6           | ALT_MASK },	/* Alt 6 */
  {"\267", KEY_7           | ALT_MASK },	/* Alt 7 */
  {"\270", KEY_8           | ALT_MASK },	/* Alt 8 */
  {"\271", KEY_9           | ALT_MASK },	/* Alt 9 */
  {"\260", KEY_0           | ALT_MASK },	/* Alt 0 */
  {"\255", KEY_DASH        | ALT_MASK },	/* Alt - */
  {"\275", KEY_EQUALS      | ALT_MASK },	/* Alt = */
  {"\211", KEY_TAB         | ALT_MASK },	/* Alt Tab */
  {"", 0}
};

static Keymap_Scan_Type Esc_ALT[] =
{
/* Another form of alt keys */
  {"\033q", KEY_Q          | ALT_MASK },	/* Alt Q */
  {"\033w", KEY_W          | ALT_MASK },	/* Alt W */
  {"\033e", KEY_E          | ALT_MASK },	/* Alt E */
  {"\033r", KEY_R          | ALT_MASK },	/* Alt R */
  {"\033t", KEY_T          | ALT_MASK },	/* Alt T */
  {"\033y", KEY_Y          | ALT_MASK },	/* Alt Y */
  {"\033u", KEY_U          | ALT_MASK },	/* Alt U */
  {"\033i", KEY_I          | ALT_MASK },	/* Alt I */
  {"\033o", KEY_O          | ALT_MASK },	/* Alt O */
  {"\033p", KEY_P          | ALT_MASK },	/* Alt P */
  {"\033\015", KEY_RETURN  | ALT_MASK },	/* Alt Enter */
  {"\033a", KEY_A          | ALT_MASK },	/* Alt A */
  {"\033s", KEY_S          | ALT_MASK },	/* Alt S */
  {"\033d", KEY_D          | ALT_MASK },	/* Alt D */
  {"\033f", KEY_F          | ALT_MASK },	/* Alt F */
  {"\033g", KEY_G          | ALT_MASK },	/* Alt G */
  {"\033h", KEY_H          | ALT_MASK },	/* Alt H */
  {"\033j", KEY_J          | ALT_MASK },	/* Alt J */
  {"\033k", KEY_K          | ALT_MASK },	/* Alt K */
  {"\033l", KEY_L          | ALT_MASK },	/* Alt L */
  {"\033;", KEY_SEMICOLON  | ALT_MASK },	/* Alt ; */
  {"\033'", KEY_APOSTROPHE | ALT_MASK },	/* Alt ' */
  {"\033`", KEY_GRAVE      | ALT_MASK },	/* Alt ` */
  {"\033\\", KEY_BACKSLASH | ALT_MASK },	/* Alt \ */
  {"\033z", KEY_Z          | ALT_MASK },	/* Alt Z */
  {"\033x", KEY_X          | ALT_MASK },	/* Alt X */
  {"\033c", KEY_C          | ALT_MASK },	/* Alt C */
  {"\033v", KEY_V          | ALT_MASK },	/* Alt V */
  {"\033b", KEY_B          | ALT_MASK },	/* Alt B */
  {"\033n", KEY_N          | ALT_MASK },	/* Alt N */
  {"\033m", KEY_M          | ALT_MASK },	/* Alt M */
  {"\033,", KEY_COMMA      | ALT_MASK },	/* Alt , */
  {"\033.", KEY_PERIOD     | ALT_MASK },	/* Alt . */
  {"\033/", KEY_SLASH      | ALT_MASK },	/* Alt / */
  {"\0331", KEY_1          | ALT_MASK },	/* Alt 1 */
  {"\0332", KEY_2          | ALT_MASK },	/* Alt 2 */
  {"\0333", KEY_3          | ALT_MASK },	/* Alt 3 */
  {"\0334", KEY_4          | ALT_MASK },	/* Alt 4 */
  {"\0335", KEY_5          | ALT_MASK },	/* Alt 5 */
  {"\0336", KEY_6          | ALT_MASK },	/* Alt 6 */
  {"\0337", KEY_7          | ALT_MASK },	/* Alt 7 */
  {"\0338", KEY_8          | ALT_MASK },	/* Alt 8 */
  {"\0339", KEY_9          | ALT_MASK },	/* Alt 9 */
  {"\0330", KEY_0          | ALT_MASK },	/* Alt 0 */
  {"\033-", KEY_DASH       | ALT_MASK },	/* Alt - */
  {"\033=", KEY_EQUALS     | ALT_MASK },	/* Alt = */
  {"\033\011", KEY_TAB     | ALT_MASK },	/* Alt Tab */
  {"", 0}
};

static Keymap_Scan_Type Linux_Keypad[] =
{
/* Keypad keys */
#if 0
  {"\033OP", KEY_NUMLOCK },		/* Keypad Numlock */
#endif
  {"\033OQ", KEY_PAD_SLASH },		/* Keypad / */
  {"\033OR", KEY_PAD_AST },		/* Keypad * */
  {"\033OS", KEY_PAD_MINUS },		/* Keypad - */
  {"", 0}
};

static Keymap_Scan_Type vtxxx_Keypad[] =
{
  {"\033Ow", KEY_PAD_7 },		/* Keypad 7 */
  {"\033Ox", KEY_PAD_8 },		/* Keypad 8 */
  {"\033Oy", KEY_PAD_9 },		/* Keypad 9 */
  {"\033Ot", KEY_PAD_4 },		/* Keypad 4 */
  {"\033Ou", KEY_PAD_5 },		/* Keypad 5 */
  {"\033Ov", KEY_PAD_6 },		/* Keypad 6 */
  {"\033Ol", KEY_PAD_PLUS },		/* Keypad + */
  {"\033Oq", KEY_PAD_1 },		/* Keypad 1 */
  {"\033Or", KEY_PAD_2 },		/* Keypad 2 */
  {"\033Os", KEY_PAD_3 },		/* Keypad 3 */
  {"\033Op", KEY_PAD_0 },		/* Keypad 0 */
  {"\033On", KEY_PAD_DECIMAL },		/* Keypad . */
  {"\033OM", KEY_PAD_ENTER },		/* Keypad Enter */
  {"", 0}
};

static Keymap_Scan_Type Linux_Xkeys[] =
{
  {"\033[2~",  KEY_INS },		/* Ins */
  {"\033[3~",  KEY_DEL },		/* Del    Another keyscan is 0x007F */
  {"\033[1~",  KEY_HOME },		/* Ho     Another keyscan is 0x5c00 */
  {"\033[4~",  KEY_END },		/* End    Another keyscan is 0x6100 */
  {"\033[5~",  KEY_PGUP },		/* PgUp */
  {"\033[6~",  KEY_PGDN },		/* PgDn */
  {"\033[A",   KEY_UP },		/* Up */
  {"\033[B",   KEY_DOWN },		/* Dn */
  {"\033[C",   KEY_RIGHT },		/* Ri */
  {"\033[D",   KEY_LEFT },		/* Le */
  {"", 0}
};

static Keymap_Scan_Type Xterm_Xkeys[] =
{
  {"\033[2~",  KEY_INS },		/* Ins */
#if 0
  {"\177",     KEY_DEL },		/* Del  Same as backspace! */
#endif
  {"\033[H",   KEY_HOME },		/* Ho     (rxvt)  */
#if 0
  {"\033[^@",  KEY_HOME },		/* Ho     (xterm) Hmm, Would this work. */
#endif
  {"\033Ow",   KEY_END },		/* End    (rxvt) */
  {"\033[e",   KEY_END },		/* End    (color_xterm) */
  {"\033[K",   KEY_END },		/* End  - Where does this come from ? */
  {"\033[5~",  KEY_PGUP },		/* PgUp */
  {"\033[6~",  KEY_PGDN },		/* PgDn */

  {"\033[7~",  KEY_HOME },		/* Ho     (xterm) */
  {"\033[8~",  KEY_END },		/* End    (xterm) */

  {"\033[A",   KEY_UP },		/* Up */
  {"\033[B",   KEY_DOWN },		/* Dn */
  {"\033[C",   KEY_RIGHT },		/* Ri */
  {"\033[D",   KEY_LEFT },		/* Le */
  {"", 0}
};

static Keymap_Scan_Type vtxxx_fkeys[] =
{
  {"\033[17~", KEY_F6 },		/* F6 */
  {"\033[18~", KEY_F7 },		/* F7 */
  {"\033[19~", KEY_F8 },		/* F8 */
  {"\033[20~", KEY_F9 },		/* F9 */
  {"\033[21~", KEY_F10 },		/* F10 */
  {"\033[23~", KEY_F1  | SHIFT_MASK },	/* Shift F1  (F11 acts like
					 			* Shift-F1) */
  {"\033[24~", KEY_F2  | SHIFT_MASK },	/* Shift F2  (F12 acts like
								 * Shift-F2) */
  {"\033[25~", KEY_F3  | SHIFT_MASK },	/* Shift F3 */
  {"\033[26~", KEY_F4  | SHIFT_MASK },	/* Shift F4 */
  {"\033[28~", KEY_F5  | SHIFT_MASK },	/* Shift F5 */
  {"\033[29~", KEY_F6  | SHIFT_MASK },	/* Shift F6 */
  {"\033[31~", KEY_F7  | SHIFT_MASK },	/* Shift F7 */
  {"\033[32~", KEY_F8  | SHIFT_MASK },	/* Shift F8 */
  {"\033[33~", KEY_F9  | SHIFT_MASK },	/* Shift F9 */
  {"\033[34~", KEY_F10 | SHIFT_MASK },	/* Shift F10 */
  {"", 0}
};

static Keymap_Scan_Type Xterm_fkeys[] =
{
  {"\033[11~", KEY_F1 },		/* F1 */
  {"\033[12~", KEY_F2 },		/* F2 */
  {"\033[13~", KEY_F3 },		/* F3 */
  {"\033[14~", KEY_F4 },		/* F4 */
  {"\033[15~", KEY_F5 },		/* F5 */
  {"", 0}
};

static Keymap_Scan_Type Linux_fkeys[] =
{
  {"\033[[A",  KEY_F1 },		/* F1 */
  {"\033[[B",  KEY_F2 },		/* F2 */
  {"\033[[C",  KEY_F3 },		/* F3 */
  {"\033[[D",  KEY_F4 },		/* F4 */
  {"\033[[E",  KEY_F5 },		/* F5 */
  {"", 0}
};

static Keymap_Scan_Type vtxxx_xkeys[] =
{
/* Who knows which mode it'll be in */
  {"\033OA",   KEY_UP },		/* Up */
  {"\033OB",   KEY_DOWN },		/* Dn */
  {"\033OC",   KEY_RIGHT },		/* Ri */
  {"\033OD",   KEY_LEFT },		/* Le */
  {"\033[A",   KEY_UP },		/* Up */
  {"\033[B",   KEY_DOWN },		/* Dn */
  {"\033[C",   KEY_RIGHT },		/* Ri */
  {"\033[D",   KEY_LEFT },		/* Le */
  {"", 0}
};

static Keymap_Scan_Type rxvt_alt_keys[] =
{
  {"\033\033[11~", ALT_MASK | KEY_F1 },		/* F1 */
  {"\033\033[12~", ALT_MASK | KEY_F2 },		/* F2 */
  {"\033\033[13~", ALT_MASK | KEY_F3 },		/* F3 */
  {"\033\033[14~", ALT_MASK | KEY_F4 },		/* F4 */
  {"\033\033[15~", ALT_MASK | KEY_F5 },		/* F5 */
  {"\033\033[17~", ALT_MASK | KEY_F6 },		/* F6 */
  {"\033\033[18~", ALT_MASK | KEY_F7 },		/* F7 */
  {"\033\033[19~", ALT_MASK | KEY_F8 },		/* F8 */
  {"\033\033[20~", ALT_MASK | KEY_F9 },		/* F9 */
  {"\033\033[21~", ALT_MASK | KEY_F10 },		/* F10 */
  {"\033\033[23~", ALT_MASK | KEY_F1  | SHIFT_MASK },	/* Shift F1  (F11 acts like
					 			* Shift-F1) */
  {"\033\033[24~", ALT_MASK | KEY_F2  | SHIFT_MASK },	/* Shift F2  (F12 acts like
								 * Shift-F2) */
  {"\033\033[25~", ALT_MASK | KEY_F3  | SHIFT_MASK },	/* Shift F3 */
  {"\033\033[26~", ALT_MASK | KEY_F4  | SHIFT_MASK },	/* Shift F4 */
  {"\033\033[28~", ALT_MASK | KEY_F5  | SHIFT_MASK },	/* Shift F5 */
  {"\033\033[29~", ALT_MASK | KEY_F6  | SHIFT_MASK },	/* Shift F6 */
  {"\033\033[31~", ALT_MASK | KEY_F7  | SHIFT_MASK },	/* Shift F7 */
  {"\033\033[32~", ALT_MASK | KEY_F8  | SHIFT_MASK },	/* Shift F8 */
  {"\033\033[33~", ALT_MASK | KEY_F9  | SHIFT_MASK },	/* Shift F9 */
  {"\033\033[34~", ALT_MASK | KEY_F10 | SHIFT_MASK },	/* Shift F10 */

  {"\033\033[2~",  ALT_MASK | KEY_INS },		/* Ins */
  {"\033\177",     ALT_MASK | KEY_DEL },		/* Del */
  {"\033\033[H",   ALT_MASK | KEY_HOME },		/* Ho     (rxvt)  */
  {"\033\033Ow",   ALT_MASK | KEY_END },		/* End    (rxvt) */
  {"\033\033[5~",  ALT_MASK | KEY_PGUP },		/* PgUp */
  {"\033\033[6~",  ALT_MASK | KEY_PGDN },		/* PgDn */
  {"\033\033[A",   ALT_MASK | KEY_UP },			/* Up */
  {"\033\033[B",   ALT_MASK | KEY_DOWN },		/* Dn */
  {"\033\033[C",   ALT_MASK | KEY_RIGHT },		/* Ri */
  {"\033\033[D",   ALT_MASK | KEY_LEFT },		/* Le */
  {"", 0}
};

static Keymap_Scan_Type vtxxx_pfkey[] =
{
/* Keypad keys */
  {"\033OP", KEY_F1 },		/* PF1 */
  {"\033OQ", KEY_F2 },		/* PF2 */
  {"\033OR", KEY_F3 },		/* PF3 */
  {"\033OS", KEY_F4 },		/* PF4 */
  {"", 0}
};

/* Keyboard map for terminfo keys */
static Keymap_Scan_Type terminfo_keys[] =
{
#if SLANG_VERSION > 9934
   {"^(kb)",	KEY_BKSP},	       /* BackSpace */
   {"^(k1)",	KEY_F1},	       /* F1 */
   {"^(k2)",	KEY_F2},	       /* F2 */
   {"^(k3)",	KEY_F3},	       /* F3 */
   {"^(k4)",	KEY_F4},	       /* F4 */
   {"^(k5)",	KEY_F5},	       /* F5 */
   {"^(k6)",	KEY_F6},	       /* F6 */
   {"^(k7)",	KEY_F7},	       /* F7 */
   {"^(k8)",	KEY_F8},	       /* F8 */
   {"^(k9)",	KEY_F9},	       /* F9 */
   {"^(k;)",	KEY_F10},	       /* F10 */
   {"^(F1)",	KEY_F11},	       /* F11 */
   {"^(F2)",	KEY_F12},	       /* F12 */
   {"^(kI)",	KEY_INS},	       /* Ins */
   {"^(#3)",	KEY_INS|SHIFT_MASK},   /* Shift Insert */
   {"^(kD)",	KEY_DEL},	       /* Del */
   {"^(*5)",	KEY_DEL|SHIFT_MASK},   /* Shift Del */
   {"^(kh)",	KEY_HOME},	       /* Ho */
   {"^(#2)",	KEY_HOME|SHIFT_MASK},  /* Shift Home */
   {"^(kH)",	KEY_END},	       /* End */
   {"^(@7)",	KEY_END},	       /* End */
   {"^(*7)",	KEY_END|SHIFT_MASK},   /* Shift End */
   {"^(kP)",	KEY_PGUP},	       /* PgUp */
   {"^(kN)",	KEY_PGDN},	       /* PgDn */
   {"^(K1)",	KEY_PAD_7},	       /* Upper Left key on keypad */
   {"^(ku)",	KEY_UP},	       /* Up */
   {"^(K3)",	KEY_PAD_9},	       /* Upper Right key on keypad */
   {"^(kl)",	KEY_LEFT},	       /* Le */
   {"^(#4)",	KEY_LEFT|SHIFT_MASK},  /* Shift Left */
   {"^(K2)",	KEY_PAD_5},	       /* Center key on keypad */
   {"^(kr)",	KEY_RIGHT},	       /* Ri */
   {"^(K4)",	KEY_PAD_1},	       /* Lower Left key on keypad */
   {"^(kd)",	KEY_DOWN},	       /* Dn */
   {"^(K5)",	KEY_PAD_3},	       /* Lower Right key on keypad */
   {"^(%i)",	KEY_RIGHT|SHIFT_MASK}, /* Shift Right */
   {"^(kB)",	KEY_TAB|SHIFT_MASK},   /* Shift Tab -- BackTab */
   {"^(@8)",	KEY_PAD_ENTER}, /* KEY_RETURN? */	/* Enter */

   /* Special keys */
   {"^(&2)",	REDRAW_SCAN_CODE},	/* Refresh */
   {"^(%1)",	HELP_SCAN_CODE},	/* Help */
#endif
   {"", 0}
};

static Keymap_Scan_Type Dosemu_Xkeys[] =
{
/* These keys are laid out like the numbers on the keypad - not too difficult */
  {"^@K0",  KEY_INS },		/* Ins */
  {"^@K1",  KEY_END },		/* End    Another keyscan is 0x6100 */
  {"^@K2",  KEY_DOWN },		/* Dn */
  {"^@K3",  KEY_PGDN },		/* PgDn */
  {"^@K4",  KEY_LEFT },		/* Le */
  {"^@K5",  KEY_PAD_5 },	/* There's no Xkey equlivant */
  {"^@K6",  KEY_RIGHT },	/* Ri */
  {"^@K7",  KEY_HOME },		/* Ho     Another keyscan is 0x5c00 */
  {"^@K8",  KEY_UP },		/* Up */
  {"^@K9",  KEY_PGUP },		/* PgUp */
  {"^@K.",  KEY_DEL },		/* Del    Another keyscan is 0x007F */
  {"^@Kd",  KEY_DEL },		/* Del */

  /* And a few more */
  {"^@Kh",  KEY_PAUSE },	/* Hold or Pause DOS */
  {"^@Kp",  KEY_PRTSCR },	/* Print screen, SysRequest. */
  {"^@Ky",  KEY_SYSRQ },	/* SysRequest. */
  {"^@KS",  KEY_SCROLL },	/* Scroll Lock */
  {"^@KN",  KEY_NUM },		/* Num Lock */

  {"", 0}
};

static Keymap_Scan_Type Dosemu_Ctrl_keys[] =
{
  /* Repair some of the mistakes from 'define_key_from_keymap()' */
  /* REMEMBER, we're pretending this is a us-ascii keyboard */
  {"^C",	KEY_BREAK },
  {"*",		KEY_8 | SHIFT_MASK },
  {"+",		KEY_EQUALS | SHIFT_MASK },

  /* Now setup the shift modifier keys */
  {"^@a",	ALT_KEY_SCAN_CODE },
  {"^@c",	CTRL_KEY_SCAN_CODE },
  {"^@s",	SHIFT_KEY_SCAN_CODE },
  {"^@g",	ALTGR_KEY_SCAN_CODE },

  {"^@A",	STICKY_ALT_KEY_SCAN_CODE },
  {"^@C",	STICKY_CTRL_KEY_SCAN_CODE },
  {"^@S",	STICKY_SHIFT_KEY_SCAN_CODE },
  {"^@G",	STICKY_ALTGR_KEY_SCAN_CODE },

  {"^@k",	KEYPAD_KEY_SCAN_CODE },

  {"^@?",	HELP_SCAN_CODE },
  {"^@h",	HELP_SCAN_CODE },

  {"^@^R",	REDRAW_SCAN_CODE },
  {"^@^L",	REDRAW_SCAN_CODE },
  {"^@^Z",	SUSPEND_SCAN_CODE },
  {"^@ ",	RESET_SCAN_CODE },
  {"^@B",	SET_MONO_SCAN_CODE },

  {"^@\033[A",	SCROLL_UP_SCAN_CODE },
  {"^@\033OA",	SCROLL_UP_SCAN_CODE },
  {"^@U",	SCROLL_UP_SCAN_CODE },

  {"^@\033[B",	SCROLL_DOWN_SCAN_CODE },
  {"^@\033OB",	SCROLL_DOWN_SCAN_CODE },
  {"^@D",	SCROLL_DOWN_SCAN_CODE },

  {"", 0}
};

/* This table is for translating the ESC key. */
/* The entries with 0x00 are not going to be used by any sane person. */
Bit8u Ctrl_Char_Scan_Codes[32] =
{
  0x03,			/* ^@ */
  0x1E,			/* ^A */
  0x30,			/* ^B */
  0x2E,			/* ^C */
  0x20,			/* ^D */
  0x12,			/* ^E */
  0x21,			/* ^F */
  0x22,			/* ^G */
  0x23,			/* ^H */
  0x0F,			/* TAB */
  0x24,			/* ^J */
  0x25,			/* ^K */
  0x26,			/* ^L */
  0x00,			/* ^M --- RETURN-- Not used. */
  0x31,			/* ^N */
  0x18,			/* ^O */
  0x25,			/* ^P */
  0x10,			/* ^Q */
  0x13,			/* ^R */
  0x1F,			/* ^S */
  0x14,			/* ^T */
  0x16,			/* ^U */
  0x2F,			/* ^V */
  0x11,			/* ^W */
  0x2D,			/* ^X */
  0x15,			/* ^Y */
  0x2C,			/* ^Z */
  0x00,			/* ^[ --- ESC --- Not used. */
  0x2B,			/* ^\\ */
  0x1B,			/* ^] */
  0x07,			/* ^^ */
  0x0C			/* ^_ */
};


static SLKeyMap_List_Type *The_Normal_KeyMap;
static unsigned char Esc_Char;

#if SLANG_VERSION > 9929
# define SLang_define_key1(s,f,t,m) SLkm_define_key((s), (FVOID_STAR)(f), (m))
#endif

/* Note: Later definitions with the same or a conflicting key sequence fail,
 *  and give an error message, but now don't stop the emulator.
 */
static int define_key(const unsigned char *key, unsigned long scan,
		      SLKeyMap_List_Type * m)
{
  unsigned char buf[16], k1;
  int ret;

#if 0
  k_printf("KBD: define '%s' %02x as %08x\n",strprintable(key),key[0], (int)scan);
#endif

  if (SLang_Error)
    return -1;
  if ((*key == '^') && (Esc_Char != '@')) {
    k1 = key[1];
    if (k1 == Esc_Char)
      return 0;			/* ^Esc_Char is not defined here */
    if (k1 == '@') {
      strcpy(buf, key);
      buf[1] = Esc_Char;
      key = buf;
    }
  }
  ret = SLang_define_key1((unsigned char *)key, (VOID *) scan, SLKEY_F_INTRINSIC, m);
  if (ret == -2) {  /* Conflicting key error, ignore it */
    int i, len;
    k_printf("Conflicting key: ");
    len = strlen(key);
    for(i = 0; i < len; i++) {
      k_printf("%02x", key[i]);
    }
    k_printf(" = %08x\n", (int)scan);
    SLang_Error = 0;
  }
  if (SLang_Error) {
    fprintf(stderr, "Bad key: %s\n", key);
    return -1;
  }
  return 0;
}

static int define_keyset(Keymap_Scan_Type *k, SLKeyMap_List_Type *m)
{
  char *str;

  while ((str = k->keystr), (*str != 0)) {
    define_key(str, k->scan_code, m);
    k++;
  }
  return 0;
}

/* this is a rather bad hack. This makes use of the fact that the keysym values are
 * (mostly) identical to the hardware scancodes so we can use the keymaps.
 */
static void define_key_from_keymap(const unsigned char *map,
				   unsigned long mask, int flag)
{
  unsigned char buf[3], ch;
  int i;
  static unsigned char mapped_already[256];

  for (i = 0; i < 97; i++) {
    if (((ch = map[i]) == 0) || (ch == 27)
	|| (ch == config.term_esc_char))
      continue;

    if (!mapped_already[ch]) {
      buf[0] = ch;
      buf[1] = 0;
      define_key(buf, mask | i, The_Normal_KeyMap);
    }
    mapped_already[ch] = 1;

    if (flag && (ch > 0x40) && (ch < 0x60) && (ch != '[')
	&& (0 == mapped_already[ch - 0x40])) {
      mapped_already[ch - 0x40] = 1;
      buf[0] = '^';
      buf[1] = ch;
      buf[2] = 0;
      define_key(buf, CTRL_MASK | i, The_Normal_KeyMap);
    }
  }
}

static int init_slang_keymaps(void)
{
  SLKeyMap_List_Type *m;
  unsigned char buf[5];
  unsigned long esc_scan;
  char * term;

  /* Do some sanity checking */
  if (config.term_esc_char >= 32)
    config.term_esc_char = 30;

  esc_scan = Ctrl_Char_Scan_Codes[config.term_esc_char];
  if (esc_scan == 0) {
    config.term_esc_char = 30;
    esc_scan = Ctrl_Char_Scan_Codes[30];
  }

  esc_scan |= CTRL_MASK;

  Esc_Char = config.term_esc_char + '@';

  if (The_Normal_KeyMap != NULL)
    return 0;

  if (NULL == (m = The_Normal_KeyMap = SLang_create_keymap("Normal", NULL)))
    return -1;

  /* Everybody needs these */
  define_keyset(Dosemu_defined_fkeys, m);
  define_keyset(Generic_backspace, m);

  /* Keypad in a special way */
  define_keyset(Dosemu_Xkeys, m);

  term = getenv("TERM");
  if( term && !strncmp("xterm", term, 5) ) {
    /* Oh no, this is _BAD_, there are so many different things called 'xterm'*/

    define_keyset(Meta_ALT, m);		/* For xterms */
    define_keyset(Esc_ALT, m);		/* For rxvt */
    define_keyset(vtxxx_fkeys, m);
    define_keyset(Xterm_Xkeys, m);
    define_keyset(Xterm_fkeys, m);
    /* The rxvt_alt_keys confilict with ESCESC == ESC in Dosemu_defined_fkeys */
    define_keyset(rxvt_alt_keys, m);	/* For rxvt */
  }
  else if( term && !strncmp("linux", term, 5) ) {
    /* This isn't too nasty */

    define_keyset(Esc_ALT, m);
    define_keyset(vtxxx_fkeys, m);
    define_keyset(vtxxx_Keypad, m);
    define_keyset(Linux_Keypad, m);
    define_keyset(Linux_fkeys, m);
    define_keyset(Linux_Xkeys, m);

    /* Linux using Meta ALT is _very_ rare, it can only be
     * changed through an ioctl, which nobody uses (I hope!)
     */
  }
  else if( term && strcmp("vt52", term) && 
	   !strncmp("vt", term, 2) && term[2] >= '1' && term[2] <= '9' ) {
    /* A 'real' VT ... yesss, if you're sure ... */

    define_keyset(vtxxx_fkeys, m);
    define_keyset(vtxxx_xkeys, m);
    define_keyset(vtxxx_pfkey, m);
    define_keyset(vtxxx_Keypad, m);
  }
  else {
    /* Ok, the terminfo Must be correct here, but add
       something to allow for keypad stuff. */

#if 0  /* S-lang codes appears to send ke/ks (Keypad enable/disable) --Eric,  
	* It might not if you are on the console _and_ you use the
	* slang keyboard but use a linux console for display, possible?
	*/
    /* NEED This for screen 'cause S-lang doesn't send ke/ks */
    define_keyset(vtxxx_xkeys, m);
#endif

#if SLANG_VERSION <= 9934
    /* Then we haven't got terminfo keys - lets get guessing */

    define_keyset(vtxxx_xkeys, m);
    define_keyset(vtxxx_fkeys, m);
    define_keyset(vtxxx_pfkey, m);
    define_keyset(vtxxx_Keypad, m);
    define_keyset(Xterm_Xkeys, m);
    define_keyset(Xterm_fkeys, m);
    define_keyset(Linux_Keypad, m);
    define_keyset(Linux_fkeys, m);
    define_keyset(Linux_Xkeys, m);
#endif
  }

  /* Just on the offchance they've done something right! */
  define_keyset(terminfo_keys, m);

  define_key_from_keymap(config.keytable->key_map,   0, 1);
  define_key_from_keymap(config.keytable->shift_map, SHIFT_MASK, 1);
  define_key_from_keymap(config.keytable->alt_map,   ALT_MASK, 0);
  if (config.altkeytable) {
    define_key_from_keymap(config.altkeytable->key_map,   0, 1);
    define_key_from_keymap(config.altkeytable->shift_map, SHIFT_MASK, 1);
    define_key_from_keymap(config.altkeytable->alt_map,   ALT_MASK, 0);
  }

  /* And more Dosemu keys */
  define_keyset(Dosemu_Ctrl_keys, m);

  if (SLang_Error)
    return -1;

  /*
   * If the erase key (as set by stty) is a reasonably safe one, use it.
   */
  if( ((erasekey>0 && erasekey<' ')) && erasekey != 27 && erasekey != Esc_Char)
  {
     buf[0] = '^';
     buf[1] = erasekey+'@';
     buf[2] = 0;
     define_key(buf, KEY_BKSP, m);
  } else if (erasekey > '~' && erasekey <= 0xFF) {
     buf[0] = erasekey &0xff;
     buf[1] = 0;
     define_key(buf, KEY_BKSP, m);
  }

  /*
   * Now add one more for the esc character so that sending it twice sends
   * it.
   */
  buf[0] = '^';
  buf[1] = Esc_Char;
  buf[2] = '^';
  buf[3] = Esc_Char;
  buf[4] = 0;
  SLang_define_key1(buf, (VOID *) esc_scan, SLKEY_F_INTRINSIC, The_Normal_KeyMap);
  if (SLang_Error)
    return -1;

  return 0;
}

/*
 * Global variables this module uses: int kbcount : number of characters
 * in the keyboard buffer to be processed. unsigned char kbbuf[KBBUF_SIZE]
 * : buffer where characters are waiting to be processed unsigned char
 * *kbp : pointer into kbbuf where next char to be processed is.
 */

static int read_some_keys(void)
{
  int cc;

  if (kbcount == 0)
    kbp = kbbuf;
  else if (kbp > &kbbuf[(KBBUF_SIZE * 3) / 5]) {
    memmove(kbbuf, kbp, kbcount);
    kbp = kbbuf;
  }
  cc = read(kbd_fd, &kbp[kbcount], KBBUF_SIZE - kbcount - 1);
  k_printf("KBD: cc found %d characters (Xlate)\n", cc);
  if (cc > 0)
    kbcount += cc;
  return cc;
}

/*
 * This function is a callback to read the key.  It returns 0 if one is
 * not ready.
 */

static int KeyNot_Ready;	/* a flag */
static int Keystr_Len;

static int getkey_callback(void)
{
  if (kbcount == Keystr_Len)
    read_some_keys();
  if (kbcount == Keystr_Len) {
    KeyNot_Ready = 1;
    return 0;
  }
  return (int)*(kbp + Keystr_Len++);
}

/* DANG_BEGIN_COMMENT
 * sltermio_input_pending is called when a key is pressed and the time
 * till next keypress is important in interpreting the meaning of the
 * keystroke.  -- i.e. ESC
 * DANG_END_COMMENT
 */
static int sltermio_input_pending(void)
{
  struct timeval scr_tv;
  hitimer_t t_start, t_dif;
  fd_set fds;

#if 0
#define	THE_TIMEOUT 750000L
#else
#define THE_TIMEOUT 250000L
#endif
  FD_ZERO(&fds);
  FD_SET(kbd_fd, &fds);
  scr_tv.tv_sec = 0L;
  scr_tv.tv_usec = THE_TIMEOUT;

  t_start = GETusTIME(0);
  errno = 0;
  while ((int)select(kbd_fd + 1, &fds, NULL, NULL, &scr_tv) < (int)1) {
    t_dif = GETusTIME(0) - t_start;

    if ((t_dif >= THE_TIMEOUT) || (errno != EINTR))
      return 0;
    errno = 0;
    scr_tv.tv_sec = 0L;
    scr_tv.tv_usec = THE_TIMEOUT - (long)t_dif;
  }
  return 1;
}


/*
 * If the sticky bits are set, then the scan code or the modifier key has
 * already been taken care of.  In this case, the unsticky bit should be
 * ignored.
 */
static unsigned long Shift_Flags;
static void slang_send_scancode(unsigned long lscan, unsigned char ch)
{
  unsigned long flags = 0;
  unsigned long ls_flags = 0;

  k_printf("KBD: slang_send_scancode(lscan=%08x, ch='%s')\n", (unsigned int)lscan, chrprintable(ch));

  ls_flags = (lscan & ~0xFFFF);

  if (lscan & KEYPAD_MASK) {
    flags |= KEYPAD_MASK;
    switch(lscan & 0xFFFF)
    {
    case KEY_INS:	lscan = ls_flags + KEY_PAD_0; break;
    case KEY_END:	lscan = ls_flags + KEY_PAD_1; break;
    case KEY_DOWN:	lscan = ls_flags + KEY_PAD_2; break;
    case KEY_PGDN:	lscan = ls_flags + KEY_PAD_3; break;
    case KEY_LEFT:	lscan = ls_flags + KEY_PAD_4; break;

    case KEY_RIGHT:	lscan = ls_flags + KEY_PAD_6; break;
    case KEY_HOME:	lscan = ls_flags + KEY_PAD_7; break;
    case KEY_UP:	lscan = ls_flags + KEY_PAD_8; break;
    case KEY_PGUP:	lscan = ls_flags + KEY_PAD_9; break;
    case KEY_DEL:	lscan = ls_flags + KEY_PAD_DECIMAL; break;

    case KEY_DASH:	lscan = ls_flags + KEY_PAD_MINUS; break;
    case KEY_RETURN:	lscan = ls_flags + KEY_PAD_ENTER; break;

    case KEY_0:		lscan = ls_flags + KEY_PAD_0; break;
    case KEY_1:		lscan = ls_flags + KEY_PAD_1; break;
    case KEY_2:		lscan = ls_flags + KEY_PAD_2; break;
    case KEY_3:		lscan = ls_flags + KEY_PAD_3; break;
    case KEY_4:		lscan = ls_flags + KEY_PAD_4; break;
    case KEY_5:		lscan = ls_flags + KEY_PAD_5; break;
    case KEY_6:		lscan = ls_flags + KEY_PAD_6; break;
    case KEY_7:		lscan = ls_flags + KEY_PAD_7; break;
    case KEY_9:		lscan = ls_flags + KEY_PAD_9; break;

    /* This is a special */
    case KEY_8:		if ( lscan & SHIFT_MASK )
                          lscan = (lscan& ~(SHIFT_MASK|0xFFFF)) + KEY_PAD_AST;
			else
			  lscan = ls_flags + KEY_PAD_8;
                        break;

    /* Need to remove the shift flag for this */
    case KEY_EQUALS:	if (lscan & SHIFT_MASK ) {
                          lscan = (lscan & ~(SHIFT_MASK|0xFFFF)) + KEY_PAD_PLUS;
                        } /* else It is silly to translate an equals */
                        break;

    /* This still generates the wrong scancode - should be $E02F */
    case KEY_SLASH:	lscan = ls_flags + KEY_PAD_SLASH; break;
    }
  }
  else if( (lscan & (ALT_MASK|STICKY_ALT_MASK|ALTGR_MASK|STICKY_ALTGR_MASK))
        && (lscan & 0xFFFF) == KEY_PRTSCR)
    lscan = ls_flags + KEY_SYSRQ;
   
  if ((lscan & SHIFT_MASK)
      && ((lscan & STICKY_SHIFT_MASK) == 0)) {
    flags |= SHIFT_MASK;
    presskey(KEY_L_SHIFT,0);
  }

  if ((lscan & CTRL_MASK)
      && ((lscan & STICKY_CTRL_MASK) == 0)) {
    flags |= CTRL_MASK;
    presskey(KEY_L_CTRL,0);
  }

  if ((lscan & ALT_MASK)
      && ((lscan & STICKY_ALT_MASK) == 0)) {
    flags |= ALT_MASK;
    presskey(KEY_L_ALT,0);
  }

  if ((lscan & ALTGR_MASK)
      && ((lscan & STICKY_ALTGR_MASK) == 0)) {
    flags |= ALTGR_MASK;
    presskey(KEY_R_ALT,0);
  }

  /* create a character if we don't have one */
  if (!ch) {
	  switch(lscan) {
	  case KEY_PAD_ENTER:	ch= 0x0d; break;
	  case KEY_PAD_PLUS:	ch= '+'; break;
	  case KEY_PAD_MINUS:	ch= '-'; break;
	  case KEY_PAD_AST:	ch= '*'; break;
	  case KEY_PAD_DECIMAL:	ch= '.'; break;
	  case KEY_PAD_SLASH:	ch= '/'; break;
	  case KEY_PAD_0:	ch= '0'; break;
	  case KEY_PAD_1:	ch= '1'; break;
	  case KEY_PAD_2:	ch= '2'; break;
	  case KEY_PAD_3:	ch= '3'; break;
	  case KEY_PAD_4:	ch= '4'; break;
	  case KEY_PAD_5:	ch= '5'; break;
	  case KEY_PAD_6:	ch= '6'; break;
	  case KEY_PAD_7:	ch= '7'; break;
	  case KEY_PAD_8:	ch= '8'; break;
	  case KEY_PAD_9:	ch= '9'; break;
	  }
  }

  /* Sanity adjustments on ch */

  if (lscan & (STICKY_SHIFT_MASK | SHIFT_MASK)) {
    if ((ch >= 0x61) && (ch <= 0x7a)) {
      ch -= 0x20;
    }
  }

  if (lscan & (STICKY_CTRL_MASK | CTRL_MASK)) {
    /* actually this is probably a little extra */
    /* taken form serv_xlate.c ... translate .. (shiftstate & CTRL) */
    if ((ch >= 0x40) && (ch <= 0x7e)) {
      ch &= 0x1f;
    } 
    else if (ch == 0x2D) {
      ch = 0x1f;
    }
  }

  if (lscan & (STICKY_ALT_MASK | ALT_MASK | ALTGR_MASK | STICKY_ALTGR_MASK)) {
    ch = 0;
  }

  if (ch>=0xa0) {
    /* Does latin need translate too? */
    switch (config.term_charset) {
    case CHARSET_KOI8:   ch=koi8_to_dos[ch-0xa0]; break;
    }
  }

  presskey(lscan & 0x0000ffff, ch); 
  releasekey(lscan & 0x0000ffff); 

  if (flags & SHIFT_MASK) {
    releasekey(KEY_L_SHIFT);
    Shift_Flags &= ~SHIFT_MASK;
  }
  if (flags & CTRL_MASK) {
    releasekey(KEY_L_CTRL);
    Shift_Flags &= ~CTRL_MASK;
  }
  if (flags & ALT_MASK) {
    releasekey(KEY_L_ALT);
    Shift_Flags &= ~ALT_MASK;
  }
  if (flags & ALTGR_MASK) {
    releasekey(KEY_R_ALT);
    Shift_Flags &= ~ALTGR_MASK;
  }
  if (flags & KEYPAD_MASK) {
    Shift_Flags &= ~KEYPAD_MASK;
  }
}

void do_slang_getkeys(void)
{
  SLang_Key_Type *key;
  unsigned long scan;

  k_printf("KBD: do_slang_getkeys()\n");
  if (-1 == read_some_keys())
    return;

  k_printf("KBD: do_slang_getkeys() found %d bytes\n",kbcount);
   
  /* Now process the keys that are buffered up */
  while (kbcount) {
    Keystr_Len = 0;
    KeyNot_Ready = 0;
    key = SLang_do_key(The_Normal_KeyMap, getkey_callback);
    SLang_Error = 0;

    if (KeyNot_Ready) {
      if ((Keystr_Len == 1) && (*kbp == 27)) {
	/*
	 * We have an esc character.  If nothing else is available to be
	 * read after a brief period, assume user really wants esc.
	 */
        k_printf("KBD: got ESC character\n");
	if (sltermio_input_pending())
	  return;

        k_printf("KBD: slang got single ESC\n");
	slang_send_scancode(KEY_ESC | Shift_Flags, 27);
	key = NULL;
	/* drop on through to the return for the undefined key below. */
      }
      else
	break;			/* try again next time */
    }

    kbcount -= Keystr_Len;	/* update count */
    kbp += Keystr_Len;

    if (key == NULL) {
      /* undefined key --- return */
      DOSemu_Slang_Show_Help = 0;
      kbcount = 0;
      break;
    }

    if (DOSemu_Slang_Show_Help) {
      DOSemu_Slang_Show_Help = 0;
      continue;
    }

#if SLANG_VERSION < 9930
        scan = (unsigned long) key->f;
#else
	scan = (unsigned long) key->f.f;
#endif

    k_printf("KBD: scan=%08lx Shift_Flags=%08lx str[0]=%d str='%s' len=%d\n",
             scan,Shift_Flags,key->str[0],strprintable(key->str+1),Keystr_Len);
    if (!(scan&0x80000000)) {
      slang_send_scancode(scan | Shift_Flags, (Keystr_Len==1)?key->str[1]:0);
      continue;
    }

    switch (scan) {
      case CTRL_KEY_SCAN_CODE:
	if ( !(Shift_Flags & STICKY_CTRL_MASK))
	   Shift_Flags |= CTRL_MASK;
	break;

      case STICKY_CTRL_KEY_SCAN_CODE:
	if (Shift_Flags & CTRL_MASK)
	   Shift_Flags &= ~CTRL_MASK;
	if (Shift_Flags & STICKY_CTRL_MASK) {
          releasekey(KEY_L_CTRL);
	  Shift_Flags &= ~STICKY_CTRL_MASK;
	}
        else {
	  Shift_Flags |= STICKY_CTRL_MASK;
          presskey(KEY_L_CTRL,0);
	}
	break;

      case SHIFT_KEY_SCAN_CODE:
	if ( !(Shift_Flags & STICKY_SHIFT_MASK))
	   Shift_Flags |= SHIFT_MASK;
	break;

      case STICKY_SHIFT_KEY_SCAN_CODE:
	if (Shift_Flags & SHIFT_MASK)
	   Shift_Flags &= ~SHIFT_MASK;
	if (Shift_Flags & STICKY_SHIFT_MASK) {
          releasekey(KEY_L_SHIFT);
	  Shift_Flags &= ~STICKY_SHIFT_MASK;
	}
        else {
	  Shift_Flags |= STICKY_SHIFT_MASK;
          presskey(KEY_L_SHIFT,0);
	}
	break;

      case ALT_KEY_SCAN_CODE:
	if ( !(Shift_Flags & STICKY_ALT_MASK))
	   Shift_Flags |= ALT_MASK;
	break;

      case STICKY_ALT_KEY_SCAN_CODE:
	if (Shift_Flags & ALT_MASK)
	   Shift_Flags &= ~ALT_MASK;
	if (Shift_Flags & STICKY_ALT_MASK) {
          releasekey(KEY_L_ALT);
	  Shift_Flags &= ~STICKY_ALT_MASK;
	}
        else {
	  Shift_Flags |= STICKY_ALT_MASK;
          presskey(KEY_L_ALT,0);
	}
	break;

      case ALTGR_KEY_SCAN_CODE:
	if ( !(Shift_Flags & STICKY_ALTGR_MASK))
	   Shift_Flags |= ALTGR_MASK;
	break;

      case STICKY_ALTGR_KEY_SCAN_CODE:
	if (Shift_Flags & ALTGR_MASK)
	   Shift_Flags &= ~ALTGR_MASK;
	if (Shift_Flags & STICKY_ALTGR_MASK) {
          releasekey(KEY_R_ALT);
	  Shift_Flags &= ~STICKY_ALTGR_MASK;
	}
        else {
	  Shift_Flags |= STICKY_ALTGR_MASK;
          presskey(KEY_R_ALT,0);
	}
	break;

      case KEYPAD_KEY_SCAN_CODE:
	Shift_Flags |= KEYPAD_MASK;
	break;

      case SCROLL_DOWN_SCAN_CODE:
	DOSemu_Terminal_Scroll = 1;
	break;

      case SCROLL_UP_SCAN_CODE:
	DOSemu_Terminal_Scroll = -1;
	break;

      case REDRAW_SCAN_CODE:
	dos_slang_redraw();
	break;

      case SUSPEND_SCAN_CODE:
	dos_slang_suspend();
	break;

      case HELP_SCAN_CODE:
	DOSemu_Slang_Show_Help = 1;
	break;

      case RESET_SCAN_CODE:
	DOSemu_Slang_Show_Help = 0;
	DOSemu_Terminal_Scroll = 0;

	if (Shift_Flags & STICKY_CTRL_MASK) {
           releasekey(KEY_L_CTRL);
	}
	if (Shift_Flags & STICKY_SHIFT_MASK) {
           releasekey(KEY_L_SHIFT);
	}
	if (Shift_Flags & STICKY_ALT_MASK) {
           releasekey(KEY_L_ALT);
	}
	if (Shift_Flags & STICKY_ALTGR_MASK) {
           releasekey(KEY_R_ALT);
	}

	Shift_Flags = 0;

	break;

      case SET_MONO_SCAN_CODE:
	dos_slang_smart_set_mono();
	break;
    }
  }

  {
static char * keymap_prompts[] = {
  0,
  "[Shift]",
  "[Ctrl]",
  "[Ctrl-Shift]",
  "[Alt]",
  "[Alt-Shift]",
  "[Alt-Ctrl]",
  "[Alt-Ctrl-Shift]",
  "[AltGr]",
  "[AltGr-Shift]",
  "[AltGr-Ctrl]",
  "[AltGr-Ctrl-Shift]",
  "[AltGr-Alt]",
  "[AltGr-Alt-Shift]",
  "[AltGr-Alt-Ctrl]",
  "[AltGr-Alt-Ctrl-Shift]"
};
    int prompt_no = 0;

    if (Shift_Flags & (SHIFT_MASK | STICKY_SHIFT_MASK))	prompt_no += 1;
    if (Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK)) 	prompt_no += 2;
    if (Shift_Flags & (ALT_MASK | STICKY_ALT_MASK)) 	prompt_no += 4;
    if (Shift_Flags & (ALTGR_MASK | STICKY_ALTGR_MASK))	prompt_no += 8;

    DOSemu_Keyboard_Keymap_Prompt = keymap_prompts[prompt_no];
  }

#if 0
  if (Shift_Flags & (ALT_MASK | STICKY_ALT_MASK)) {
    if (Shift_Flags & (SHIFT_MASK | STICKY_SHIFT_MASK)) {
      if (Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK)) {
	DOSemu_Keyboard_Keymap_Prompt = "[Alt-Ctrl-Shift]";
      }
      else
	DOSemu_Keyboard_Keymap_Prompt = "[Alt-Shift]";
    }
    else if (Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK))
      DOSemu_Keyboard_Keymap_Prompt = "[Alt-Ctrl]";
    else
      DOSemu_Keyboard_Keymap_Prompt = "[Alt]";
  }
  else if (Shift_Flags & (SHIFT_MASK | STICKY_SHIFT_MASK)) {
    if (Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK)) {
      DOSemu_Keyboard_Keymap_Prompt = "[Ctrl-Shift]";
    }
    else
      DOSemu_Keyboard_Keymap_Prompt = "[Shift]";
  }
  else if (Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK))
    DOSemu_Keyboard_Keymap_Prompt = "[Ctrl]";
  else
    DOSemu_Keyboard_Keymap_Prompt = NULL;
#endif
}

#if SLANG_VERSION < 10000
static void sl_exit_error (char *err)
{
   error ("%s\n", err);
   leavedos (32);
}
#else
#include <stdarg.h>
static void sl_exit_error (char *fmt, va_list args)
{
   verror (fmt, args);
   leavedos (32);
}
#endif

static void sl_print_error(char *str)
{
   k_printf("%s\n", str);
}

/*
 * DANG_BEGIN_FUNCTION slang_keyb_init()
 * 
 * Code is called at start up to set up the terminal line for non-raw mode.
 * 
 * DANG_END_FUNCTION
 */
   
int slang_keyb_init(void) {
  struct termios buf;

  k_printf("KBD: slang_keyb_init()\n");
   
  /* XXX - x2dos doesn't make sense with SLANG, does it? */
  kbd_fd = STDIN_FILENO;
  save_kbd_flags = fcntl(kbd_fd, F_GETFL);
  fcntl(kbd_fd, F_SETFL, O_RDONLY | O_NONBLOCK);
   
  if (tcgetattr(kbd_fd, &save_termios) < 0) {
    int ignore = 0;
    if (errno == EINVAL || errno == ENOTTY) {
      if ( (config.cardtype == CARD_NONE)
          || !strcmp(getenv("TERM"),"dumb")        /* most cron's have this */
          || !strcmp(getenv("TERM"),"none")        /* ... some have this */
          || !strcmp(getenv("TERM"),"dosemu-none") /* ... when called recursivly */
                                                ) {
        /*
         * We assume we are running without a terminal (e.g. in a cronjob).
         * Hence, we setup the TERM variable to "dosemu-none",
         * set a suitable TERMCAP entry ... and ignore the rest
         */
        setenv("TERM", "dosemu-none", 1);
        setenv("TERMCAP",
          "dosemu-none|for running DOSEMU without real terminal:"
          ":am::co#80:it#8:li#25:"
          ":ce=\\E[K:cl=\\E[H\\E[2J:cm=\\E[%i%d;%dH:do=\\E[B:ho=\\E[H:"
          ":le=\\E[D:nd=\\E[C:ta=^I:up=\\E[A:",
          1
        );
        ignore = 1;
      }
    }
    if (!ignore) {
      error("slang_keyb_init(): Couldn't tcgetattr(kbd_fd,...) errno=%d\n", errno);
      leavedos(66);
    }
  }

  buf = save_termios;
  buf.c_iflag &= (ISTRIP | IGNBRK | IXON | IXOFF);
/* buf.c_oflag &= ~OPOST; 
  buf.c_cflag &= ~(HUPCL); */
  buf.c_cflag &= ~(CLOCAL | CSIZE | PARENB);  
  buf.c_cflag |= CS8;
  buf.c_lflag &= 0;	/* ISIG */
  buf.c_cc[VMIN] = 1;
  buf.c_cc[VTIME] = 0;
  erasekey = buf.c_cc[VERASE];
#if 0
  cfgetispeed(&buf);
  cfgetospeed(&buf);
#endif
  if (tcsetattr(kbd_fd, TCSANOW, &buf) < 0 
         && errno != EINVAL && errno != ENOTTY) {
    error("slang_keyb_init(): Couldn't tcsetattr(kbd_fd,TCSANOW,...) !\n");
  }

   /* Error:
    *        This function will try to be called again in env/video/terminal.c
    *        but we are called first and need this to allow terminfo defined
    *        keys to be found.
    */
    SLang_Exit_Error_Hook = sl_exit_error;
    if( !DOSemu_Slang_Got_Terminfo )
    {
       SLtt_get_terminfo ();
       DOSemu_Slang_Got_Terminfo = 1;
    }
#if SLANG_VERSION < 10000
  SLang_Error_Routine = sl_print_error;
#else
  SLang_Error_Hook = sl_print_error;
#endif

  if (-1 == init_slang_keymaps()) {
    error("Unable to initialize S-Lang keymaps.\n");
    leavedos(31);
  }

  if (!isatty(kbd_fd)) {
    k_printf("KBD: Using SIGIO\n");
    add_to_io_select(kbd_fd, 1, keyb_client_run);
  }
  else {
    k_printf("KBD: Not using SIGIO\n");
    add_to_io_select(kbd_fd, 0, keyb_client_run);
  }
   
  k_printf("KBD: slang_keyb_init() ok\n");
  return TRUE;
}

void slang_keyb_close(void)  {
   if (tcsetattr(kbd_fd, TCSAFLUSH, &save_termios) == -1
            && errno != EINVAL && errno != ENOTTY) {
      error("slang_keyb_close(): failed to restore keyboard termios settings!\n");
   }
   if (save_kbd_flags != -1) {
      fcntl(kbd_fd, F_SETFL, save_kbd_flags);
   }
}

struct keyboard_client Keyboard_slang =  {
   "slang",                    /* name */
   slang_keyb_init,            /* init */
   NULL,                       /* reset */
   slang_keyb_close,           /* close */
   do_slang_getkeys            /* run */
};
   
