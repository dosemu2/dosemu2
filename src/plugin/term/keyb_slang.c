/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <ctype.h>
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
#include "env_term.h"
#include "translate.h"

#if SLANG_VERSION < 20000
#define SLang_set_error(x) (SLang_Error = (x))
#define SLang_get_error() SLang_Error
#endif

#ifndef VOID
#  define VOID void
#endif

#define KBBUF_SIZE 80

static struct keyboard_state
{
	int kbd_fd;

	int kbcount;
	Bit8u kbbuf[KBBUF_SIZE];
	Bit8u *kbp;

	int save_kbd_flags; /* saved flags for STDIN before our fcntl */
	struct termios save_termios;

	int pc_scancode_mode; /* By default we are not in pc_scancode_mode */
	SLKeyMap_List_Type *The_Normal_KeyMap;

	unsigned char erasekey;
	unsigned char Esc_Char;
	int KeyNot_Ready;	 /* a flag */
	int Keystr_Len;
	unsigned long Shift_Flags;
	
	struct char_set_state translate_state;
} keyb_state;


#ifndef SLANG_VERSION
# define SLANG_VERSION 1
#endif

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
#define MOVE_MASK			0x02000000
#define WAIT_MASK			0x04000000

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
  {"", 0}
};

static Keymap_Scan_Type CTRL[] =
{
  {"^Q", KEY_Q | CTRL_MASK },
  {"^W", KEY_W | CTRL_MASK },
  {"^E", KEY_E | CTRL_MASK },
  {"^R", KEY_R | CTRL_MASK },
  {"^T", KEY_T | CTRL_MASK },
  {"^Y", KEY_Y | CTRL_MASK },
  {"^U", KEY_U | CTRL_MASK },
  {"^I", KEY_TAB },
  {"^O", KEY_O | CTRL_MASK },
  {"^P", KEY_P | CTRL_MASK },
  {"^A", KEY_A | CTRL_MASK },
  {"^S", KEY_S | CTRL_MASK },
  {"^D", KEY_D | CTRL_MASK },
  {"^F", KEY_F | CTRL_MASK },
  {"^G", KEY_G | CTRL_MASK },
  /* if backspace generates ^H then the terminfo definition
     takes precedence here */
  {"^H", KEY_H | CTRL_MASK },
  {"^J", KEY_J | CTRL_MASK },
  {"^K", KEY_K | CTRL_MASK },
  {"^L", KEY_L | CTRL_MASK },
  {"^Z", KEY_Z | CTRL_MASK },
  {"^X", KEY_X | CTRL_MASK },
  {"^C", KEY_BREAK },
  {"^V", KEY_V | CTRL_MASK },
  {"^B", KEY_B | CTRL_MASK },
  {"^N", KEY_N | CTRL_MASK },
  {"^M", KEY_RETURN },
  {"^\\",KEY_BACKSLASH | CTRL_MASK },
  {"^]", KEY_RBRACK | CTRL_MASK },
  {"^^", KEY_6 | CTRL_MASK },
  {"^_", KEY_DASH | CTRL_MASK | SHIFT_MASK },
  {"", 0}
};

#if 0 /* better not to use this (see comment below) */
static Keymap_Scan_Type Linux_Keypad[] =
{
/* Keypad keys */
  {"\033OP", KEY_NUM },			/* Keypad Numlock */
  {"\033OQ", KEY_PAD_SLASH },		/* Keypad / */
  {"\033OR", KEY_PAD_AST },		/* Keypad * */
  {"\033OS", KEY_PAD_MINUS },		/* Keypad - */
  {"", 0}
};
#endif

