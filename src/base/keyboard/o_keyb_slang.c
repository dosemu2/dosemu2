#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include "slang.h"
#include "emu.h"
#include "timers.h"
#include "keymaps.h"
#include "keyb_clients.h"
#include "keyboard.h"

#define KBBUF_SIZE 80

static int kbcount = 0;
static Bit8u kbbuf[KBBUF_SIZE];
static Bit8u *kbp;

static int save_kbd_flags = -1;	/* saved flags for STDIN before our fcntl */
static struct termios save_termios;
static Bit8u erasekey;

#ifndef SLANG_VERSION
# define SLANG_VERSION 1
#endif

/* unsigned char DOSemu_Slang_Escape_Character = 30; */

const char *DOSemu_Keyboard_Keymap_Prompt = NULL;
int DOSemu_Terminal_Scroll = 0;
int DOSemu_Slang_Show_Help = 0;

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
#define STICKY_SHIFT_MASK		0x00100000
#define STICKY_CTRL_MASK		0x00200000
#define STICKY_ALT_MASK			0x00400000

#define ALT_KEY_SCAN_CODE		0x80000000
#define STICKY_ALT_KEY_SCAN_CODE	0x80000001
#define SHIFT_KEY_SCAN_CODE		0x80000002
#define STICKY_SHIFT_KEY_SCAN_CODE	0x80000003
#define CTRL_KEY_SCAN_CODE		0x80000004
#define STICKY_CTRL_KEY_SCAN_CODE	0x80000005
#define SCROLL_UP_SCAN_CODE		0x80000020
#define SCROLL_DOWN_SCAN_CODE		0x80000021
#define REDRAW_SCAN_CODE		0x80000022
#define SUSPEND_SCAN_CODE		0x80000023
#define HELP_SCAN_CODE			0x80000024
#define RESET_SCAN_CODE			0x80000025
#define SET_MONO_SCAN_CODE		0x80000026


static Keymap_Scan_Type Normal_Map[] =
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
  {"^H",   KEY_BKSP },			/* Backspace */
  {"\377", KEY_BKSP },
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
/* Keypad keys */
  {"\033OM", KEY_PAD_ENTER },		/* Keypad Enter */
  {"\033OQ", KEY_PAD_SLASH },		/* Keypad / */
  {"\033OR", KEY_PAD_AST },		/* Keypad * */
#if 0  /* rxvt sends this for "End" (see below) */
  {"\033Ow", KEY_PAD_7 },		/* Keypad 7 */
#endif
  {"\033Ox", KEY_PAD_8 },		/* Keypad 8 */
  {"\033Oy", KEY_PAD_9 },		/* Keypad 9 */
  {"\033OS", KEY_PAD_MINUS },		/* Keypad - */
  {"\033Ot", KEY_PAD_4 },		/* Keypad 4 */
  {"\033Ou", KEY_PAD_5 },		/* Keypad 5 */
  {"\033Ov", KEY_PAD_6 },		/* Keypad 6 */
  {"\033Ol", KEY_PAD_PLUS },		/* Keypad + */
  {"\033Oq", KEY_PAD_1 },		/* Keypad 1 */
  {"\033Or", KEY_PAD_2 },		/* Keypad 2 */
  {"\033Os", KEY_PAD_3 },		/* Keypad 3 */
  {"\033Op", KEY_PAD_0 },		/* Keypad 0 */
  {"\033On", KEY_PAD_DECIMAL },		/* Keypad . */

/* Now here are the simulated keys using escape sequences */
  {"\033[2~",  KEY_INS },		/* Ins */
  {"\033[3~",  KEY_DEL },		/* Del    Another keyscan is 0x007F */
  {"\033[1~",  KEY_HOME },		/* Ho     Another keyscan is 0x5c00 */
  {"\033[7~",  KEY_HOME },		/* Ho     (xterm)  */
  {"\033[H",   KEY_HOME },		/* Ho */
  {"\033[4~",  KEY_END },		/* End    Another keyscan is 0x6100 */
  {"\033[8~",  KEY_END },		/* End    (xterm) */
  {"\033[K",   KEY_END },		/* End */
  {"\033Ow",   KEY_END },		/* End    (rxvt) */
  {"\033[5~",  KEY_PGUP },		/* PgUp */
  {"\033[6~",  KEY_PGDN },		/* PgDn */
  {"\033[A",   KEY_UP },		/* Up */
  {"\033OA",   KEY_UP },		/* Up */
  {"\033[B",   KEY_DOWN },		/* Dn */
  {"\033OB",   KEY_DOWN },		/* Dn */
  {"\033[C",   KEY_RIGHT },		/* Ri */
  {"\033OC",   KEY_RIGHT },		/* Ri */
  {"\033[D",   KEY_LEFT },		/* Le */
  {"\033OD",   KEY_LEFT },		/* Le */
  {"\033[[A",  KEY_F1 },		/* F1 */
  {"\033[[B",  KEY_F2 },		/* F2 */
  {"\033[[C",  KEY_F3 },		/* F3 */
  {"\033[[D",  KEY_F4 },		/* F4 */
  {"\033[[E",  KEY_F5 },		/* F5 */
  {"\033[11~", KEY_F1 },		/* F1 */
  {"\033[12~", KEY_F2 },		/* F2 */
  {"\033[13~", KEY_F3 },		/* F3 */
  {"\033[14~", KEY_F4 },		/* F4 */
  {"\033[15~", KEY_F5 },		/* F5 */
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

#if SLANG_VERSION > 9934
   {"^(k1)",	0x3b00},	       /* F1 */
   {"^(k2)",	0x3c00},	       /* F2 */
   {"^(k3)",	0x3d00},	       /* F3 */
   {"^(k4)",	0x3e00},	       /* F4 */
   {"^(k5)",	0x3f00},	       /* F5 */
   {"^(k6)",	0x4000},	       /* F6 */
   {"^(k7)",	0x4100},	       /* F7 */
   {"^(k8)",	0x4200},	       /* F8 */
   {"^(k9)",	0x4300},	       /* F9 */
   {"^(k;)",	0x4400},	       /* F10 */
   {"^(kI)",	0x52E0},	       /* Ins */
   {"^(kD)",	0x53E0},	       /* Del    Another keyscan is 0x007F */
   {"^(kh)",	0x47E0},	       /* Ho */
   {"^(@7)",	0x4fE0},	       /* End */
   {"^(kP)",	0x49E0},	       /* PgUp */
   {"^(kN)",	0x51E0},	       /* PgDn */
   {"^(ku)",	0x48E0},	       /* Up */
   {"^(kd)",	0x50E0},	       /* Dn */
   {"^(kr)",	0x4dE0},	       /* Ri */
   {"^(kl)",	0x4bE0},	       /* Le */
#endif
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