static Keymap_Scan_Type Xterm_Keypad[] =
{
/* Keypad keys */
  {"\033Oo", KEY_PAD_SLASH },		/* Keypad / */
  {"\033Oj", KEY_PAD_AST },		/* Keypad * */
  {"\033Om", KEY_PAD_MINUS },		/* Keypad - */
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

static Keymap_Scan_Type Xterm_Xkeys[] =
{
  {"\033[2~",  KEY_INS | MOVE_MASK },	/* Ins */
  {"\033[H",   KEY_HOME },		/* Ho     (rxvt)  */
  {"\033[e",   KEY_END },		/* End    (color_xterm) */
  {"\033[K",   KEY_END },		/* End  - Where does this come from ? */
  {"\033[F",   KEY_END },		/* End  - another xterm */
  {"\033[5~",  KEY_PGUP },		/* PgUp */
  {"\033[6~",  KEY_PGDN },		/* PgDn */

  {"\033[7~",  KEY_HOME },		/* Ho     (xterm) */
  {"\033[8~",  KEY_END },		/* End    (xterm) */
  {"\033[1~",  KEY_HOME },		/* Ho     (putty) */
  {"\033[4~",  KEY_END },		/* End    (putty) */

  {"\033[A",   KEY_UP },		/* Up */
  {"\033[B",   KEY_DOWN },		/* Dn */
  {"\033[C",   KEY_RIGHT },		/* Ri */
  {"\033[D",   KEY_LEFT },		/* Le */

  {"\033[G",   KEY_PAD_5 },             /* 5 (putty) */
  {"\033OA",   KEY_UP | CTRL_MASK },    /* Up (putty) */
  {"\033OB",   KEY_DOWN | CTRL_MASK },  /* Dn (putty) */
  {"\033OC",   KEY_RIGHT | CTRL_MASK }, /* Ri (putty) */
  {"\033OD",   KEY_LEFT | CTRL_MASK },  /* Le (putty) */
  {"\033OG",   KEY_PAD_5 | CTRL_MASK }, /* 5 (putty) */

  {"\033[Z",   KEY_TAB | SHIFT_MASK },  /* Shift-Tab */

  /* rxvt keys */
  {"\033[a",    KEY_UP | SHIFT_MASK },   /* Up */
  {"\033[b",    KEY_DOWN | SHIFT_MASK }, /* Dn */
  {"\033[c",    KEY_RIGHT | SHIFT_MASK },/* Ri */
  {"\033[d",    KEY_LEFT | SHIFT_MASK }, /* Le */
  {"\033Oa",    KEY_UP | CTRL_MASK },    /* Up */
  {"\033Ob",    KEY_DOWN | CTRL_MASK },  /* Dn */
  {"\033Oc",    KEY_RIGHT | CTRL_MASK }, /* Ri */
  {"\033Od",    KEY_LEFT | CTRL_MASK },  /* Le */

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

/* Keyboard map for terminfo keys */
static Keymap_Scan_Type terminfo_keys[] =
{
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
   {"^(kI)",	KEY_INS|MOVE_MASK},    /* Ins */
   {"^(#3)",	KEY_INS|MOVE_MASK|SHIFT_MASK},   /* Shift Insert */
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
   {"^(Km)",	KEY_MOUSE },		/* Mouse */

   /* Special keys */
   {"^(&2)",	REDRAW_SCAN_CODE},	/* Refresh */
   {"^(%1)",	HELP_SCAN_CODE},	/* Help */
   {"", 0}
};

/* Keyboard map for basic terminfo keys F11--F20 */
static Keymap_Scan_Type terminfo_basic_fkeys[] =
{
   {"^(F1)",	KEY_F1|SHIFT_MASK},    /* Shuft F1 */
   {"^(F2)",	KEY_F2|SHIFT_MASK},    /* Shift F2 */
   {"^(F3)",	KEY_F3|SHIFT_MASK},    /* Shift F3 */
   {"^(F4)",	KEY_F4|SHIFT_MASK},    /* Shift F4 */
   {"^(F5)",	KEY_F5|SHIFT_MASK},    /* Shift F5 */
   {"^(F6)",	KEY_F6|SHIFT_MASK},    /* Shift F6 */
   {"^(F7)",	KEY_F7|SHIFT_MASK},    /* Shift F7 */
   {"^(F8)",	KEY_F8|SHIFT_MASK},    /* Shift F8 */
   {"^(F9)",	KEY_F9|SHIFT_MASK},    /* Shift F9 */
   {"^(FA)",	KEY_F10|SHIFT_MASK},   /* Shift F10 */
   {"", 0}
};

/* Keyboard map for extended terminfo keys F11--F48 */
static Keymap_Scan_Type terminfo_ext_fkeys[] =
{
   {"^(F1)",	KEY_F11},	       /* F11 */
   {"^(F2)",	KEY_F12},	       /* F12 */
   {"^(F3)",	KEY_F1|SHIFT_MASK},    /* Shift F1 */
   {"^(F4)",	KEY_F2|SHIFT_MASK},    /* Shift F2 */
   {"^(F5)",	KEY_F3|SHIFT_MASK},    /* Shift F3 */
   {"^(F6)",	KEY_F4|SHIFT_MASK},    /* Shift F4 */
   {"^(F7)",	KEY_F5|SHIFT_MASK},    /* Shift F5 */
   {"^(F8)",	KEY_F6|SHIFT_MASK},    /* Shift F6 */
   {"^(F9)",	KEY_F7|SHIFT_MASK},    /* Shift F7 */
   {"^(FA)",	KEY_F8|SHIFT_MASK},    /* Shift F8 */
   {"^(FB)",	KEY_F9|SHIFT_MASK},    /* Shift F9 */
   {"^(FC)",	KEY_F10|SHIFT_MASK},   /* Shift F10 */
   {"^(FD)",	KEY_F11|SHIFT_MASK},   /* Shift F11 */
   {"^(FE)",	KEY_F12|SHIFT_MASK},   /* Shift F12 */
   {"^(FF)",	KEY_F1|CTRL_MASK},     /* Ctrl F1 */
   {"^(FG)",	KEY_F2|CTRL_MASK},     /* Ctrl F2 */
   {"^(FH)",	KEY_F3|CTRL_MASK},     /* Ctrl F3 */
   {"^(FI)",	KEY_F4|CTRL_MASK},     /* Ctrl F4 */
   {"^(FJ)",	KEY_F5|CTRL_MASK},     /* Ctrl F5 */
   {"^(FK)",	KEY_F6|CTRL_MASK},     /* Ctrl F6 */
   {"^(FL)",	KEY_F7|CTRL_MASK},     /* Ctrl F7 */
   {"^(FM)",	KEY_F8|CTRL_MASK},     /* Ctrl F8 */
   {"^(FN)",	KEY_F9|CTRL_MASK},     /* Ctrl F9 */
   {"^(FO)",	KEY_F10|CTRL_MASK},    /* Ctrl F10 */
   {"^(FP)",	KEY_F11|CTRL_MASK},    /* Ctrl F11 */
   {"^(FQ)",	KEY_F12|CTRL_MASK},    /* Ctrl F12 */
   {"^(FR)",	KEY_F1|CTRL_MASK|SHIFT_MASK},     /* Ctrl Shift F1 */
   {"^(FS)",	KEY_F2|CTRL_MASK|SHIFT_MASK},     /* Ctrl Shift F2 */
   {"^(FT)",	KEY_F3|CTRL_MASK|SHIFT_MASK},     /* Ctrl Shift F3 */
   {"^(FU)",	KEY_F4|CTRL_MASK|SHIFT_MASK},     /* Ctrl Shift F4 */
   {"^(FV)",	KEY_F5|CTRL_MASK|SHIFT_MASK},     /* Ctrl Shift F5 */
   {"^(FW)",	KEY_F6|CTRL_MASK|SHIFT_MASK},     /* Ctrl Shift F6 */
   {"^(FX)",	KEY_F7|CTRL_MASK|SHIFT_MASK},     /* Ctrl Shift F7 */
   {"^(FY)",	KEY_F8|CTRL_MASK|SHIFT_MASK},     /* Ctrl Shift F8 */
   {"^(FZ)",	KEY_F9|CTRL_MASK|SHIFT_MASK},     /* Ctrl Shift F9 */
   {"^(Fa)",	KEY_F10|CTRL_MASK|SHIFT_MASK},    /* Ctrl Shift F10 */
   {"^(Fb)",	KEY_F11|CTRL_MASK|SHIFT_MASK},    /* Ctrl Shift F11 */
   {"^(Fc)",	KEY_F12|CTRL_MASK|SHIFT_MASK},    /* Ctrl Shift F12 */
   {"", 0}
};

static Keymap_Scan_Type Dosemu_Xkeys[] =
{
/* These keys are laid out like the numbers on the keypad - not too difficult */
  {"^@K0",  KEY_INS|MOVE_MASK}, /* Ins */
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


/* shift state from previous keypress */
static unsigned long old_flags = 0;

static const unsigned char *define_key_keys = 0;
static int define_key_keys_length =0;

static int define_getkey_callback(void)
{
	if (define_key_keys_length == 0) {
		define_key_keys = 0;
	}
	if (!define_key_keys) {
		return 0;
	}
	define_key_keys_length--;
	return *define_key_keys++;
}

/* Note: Later definitions with the same or a conflicting key sequence fail,
 *  and give an error message, but now don't stop the emulator.
 */
static int define_key(const unsigned char *key, unsigned long scan,
		      SLKeyMap_List_Type * m)
{
	unsigned char buf[SLANG_MAX_KEYMAP_KEY_SEQ +1], k1;
	unsigned char buf2[SLANG_MAX_KEYMAP_KEY_SEQ +1];
	int ret;
	const unsigned char *key_str;
	SLang_Key_Type *pre_key;
	int i;

	if (strlen(key) > SLANG_MAX_KEYMAP_KEY_SEQ) {
		k_printf("key string too long %s\n", key); 
		return -1;
	}

	if (SLang_get_error()) {
		k_printf("Current slang error skipping string %s\n", key);
		return -1;
	}

	if ((*key == '^') && (keyb_state.Esc_Char != '@')) {
		k1 = key[1];
		if (k1 == keyb_state.Esc_Char)
			return 0;		/* ^Esc_Char is not defined here */
		if (k1 == '@') {
			strcpy(buf, key);
			buf[1] = keyb_state.Esc_Char;
			key = buf;
		}
	}

	/* Get the translated keystring, and save a copy */
	key_str = SLang_process_keystring((char *)key);
	memcpy(buf2, key_str, key_str[0]);
	key_str = buf2;

	/* Display in the debug logs what we are defining */
	k_printf("KBD: define ");
	k_printf("'%s'=", strprintable((char *)key));
	for(i = 1; i < key_str[0]; i++) {
		if (i != 1) {
			k_printf(",");
		}
		k_printf("%02x", key_str[i]);
	}
	k_printf(" -> %04lX:%04lX\n", scan >> 16, scan & 0xFFFF);

	if (key_str[0] == 1) {
		k_printf("KBD: no input string skipping\n\n");
		return 0;
	}

	/* Lookup the key to see if we have already defined it */
	define_key_keys = key_str +1;
	define_key_keys_length = key_str[0] -1;
	pre_key = SLang_do_key(m, define_getkey_callback);

	/* Duplicate key definition, warn and ignore it */
	if (pre_key && (pre_key->str[0] == key_str[0]) &&
		(memcmp(pre_key->str, key_str, key_str[0]) == 0)) {
		unsigned long prev_scan;
		prev_scan = (unsigned long)pre_key->f.f;
		
		k_printf("KBD: Previously mapped to: %04lx:%04lx\n\n",
			prev_scan >> 16, prev_scan & 0xFFFF);
		return 0;
	}

	ret = SLkm_define_key((unsigned char *)key, (VOID *) scan, m);
	if (ret == -2) {  /* Conflicting key error, ignore it */
		k_printf("KBD: Conflicting key: \n\n");
		SLang_set_error(0);
	}
	if (SLang_get_error()) {
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

static void define_remaining_characters(SLKeyMap_List_Type *m)
{
	int i;

	for(i = ' '; i < 256; i++) {
		unsigned char str[2];
		if (keyb_state.Esc_Char != '@' &&
		    keyb_state.Esc_Char == '@' + i)
		  continue;
		str[0] = i;
		str[1] = '\0';

		if (define_key(str, KEY_VOID, m) < 0) {
			continue;
		}
	}
}

static int init_slang_keymaps(void)
{
	SLKeyMap_List_Type *m;
	unsigned char buf[5];
	unsigned long esc_scan;
	char * term;
	char * kf21;
	
	/* Do some sanity checking */
	if (config.term_esc_char >= 32)
		config.term_esc_char = 30;
	
	/* Carriage Return & Escape are not going to be used by any sane person */
	if ((config.term_esc_char == '\x0d') ||
		(config.term_esc_char == '\x1b'))
	{
		config.term_esc_char = 30;
	}
	/* escape characters are identity mapped in unicode. */
	esc_scan = config.term_esc_char;
	esc_scan |= CTRL_MASK;
	
	keyb_state.Esc_Char = config.term_esc_char + '@';
	
	if (keyb_state.The_Normal_KeyMap != NULL)
		return 0;
	
	if (NULL == (m = keyb_state.The_Normal_KeyMap = SLang_create_keymap("Normal", NULL)))
		return -1;
	
	/* Everybody needs these */
	define_keyset(Dosemu_defined_fkeys, m);
	
	/* Keypad in a special way */
	define_keyset(Dosemu_Xkeys, m);

	/* Some terminals have kf13=shift+f1, some others kf11=f11=shift+f1:
	   often the ones which stop with kf20 will have 2*10 function keys:
	   for those we assume f11=shift+f1.
	   The ones with more commonly have n*12 (xterm 4*12=48). That's
	   much better for dosemu as there is no confusing anymore
	   The Linux console is most problematic as the default mapping
	   may be different from what loadkeys does. KDGKBENT works only
	   locally and in that case raw keyboard is available anyway.
	*/
	kf21 = SLtt_tgetstr("FB");
	define_keyset(terminfo_keys, m);
	if (kf21) {
		k_printf("KBD: Many function keys detected in terminfo, trust it\n");
		define_keyset(terminfo_ext_fkeys, m);
	} else {
		define_keyset(terminfo_basic_fkeys, m);
	}

	define_keyset(CTRL, m);
	
	term = getenv("TERM");
	if( using_xterm() ) {
		/* Oh no, this is _BAD_, there are so many different things called 'xterm'*/
		if (!kf21) {
			define_keyset(vtxxx_fkeys, m);
			define_keyset(Xterm_fkeys, m);
		}
		define_keyset(vtxxx_Keypad, m);
		define_keyset(Xterm_Keypad, m);
		define_keyset(Xterm_Xkeys, m);
	}
#if 0
	/* The linux terminfo entry is missing smkx/rmkx;
	   manually setting keypad mode using an escape sequence
	   causes some confusion (NumLock LED does not work) */
	else if( term && !strncmp("linux", term, 5) ) {
		/* This isn't too nasty; most is in terminfo */
		define_keyset(vtxxx_Keypad, m);
		define_keyset(Linux_Keypad, m);
	}
#endif
	else if( term && strcmp("vt52", term) && 
		!strncmp("vt", term, 2) && term[2] >= '1' && term[2] <= '9' ) {
		/* A 'real' VT ... yesss, if you're sure ... */
		if (!kf21) define_keyset(vtxxx_fkeys, m);
		define_keyset(vtxxx_xkeys, m);
		define_keyset(vtxxx_Keypad, m);
	}

  /* And more Dosemu keys */
	define_keyset(Dosemu_Ctrl_keys, m);
	
	if (SLang_get_error())
		return -1;

	/*
	 * If the erase key (as set by stty) is a reasonably safe one, use it.
	 */
	if( ((keyb_state.erasekey>0 && keyb_state.erasekey<' ')) && 
		keyb_state.erasekey != 27 && keyb_state.erasekey != keyb_state.Esc_Char)
	{
		buf[0] = '^';
		buf[1] = keyb_state.erasekey+'@';
		buf[2] = 0;
		define_key(buf, KEY_BKSP, m);
	} else if (keyb_state.erasekey > '~') {
		buf[0] = keyb_state.erasekey;
		buf[1] = 0;
		define_key(buf, KEY_BKSP, m);
	}
	
	/*
	 * Now add one more for the esc character so that sending it twice sends
	 * it.
	 */
	buf[0] = '^';
	buf[1] = keyb_state.Esc_Char;
	buf[2] = '^';
	buf[3] = keyb_state.Esc_Char;
	buf[4] = 0;
	SLkm_define_key(buf, (VOID *) esc_scan, m);
	if (SLang_get_error())
		return -1;
	
	/* Note: define_keys_by_character comes last or we could never define functions keys. . . */
	define_remaining_characters(m);
	if (SLang_get_error())
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
	
	if (keyb_state.kbcount == 0)
		keyb_state.kbp = keyb_state.kbbuf;
	else if (keyb_state.kbp > &keyb_state.kbbuf[(KBBUF_SIZE * 3) / 5]) {
		memmove(keyb_state.kbbuf, keyb_state.kbp, keyb_state.kbcount);
		keyb_state.kbp = keyb_state.kbbuf;
	}
	cc = read(keyb_state.kbd_fd, &keyb_state.kbp[keyb_state.kbcount], KBBUF_SIZE - keyb_state.kbcount - 1);
	k_printf("KBD: cc found %d characters (Xlate)\n", cc);
	if (cc > 0)
		keyb_state.kbcount += cc;
	return cc;
}

/*
 * This function is a callback to read the key.  It returns 0 if one is
 * not ready.
 */


static int getkey_callback(void)
{
	if (keyb_state.kbcount == keyb_state.Keystr_Len)
		read_some_keys();
	if (keyb_state.kbcount == keyb_state.Keystr_Len) {
		keyb_state.KeyNot_Ready = 1;
		return 0;
	}
	return (int)*(keyb_state.kbp + keyb_state.Keystr_Len++);
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
	FD_SET(keyb_state.kbd_fd, &fds);
	scr_tv.tv_sec = 0L;
	scr_tv.tv_usec = THE_TIMEOUT;
	
	t_start = GETusTIME(0);
	errno = 0;
	while ((int)select(keyb_state.kbd_fd + 1, &fds, NULL, NULL, &scr_tv) < (int)1) {
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
static void slang_send_scancode(unsigned long ls_flags, unsigned long lscan)
{
	unsigned long flags = 0;
	
	k_printf("KBD: slang_send_scancode(ls_flags=%08lx, lscan=%08lx)\n", 
		ls_flags, lscan);

	if (lscan == KEY_MOUSE) {
		/* Xtermmouse support */
		xtermmouse_get_event(&keyb_state.kbp, &keyb_state.kbcount);
		return;
	}
	if (ls_flags & KEYPAD_MASK) {
		flags |= KEYPAD_MASK;
		switch(lscan)
		{
		case KEY_INS:    lscan = KEY_PAD_INS; break;
		case KEY_END:    lscan = KEY_PAD_END; break;
		case KEY_DOWN:   lscan = KEY_PAD_DOWN; break;
		case KEY_PGDN:   lscan = KEY_PAD_PGDN; break;
		case KEY_LEFT:   lscan = KEY_PAD_LEFT; break;
		case KEY_RIGHT:  lscan = KEY_PAD_RIGHT; break;
		case KEY_HOME:	 lscan = KEY_PAD_HOME; break;
		case KEY_UP:     lscan = KEY_PAD_UP; break;
		case KEY_PGUP:   lscan = KEY_PAD_PGUP; break;
		case KEY_DEL:    lscan = KEY_PAD_DEL; break;

		case KEY_DASH:   lscan = KEY_PAD_MINUS; break;
		case KEY_RETURN: lscan = KEY_PAD_ENTER; break;
			
		case KEY_0:      lscan = KEY_PAD_0; break;
		case KEY_1:	 lscan = KEY_PAD_1; break;
		case KEY_2:	 lscan = KEY_PAD_2; break;
		case KEY_3:      lscan = KEY_PAD_3; break;
		case KEY_4:      lscan = KEY_PAD_4; break;
		case KEY_5:      lscan = KEY_PAD_5; break;
		case KEY_6:      lscan = KEY_PAD_6; break;
		case KEY_7:      lscan = KEY_PAD_7; break;
		case KEY_9:      lscan = KEY_PAD_9; break;
			
		/* This is a special */
		case KEY_8:		
			if ( ls_flags & SHIFT_MASK ) {
				ls_flags &= ~SHIFT_MASK;
				lscan = KEY_PAD_AST;
			}
			else     lscan =  KEY_PAD_8; break;
		
		/* Need to remove the shift flag for this */
		case KEY_EQUALS:	
			if (ls_flags & SHIFT_MASK ) {
				ls_flags &= ~SHIFT_MASK;
				lscan = KEY_PAD_PLUS;
			} /* else It is silly to translate an equals */
			break;
		
		/* This still generates the wrong scancode - should be $E02F */
		case KEY_SLASH:	 lscan = KEY_PAD_SLASH; break;
		}

	}
	else if( (ls_flags & (ALT_MASK|STICKY_ALT_MASK|ALTGR_MASK|STICKY_ALTGR_MASK))
		&& (lscan == KEY_PRTSCR)) {
		lscan = KEY_SYSRQ;
		ls_flags |= MOVE_MASK;
	}
   
	if ((ls_flags & SHIFT_MASK)
		&& ((ls_flags & STICKY_SHIFT_MASK) == 0)) {
		flags |= SHIFT_MASK;
		move_key(PRESS, KEY_L_SHIFT);
	}
	
	if ((ls_flags & CTRL_MASK)
		&& ((ls_flags & STICKY_CTRL_MASK) == 0)) {
		flags |= CTRL_MASK;
		move_key(PRESS, KEY_L_CTRL);
	}
	
	if ((ls_flags & ALT_MASK)
		&& ((ls_flags & STICKY_ALT_MASK) == 0)) {
		flags |= ALT_MASK;
		move_key(PRESS, KEY_L_ALT);
	}
	
	if ((ls_flags & ALTGR_MASK)
		&& ((ls_flags & STICKY_ALTGR_MASK) == 0)) {
		flags |= ALTGR_MASK;
		move_key(PRESS, KEY_R_ALT);
	}
	
	if (!(ls_flags & MOVE_MASK)) {
		/* For any keys we know do not modify the shiftstate
		 * this is the optimal way to go.  As it handles all
		 * of the weird cases. 
		 */ 
		put_modified_symbol(PRESS, get_shiftstate(), lscan);
		put_modified_symbol(RELEASE, get_shiftstate(), lscan);
	} else {
		/* For the few keys that might modify the shiftstate
		 * we just do a straight forward key press and release.
		 */
		move_key(PRESS,   lscan);
		move_key(RELEASE, lscan);
	}

	/* set shift release in a "queue" to wait for two SIGALRMs */
	if (flags) flags |= WAIT_MASK;
	old_flags = flags;
}

void handle_slang_keys(Boolean make, t_keysym key)
{
	if (!make) {
		return;
	}
	switch(key) {
	case KEY_DOSEMU_HELP:
		DOSemu_Slang_Show_Help = 1;
		break;
	case KEY_DOSEMU_REDRAW:
		dos_slang_redraw();
		break;
	case KEY_DOSEMU_SUSPEND:
		dos_slang_suspend();
		break;
	case KEY_DOSEMU_MONO:
		dos_slang_smart_set_mono();
		break;
	case KEY_DOSEMU_PAN_UP:
		DOSemu_Terminal_Scroll = -1;
	case KEY_DOSEMU_PAN_DOWN:
		DOSemu_Terminal_Scroll = 1;
		break;
	case KEY_DOSEMU_PAN_LEFT: 
		/* this should be implemented someday */
		break;
	case KEY_DOSEMU_PAN_RIGHT:
		/* this should be implemented someday */
		break;
	case KEY_DOSEMU_RESET:
		DOSemu_Slang_Show_Help = 0;
		DOSemu_Terminal_Scroll = 0;
		if (keyb_state.Shift_Flags & STICKY_CTRL_MASK) {
			move_key(RELEASE, KEY_L_CTRL);
		}
		if (keyb_state.Shift_Flags & STICKY_SHIFT_MASK) {
			move_key(RELEASE, KEY_L_SHIFT);
		}
		if (keyb_state.Shift_Flags & STICKY_ALT_MASK) {
			move_key(RELEASE, KEY_L_ALT);
		}
		if (keyb_state.Shift_Flags & STICKY_ALTGR_MASK) {
			move_key(RELEASE, KEY_R_ALT);
		}
		
		keyb_state.Shift_Flags = 0;
	}
	return;
}

static void do_slang_special_keys(unsigned long scan)
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

	switch (scan) {
	case CTRL_KEY_SCAN_CODE:
		if ( !(keyb_state.Shift_Flags & STICKY_CTRL_MASK))
			keyb_state.Shift_Flags |= CTRL_MASK;
		break;
		
	case STICKY_CTRL_KEY_SCAN_CODE:
		if (keyb_state.Shift_Flags & CTRL_MASK)
			keyb_state.Shift_Flags &= ~CTRL_MASK;
		if (keyb_state.Shift_Flags & STICKY_CTRL_MASK) {
			move_key(RELEASE, KEY_L_CTRL);
			keyb_state.Shift_Flags &= ~STICKY_CTRL_MASK;
		}
		else {
			keyb_state.Shift_Flags |= STICKY_CTRL_MASK;
			move_key(PRESS, KEY_L_CTRL);
		}
		break;
		
	case SHIFT_KEY_SCAN_CODE:
		if ( !(keyb_state.Shift_Flags & STICKY_SHIFT_MASK))
			keyb_state.Shift_Flags |= SHIFT_MASK;
		break;
		
	case STICKY_SHIFT_KEY_SCAN_CODE:
		if (keyb_state.Shift_Flags & SHIFT_MASK)
			keyb_state.Shift_Flags &= ~SHIFT_MASK;
		if (keyb_state.Shift_Flags & STICKY_SHIFT_MASK) {
			move_key(RELEASE, KEY_L_SHIFT);
			keyb_state.Shift_Flags &= ~STICKY_SHIFT_MASK;
		}
		else {
			keyb_state.Shift_Flags |= STICKY_SHIFT_MASK;
			move_key(PRESS, KEY_L_SHIFT);
		}
		break;
		
	case ALT_KEY_SCAN_CODE:
		if ( !(keyb_state.Shift_Flags & STICKY_ALT_MASK))
			keyb_state.Shift_Flags |= ALT_MASK;
		break;
		
	case STICKY_ALT_KEY_SCAN_CODE:
		if (keyb_state.Shift_Flags & ALT_MASK)
			keyb_state.Shift_Flags &= ~ALT_MASK;
		if (keyb_state.Shift_Flags & STICKY_ALT_MASK) {
			move_key(RELEASE, KEY_L_ALT);
			keyb_state.Shift_Flags &= ~STICKY_ALT_MASK;
		}
		else {
			keyb_state.Shift_Flags |= STICKY_ALT_MASK;
			move_key(PRESS, KEY_L_ALT);
		}
		break;
		
	case ALTGR_KEY_SCAN_CODE:
		if ( !(keyb_state.Shift_Flags & STICKY_ALTGR_MASK))
			keyb_state.Shift_Flags |= ALTGR_MASK;
		break;
		
	case STICKY_ALTGR_KEY_SCAN_CODE:
		if (keyb_state.Shift_Flags & ALTGR_MASK)
			keyb_state.Shift_Flags &= ~ALTGR_MASK;
		if (keyb_state.Shift_Flags & STICKY_ALTGR_MASK) {
			move_key(RELEASE, KEY_R_ALT);
			keyb_state.Shift_Flags &= ~STICKY_ALTGR_MASK;
		}
		else {
			keyb_state.Shift_Flags |= STICKY_ALTGR_MASK;
			move_key(PRESS, KEY_R_ALT);
		}
		break;
		
	case KEYPAD_KEY_SCAN_CODE:
		keyb_state.Shift_Flags |= KEYPAD_MASK;
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
		
		if (keyb_state.Shift_Flags & STICKY_CTRL_MASK) {
			move_key(RELEASE, KEY_L_CTRL);
		}
		if (keyb_state.Shift_Flags & STICKY_SHIFT_MASK) {
			move_key(RELEASE, KEY_L_SHIFT);
		}
		if (keyb_state.Shift_Flags & STICKY_ALT_MASK) {
			move_key(RELEASE, KEY_L_ALT);
		}
		if (keyb_state.Shift_Flags & STICKY_ALTGR_MASK) {
			move_key(RELEASE, KEY_R_ALT);
		}
		
		keyb_state.Shift_Flags = 0;
		
		break;
		
	case SET_MONO_SCAN_CODE:
		dos_slang_smart_set_mono();
		break;
	}

	
	if (keyb_state.Shift_Flags & (SHIFT_MASK | STICKY_SHIFT_MASK))	prompt_no += 1;
	if (keyb_state.Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK)) 	prompt_no += 2;
	if (keyb_state.Shift_Flags & (ALT_MASK | STICKY_ALT_MASK)) 	prompt_no += 4;
	if (keyb_state.Shift_Flags & (ALTGR_MASK | STICKY_ALTGR_MASK))	prompt_no += 8;
	
	DOSemu_Keyboard_Keymap_Prompt = keymap_prompts[prompt_no];
}

/* checks for xterm and rxvt style modifiers in the escape sequence */
static int get_modifiers(void)
{
	int i, mod, replacepos, modifier;

	/* always starts with ^[[ or ^[O */
	if (keyb_state.kbcount < 4 ||
	    (keyb_state.kbp[1] != '[' && keyb_state.kbp[1] != 'O') ||
	    /* then either a digit or digits semicolon digit (or rxvt) */
	    !isdigit(keyb_state.kbp[2]))
		return 0;

	for (i = 3; isdigit(keyb_state.kbp[i]); i++)
		if (i >= keyb_state.kbcount - 1)
			return 0;

	mod = keyb_state.kbp[i];
	replacepos = i;
	if (mod == ';') {
		/* ^[[1;2A is special: get rid of "1;2" */
		if (i == 3 && keyb_state.kbp[2] == '1')
			replacepos--;
		/* e.g. ^[[15;2~ */
		i++;
		if (i >= keyb_state.kbcount - 1 || !isdigit(keyb_state.kbp[i]))
			return 0;
	} else if (mod == '$' || mod == '^' || mod == '@') {
		/*rxvt */
		keyb_state.kbp[i] = '~';
		if (mod == '$')
			return SHIFT_MASK;
		if (mod == '^')
			return CTRL_MASK;
		return CTRL_MASK | SHIFT_MASK; /* '@' */
	} else {
		/* e.g. ^[[2A */
		i--;
		if (i != 2)
			return 0;
		replacepos = i;
	}
	mod = (unsigned char)keyb_state.kbp[i] - '0';
	if(isdigit(keyb_state.kbp[i+1])) {
		i++;
		mod = mod * 10 + keyb_state.kbp[i] - '0';
	}
	mod--;
	/* after the digit must be a non-digit */
	if (mod == 0 || mod > 15 ||
	    i >= keyb_state.kbcount - 1 || isdigit(keyb_state.kbp[i+1]))
		return 0;
	modifier = 0;
	if (mod & 1)
		modifier |= SHIFT_MASK;
	if ((mod & 2) || (mod & 8))
		modifier |= ALT_MASK;
	if (mod & 4)
		modifier |= CTRL_MASK;
	/* now remove the modifier and figure out the unmodified string */
	memmove(&keyb_state.kbp[replacepos], &keyb_state.kbp[i+1],
		keyb_state.kbcount - (i + 1));
	keyb_state.kbcount -= (i - replacepos + 1);
	return modifier;
}

static void do_slang_getkeys(void)
{
	SLang_Key_Type *key;
	int cc;
	int modifier = 0;

	k_printf("KBD: do_slang_getkeys()\n");

	cc = read_some_keys();
	if (-1 == cc && (old_flags == 0 || (old_flags & WAIT_MASK))) {
		old_flags &= ~WAIT_MASK;
		return;
	}
	/* restore shift-state from previous keypress */
	if (old_flags & SHIFT_MASK) {
		move_key(RELEASE, KEY_L_SHIFT);
		keyb_state.Shift_Flags &= ~SHIFT_MASK;
	}
	if (old_flags & CTRL_MASK) {
		move_key(RELEASE, KEY_L_CTRL);
		keyb_state.Shift_Flags &= ~CTRL_MASK;
	}
	if (old_flags & ALT_MASK) {
		move_key(RELEASE, KEY_L_ALT);
		keyb_state.Shift_Flags &= ~ALT_MASK;
	}
	if (old_flags & ALTGR_MASK) {
		move_key(RELEASE, KEY_R_ALT);
		keyb_state.Shift_Flags &= ~ALTGR_MASK;
	}
	if (old_flags & KEYPAD_MASK) {
		keyb_state.Shift_Flags &= ~KEYPAD_MASK;
	}
	old_flags = 0;
	if (-1 == cc) {
		do_slang_special_keys(0);
		return;
	}
	
	k_printf("KBD: do_slang_getkeys() found %d bytes\n", keyb_state.kbcount);
	
	/* Now process the keys that are buffered up */
	while (keyb_state.kbcount) {
		unsigned long scan = 0;
		t_unicode symbol = KEY_VOID;
		size_t result;

		keyb_state.Keystr_Len = 0;
		keyb_state.KeyNot_Ready = 0;

		key = SLang_do_key(keyb_state.The_Normal_KeyMap, getkey_callback);
		SLang_set_error(0);
		
		if (keyb_state.KeyNot_Ready) {
			if ((keyb_state.Keystr_Len == 1) && (*keyb_state.kbp == 27)) {
				/*
				 * We have an esc character.  If nothing else is available to be
				 * read after a brief period, assume user really wants esc.
				 */
				k_printf("KBD: got ESC character\n");
				if (sltermio_input_pending())
					return;
				
				k_printf("KBD: slang got single ESC\n");
				symbol = KEY_ESC;
				key = NULL;
				/* drop on through to the return for the undefined key below. */
			}
			else
				break;			/* try again next time */
		} 
		
		if (key) {
			scan = (unsigned long) key->f.f | modifier;
			symbol = scan & 0xFFFF;
		} 
		result = 1;
		if (symbol == KEY_VOID) {
			/* rough draft version don't stop here... */
			result = charset_to_unicode(&keyb_state.translate_state,
				&symbol, keyb_state.kbp, keyb_state.kbcount);
			if (result != -1 && result > keyb_state.Keystr_Len)
				keyb_state.Keystr_Len = result;
			k_printf("KBD: got %08x, result=%zx\n", symbol, result);
		}

		if (key == NULL && symbol == KEY_ESC && keyb_state.Keystr_Len > 1) {
			int old_modifier = modifier;
			modifier = get_modifiers();
			if (modifier != old_modifier)
				continue;
			keyb_state.kbcount--;
			keyb_state.kbp++;
			modifier = ALT_MASK;
			continue;
		}
		modifier = 0;
		if (result == -1 && (unsigned char)keyb_state.kbp[0] >= 0x80) {
			/* allow for high bit meta on dumb ascii terminals */
			scan |= ALT_MASK;
			symbol = keyb_state.kbp[0] & 0x7f;
		}

		keyb_state.kbcount -= keyb_state.Keystr_Len;	/* update count */
		keyb_state.kbp += keyb_state.Keystr_Len;
		
               if (key == NULL && symbol != KEY_ESC) {
			/* undefined key --- return */
			DOSemu_Slang_Show_Help = 0;
			keyb_state.kbcount = 0;
			break;
		}
		
		if (DOSemu_Slang_Show_Help) {
			DOSemu_Slang_Show_Help = 0;
			continue;
		}
		
		
		k_printf("KBD: scan=%08lx Shift_Flags=%08lx str[0]=%d str='%s' len=%d\n",
                       scan,keyb_state.Shift_Flags,key ? key->str[0] : 27,
                       key ? strprintable(key->str+1): "ESC", keyb_state.Keystr_Len);
		if (!(scan&0x80000000)) {
			slang_send_scancode(keyb_state.Shift_Flags | scan, symbol);
			do_slang_special_keys(0);
		}
		else {
			do_slang_special_keys(scan);
		}
	}
}

/*
 * DANG_BEGIN_FUNCTION setup_pc_scancode_mode
 * 
 * Initialize the keyboard in pc scancode mode.
 * This functionality is ideal but rarely supported on a terminal.
 * 
 * DANG_END_FUNCTION
 */
static void setup_pc_scancode_mode(void)
{
	k_printf("entering pc scancode mode");
	set_shiftstate(0); /* disable all persistent shift state */

	/* enter pc scancode mode */
	SLtt_write_string(SLtt_tgetstr("S4"));
}

/*
 * DANG_BEGIN_FUNCTION exit_pc_scancode_mode
 * 
 * Set the terminal back to a keyboard mode other
 * programs can understand.
 * 
 * DANG_END_FUNCTION
 */
static void exit_pc_scancode_mode(void)
{
	if (keyb_state.pc_scancode_mode) {
		k_printf("leaving pc scancode mode");
		SLtt_write_string(SLtt_tgetstr("S5"));
		keyb_state.pc_scancode_mode = FALSE;
	}
}

/*
 * DANG_BEGIN_FUNCTION do_pc_scancode_getkeys
 * 
 * Set the terminal back to a keyboard mode other
 * programs can understand.
 * 
 * DANG_END_FUNCTION
 */
static void do_pc_scancode_getkeys(void)
{
	if (-1 == read_some_keys()) {
		return;
	}
	k_printf("KBD: do_pc_scancode_getkeys() found %d bytes\n", keyb_state.kbcount);
	
	/* Now process the keys that are buffered up */
	while(keyb_state.kbcount) {
		unsigned char ch = *(keyb_state.kbp++);
		keyb_state.kbcount--;
		put_rawkey(ch);
	}
}

/*
 * DANG_BEGIN_FUNCTION slang_keyb_init()
 * 
 * Code is called at start up to set up the terminal line for non-raw mode.
 * 
 * DANG_END_FUNCTION
 */
   
static int slang_keyb_init(void) 
{
	struct termios buf;

	k_printf("KBD: slang_keyb_init()\n");

	/* First initialize keyb_state */
	memset(&keyb_state, '\0', sizeof(keyb_state));
	keyb_state.kbd_fd = -1;
	keyb_state.kbcount = 0;
	keyb_state.kbp = &keyb_state.kbbuf[0];
	keyb_state.save_kbd_flags = -1;
	keyb_state.pc_scancode_mode = FALSE;
	keyb_state.The_Normal_KeyMap = (void *)0;
	
	keyb_state.Esc_Char = 0;
	keyb_state.erasekey = 0;
	keyb_state.KeyNot_Ready = TRUE;
	keyb_state.Keystr_Len = 0;
	keyb_state.Shift_Flags = 0;
	init_charset_state(&keyb_state.translate_state, trconfig.keyb_charset);

	SLtt_Force_Keypad_Init = 1;
	term_init();
	
	set_shiftstate(0);

	if (SLtt_tgetstr("S4") && SLtt_tgetstr("S5")) {
		keyb_state.pc_scancode_mode = TRUE;
	}
   
	keyb_state.kbd_fd = STDIN_FILENO;
	kbd_fd = keyb_state.kbd_fd; /* FIXME the kbd_fd global!! */
	keyb_state.save_kbd_flags = fcntl(keyb_state.kbd_fd, F_GETFL);
	fcntl(keyb_state.kbd_fd, F_SETFL, O_RDONLY | O_NONBLOCK);
   
	if (tcgetattr(keyb_state.kbd_fd, &keyb_state.save_termios) < 0
	    && errno != EINVAL && errno != ENOTTY) {
		error("slang_keyb_init(): Couldn't tcgetattr(kbd_fd,...) errno=%d\n", errno);
		return FALSE;
	}

	buf = keyb_state.save_termios;
	if (keyb_state.pc_scancode_mode) {
		buf.c_iflag = IGNBRK;
	} else {
		buf.c_iflag &= (ISTRIP | IGNBRK | IXON | IXOFF);
	}
	buf.c_cflag &= ~(CLOCAL | CSIZE | PARENB);  
	buf.c_cflag |= CS8;
	buf.c_lflag &= 0;	/* ISIG */
	buf.c_cc[VMIN] = 1;
	buf.c_cc[VTIME] = 0;
	keyb_state.erasekey = buf.c_cc[VERASE];

	if (tcsetattr(keyb_state.kbd_fd, TCSANOW, &buf) < 0
	    && errno != EINVAL && errno != ENOTTY) {
		error("slang_keyb_init(): Couldn't tcsetattr(kbd_fd,TCSANOW,...) !\n");
		return FALSE;
	}

	if (keyb_state.pc_scancode_mode) {
		setup_pc_scancode_mode();
		Keyboard_slang.run = do_pc_scancode_getkeys;
	} else {
		if (-1 == init_slang_keymaps()) {
			error("Unable to initialize S-Lang keymaps.\n");
			return FALSE;
		}
		Keyboard_slang.run = do_slang_getkeys;
	}

	if (!isatty(keyb_state.kbd_fd)) {
		k_printf("KBD: Using SIGIO\n");
		add_to_io_select(keyb_state.kbd_fd, 1, keyb_client_run);
	}
	else {
		k_printf("KBD: Not using SIGIO\n");
		add_to_io_select(keyb_state.kbd_fd, 0, keyb_client_run);
	}
   
	k_printf("KBD: slang_keyb_init() ok\n");
	return TRUE;
}

static void slang_keyb_close(void)  
{
	exit_pc_scancode_mode();
	if (tcsetattr(keyb_state.kbd_fd, TCSAFLUSH, &keyb_state.save_termios) < 0
	    && errno != EINVAL && errno != ENOTTY) {
		error("slang_keyb_close(): failed to restore keyboard termios settings!\n");
	}
	if (keyb_state.save_kbd_flags != -1) {
		fcntl(keyb_state.kbd_fd, F_SETFL, keyb_state.save_kbd_flags);
	}
	term_close();
	cleanup_charset_state(&keyb_state.translate_state);
}

/*
 * DANG_BEGIN_FUNCTION slang_keyb_probe()
 * 
 * Code is called at start up to see if we can use the slang keyboard.
 * 
 * DANG_END_FUNCTION
 */

static int slang_keyb_probe(void)
{
	struct termios buf;
	int result = TRUE;
	if (tcgetattr(STDIN_FILENO, &buf) >= 0
	    || errno == EINVAL || errno == ENOTTY)
		result = TRUE;
	return result;
}

struct keyboard_client Keyboard_slang =  {
	"slang",                    /* name */
	slang_keyb_probe,           /* probe */
	slang_keyb_init,            /* init */
	NULL,                       /* reset */
	slang_keyb_close,           /* close */
	do_slang_getkeys,           /* run */
	NULL,                       /* set_leds */
	handle_slang_keys	    /* handle_keys */
};