static int define_key(const unsigned char *key, unsigned long scan,
		      SLKeyMap_List_Type * m)
{
  unsigned char buf[16], k1;

#if 0
  k_printf("KBD: define '%s' %02x as %08x\n",key,key[0], (int)scan);
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
  SLang_define_key1((unsigned char *)key, (VOID *) scan, SLKEY_F_INTRINSIC, m);
  if (SLang_Error) {
    fprintf(stderr, "Bad key: %s\n", key);
    return -1;
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
  char *str;
  SLKeyMap_List_Type *m;
  Keymap_Scan_Type *k;
  unsigned char buf[5];
  unsigned long esc_scan;

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

  if (NULL == (The_Normal_KeyMap = SLang_create_keymap("Normal", NULL)))
    return -1;

  k = Normal_Map;
  m = The_Normal_KeyMap;

  while ((str = k->keystr), (*str != 0)) {
    define_key(str, k->scan_code, m);
    k++;
  }

  define_key_from_keymap(key_map_us,   0, 1);
  define_key_from_keymap(shift_map_us, SHIFT_MASK, 1);
  define_key_from_keymap(alt_map_us,   ALT_MASK, 0);

  /* Now setup the shift modifier keys */
  define_key("^@a", ALT_KEY_SCAN_CODE, m);
  define_key("^@c", CTRL_KEY_SCAN_CODE, m);
  define_key("^@s", SHIFT_KEY_SCAN_CODE, m);

  define_key("^@A", STICKY_ALT_KEY_SCAN_CODE, m);
  define_key("^@C", STICKY_CTRL_KEY_SCAN_CODE, m);
  define_key("^@S", STICKY_SHIFT_KEY_SCAN_CODE, m);

  define_key("^@?", HELP_SCAN_CODE, m);
  define_key("^@h", HELP_SCAN_CODE, m);

  define_key("^@^R", REDRAW_SCAN_CODE, m);
  define_key("^@^L", REDRAW_SCAN_CODE, m);
  define_key("^@^Z", SUSPEND_SCAN_CODE, m);
  define_key("^@ ", RESET_SCAN_CODE, m);
  define_key("^@B", SET_MONO_SCAN_CODE, m);

  define_key("^@\033[A", SCROLL_UP_SCAN_CODE, m);
  define_key("^@\033OA", SCROLL_UP_SCAN_CODE, m);
  define_key("^@U", SCROLL_UP_SCAN_CODE, m);

  define_key("^@\033[B", SCROLL_DOWN_SCAN_CODE, m);
  define_key("^@\033OB", SCROLL_DOWN_SCAN_CODE, m);
  define_key("^@D", SCROLL_DOWN_SCAN_CODE, m);

  if (SLang_Error)
    return -1;

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
  hitimer_t t_start;
  fd_set fds;
  long t_dif;

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
    t_dif = (long)(GETusTIME(0) - t_start);

    if ((t_dif >= THE_TIMEOUT) || (errno != EINTR))
      return 0;
    errno = 0;
    scr_tv.tv_sec = 0L;
    scr_tv.tv_usec = THE_TIMEOUT - t_dif;
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

  k_printf("KBD: slang_send_scancode(lscan=%08x, ch='%c')\n", (unsigned int)lscan, ch);
   
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

  if (lscan & (STICKY_ALT_MASK | ALT_MASK)) {
    ch = 0;
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
             scan,Shift_Flags,key->str[0],key->str+1,Keystr_Len);
    if (!(scan&0x80000000)) {
      slang_send_scancode(scan | Shift_Flags, (Keystr_Len==1)?key->str[1]:0);
      continue;
    }

    switch (scan) {
      case STICKY_ALT_KEY_SCAN_CODE:
      case ALT_KEY_SCAN_CODE:
	if (Shift_Flags & STICKY_ALT_MASK) {
          releasekey(KEY_L_ALT);
	  Shift_Flags &= ~STICKY_ALT_MASK;
	  break;
	}

	if (scan == STICKY_ALT_KEY_SCAN_CODE) {
	  Shift_Flags |= STICKY_ALT_MASK;
          presskey(KEY_L_ALT,0);
	}
	else
	  Shift_Flags |= ALT_MASK;
	break;

      case STICKY_SHIFT_KEY_SCAN_CODE:
      case SHIFT_KEY_SCAN_CODE:
	if (Shift_Flags & STICKY_SHIFT_MASK) {
          releasekey(KEY_L_SHIFT);
	  Shift_Flags &= ~STICKY_SHIFT_MASK;
	  break;
	}

	if (scan == STICKY_SHIFT_KEY_SCAN_CODE) {
	  Shift_Flags |= STICKY_SHIFT_MASK;
          presskey(KEY_L_SHIFT,0);
	}
	else
	  Shift_Flags |= SHIFT_MASK;
	break;

      case STICKY_CTRL_KEY_SCAN_CODE:
      case CTRL_KEY_SCAN_CODE:
	if (Shift_Flags & STICKY_CTRL_MASK) {
          releasekey(KEY_L_CTRL);
	  Shift_Flags &= ~STICKY_CTRL_MASK;
	  break;
	}

	if (scan == STICKY_CTRL_KEY_SCAN_CODE) {
	  Shift_Flags |= STICKY_CTRL_MASK;
          presskey(KEY_L_CTRL,0);
	}
	else
	  Shift_Flags |= CTRL_MASK;
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
           releasekey(KEY_L_SHIFT);
	}

	Shift_Flags = 0;

	break;

      case SET_MONO_SCAN_CODE:
	dos_slang_smart_set_mono();
	break;
    }
  }

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
    error("slang_keyb_init(): Couldn't tcgetattr(kbd_fd,...) !\n");
    leavedos(66);
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
  if (tcsetattr(kbd_fd, TCSANOW, &buf) < 0) {
    error("slang_keyb_init(): Couldn't tcsetattr(kbd_fd,TCSANOW,...) !\n");
  }
  if (-1 == init_slang_keymaps()) {
    error("Unable to initialize S-Lang keymaps.\n");
    leavedos(31);
  }

  if (use_sigio && !isatty(kbd_fd)) {
    k_printf("KBD: Using SIGIO\n");
    add_to_io_select(kbd_fd, 1);
  }
  else {
    k_printf("KBD: Not using SIGIO\n");
    add_to_io_select(kbd_fd, 0);
  }
   
  k_printf("KBD: slang_keyb_init() ok\n");
  return TRUE;
}

void slang_keyb_close(void)  {
   if (tcsetattr(kbd_fd, TCSAFLUSH, &save_termios) == -1) {
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
   
