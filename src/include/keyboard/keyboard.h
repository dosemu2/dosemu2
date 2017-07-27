/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "dosemu_config.h"
#include "types.h"
#include "translate/unicode_symbols.h"

#define HAVE_UNICODE_KEYB 2

/* keyboard related PUBLIC definitions (for keyboard clients) */

#ifndef __ASM__

typedef Bit8u t_rawkeycode;
typedef Bit16u t_keysym;
typedef unsigned t_modifiers;
typedef Bit8u t_keynum;
typedef Bit16u t_shiftstate;
typedef Bit32u t_scancode;

/* bits in t_shiftstate */

#define CAPS_LOCK      0x0001
#define NUM_LOCK       0x0002
#define SCR_LOCK       0x0004
#define INS_LOCK       0x0008
#define L_ALT          0x0010
#define R_ALT          0x0020
#define L_SHIFT        0x0040
#define R_SHIFT        0x0080
#define L_CTRL         0x0100
#define R_CTRL         0x0200
#define CAPS_PRESSED   0x0400
#define NUM_PRESSED    0x0800
#define SCR_PRESSED    0x1000
#define INS_PRESSED    0x2000
#define SYSRQ_PRESSED  0x4000
#define ANY_ALT            (L_ALT|R_ALT)
#define ANY_SHIFT          (L_SHIFT|R_SHIFT)
#define ANY_CTRL           (L_CTRL|R_CTRL)

/* no, this has nothing to do with a press release ;-) */

#define PRESS 1
#define RELEASE 0

#define KEYB_QUEUE_LENGTH (255)

/* public function definitions */
void put_rawkey(t_rawkeycode code);
int move_key(Boolean make, t_keysym key);
int move_keynum(Boolean make, t_keynum keynum, t_keysym sym);
t_keynum keysym_to_keynum(t_keysym key, t_modifiers * modifiers);
void put_symbol(Boolean make, t_keysym sym);
void put_modified_symbol(Boolean make, t_modifiers modifiers, t_keysym sym);
void set_shiftstate(t_modifiers s);
t_modifiers get_shiftstate(void);
int type_in_pre_strokes(void);
void append_pre_strokes(char *s);
struct char_set;

void keyb_priv_init(void);
void keyb_init(void);
void keyb_reset(void);
void keyb_close(void);

void put_shift_state(t_shiftstate shift);

#endif /* not __ASM__ */

/* modifier bits */
#define MODIFIER_SHIFT	0x0001
#define MODIFIER_CTRL	0x0002
#define MODIFIER_ALT	0x0004
#define MODIFIER_ALTGR	0x0008
#define MODIFIER_CAPS	0x0010
#define MODIFIER_NUM	0x0020
#define MODIFIER_SCR	0x0040
#define MODIFIER_INS	0x0080

/* FIXME: Is this a good value for MODIFIER_VOID? */
#define MODIFIER_VOID	U_VOID


#define NUM_KEYSYMS 0x10000 /* really 0xFFFE */

/* Keysyms: Constants to be passed to put_key().
   These constants are equal to unicode symbols,
   except where unicode is lacking in which they are mapped to
   the application specific unicode area.
   They are functionally equivalent to but different from X11 Keysyms.

   A keysym is the value that sits on the top of a key and defines
   either the character the key produces, or the function of that key shift, up...
   In a give keyboard layout there cannot be 2 instances of the same keysym, on
   two different keys.  So multiple arrow keys need different keysyms.

   Because the characters a key produces are the most likely to change
   especailly in a dos emulator, where the other keycaps are fixed by
   what a pc keyboard can do, I have picked unicode as the character
   set of my keysyms, and am using the private space 0xE000 - 0xEFFF
   to encode the keysyms I need that are not characters.
   This encoding also facilitates in translating between character
   sets, in finding dead keys (unicode more or less already does
   that), and in getting a complete multinational keysym set produced.

Note: The range 0xEF00 to 0xEFFF is reserved as a pass through to
the current display character set.  For places where I need such a thing.
 */

/* dosemu defined keysyms all start with DKY_
 * Note: for some keysyms that dosemu defines also exist as unicode
 * symbols and share the same values.
 */
/* main block */

#define DKY_SPACE	U_SPACE
#define DKY_BKSP	U_BACKSPACE
#define DKY_RETURN	U_CARRIAGE_RETURN
#define DKY_TAB		U_HORIZONTAL_TABULATION
#define DKY_A		U_LATIN_SMALL_LETTER_A
#define DKY_B		U_LATIN_SMALL_LETTER_B
#define DKY_C		U_LATIN_SMALL_LETTER_C
#define DKY_D		U_LATIN_SMALL_LETTER_D
#define DKY_E		U_LATIN_SMALL_LETTER_E
#define DKY_F		U_LATIN_SMALL_LETTER_F
#define DKY_G		U_LATIN_SMALL_LETTER_G
#define DKY_H		U_LATIN_SMALL_LETTER_H
#define DKY_I		U_LATIN_SMALL_LETTER_I
#define DKY_J		U_LATIN_SMALL_LETTER_J
#define DKY_K		U_LATIN_SMALL_LETTER_K
#define DKY_L		U_LATIN_SMALL_LETTER_L
#define DKY_M		U_LATIN_SMALL_LETTER_M
#define DKY_N		U_LATIN_SMALL_LETTER_N
#define DKY_O		U_LATIN_SMALL_LETTER_O
#define DKY_P		U_LATIN_SMALL_LETTER_P
#define DKY_Q		U_LATIN_SMALL_LETTER_Q
#define DKY_R		U_LATIN_SMALL_LETTER_R
#define DKY_S		U_LATIN_SMALL_LETTER_S
#define DKY_T		U_LATIN_SMALL_LETTER_T
#define DKY_U		U_LATIN_SMALL_LETTER_U
#define DKY_V		U_LATIN_SMALL_LETTER_V
#define DKY_W		U_LATIN_SMALL_LETTER_W
#define DKY_X		U_LATIN_SMALL_LETTER_X
#define DKY_Y		U_LATIN_SMALL_LETTER_Y
#define DKY_Z		U_LATIN_SMALL_LETTER_Z
#define DKY_1		U_DIGIT_ONE
#define DKY_2		U_DIGIT_TWO
#define DKY_3		U_DIGIT_THREE
#define DKY_4		U_DIGIT_FOUR
#define DKY_5		U_DIGIT_FIVE
#define DKY_6		U_DIGIT_SIX
#define DKY_7		U_DIGIT_SEVEN
#define DKY_8		U_DIGIT_EIGHT
#define DKY_9		U_DIGIT_NINE
#define DKY_0		U_DIGIT_ZERO
#define DKY_DASH	U_HYPHEN_MINUS
#define DKY_EQUALS	U_EQUALS_SIGN
#define DKY_LBRACK	U_LEFT_SQUARE_BRACKET
#define DKY_RBRACK	U_RIGHT_SQUARE_BRACKET
#define DKY_SEMICOLON	U_SEMICOLON
#define DKY_APOSTROPHE	U_APOSTROPHE   /* ' */
#define DKY_GRAVE	U_GRAVE_ACCENT   /* ` */
#define DKY_BACKSLASH	U_BACKSLASH
#define DKY_COMMA	U_COMMA
#define DKY_PERIOD	U_PERIOD
#define DKY_SLASH	U_SLASH

/* keypad */

#define DKY_PAD_0	0xE100
#define DKY_PAD_1	0xE101
#define DKY_PAD_2	0xE102
#define DKY_PAD_3	0xE103
#define DKY_PAD_4	0xE104
#define DKY_PAD_5	0xE105
#define DKY_PAD_6	0xE106
#define DKY_PAD_7	0xE107
#define DKY_PAD_8	0xE108
#define DKY_PAD_9	0xE109
#define DKY_PAD_DECIMAL	0xE10A
#define DKY_PAD_SLASH	0xE10B
#define DKY_PAD_AST	0xE10C
#define DKY_PAD_MINUS	0xE10D
#define DKY_PAD_PLUS	0xE10E
#define DKY_PAD_ENTER	0xE10F
#define DKY_PAD_HOME	0xE110
#define DKY_PAD_UP	0xE111
#define DKY_PAD_PGUP	0xE112
#define DKY_PAD_LEFT	0xE113
#define DKY_PAD_CENTER	0xE114
#define DKY_PAD_RIGHT	0xE115
#define DKY_PAD_END	0xE116
#define DKY_PAD_DOWN	0xE117
#define DKY_PAD_PGDN	0xE118
#define DKY_PAD_INS	0xE119
#define DKY_PAD_DEL	0xE11A

/* function keys */

#define DKY_ESC		U_ESCAPE
#define DKY_F1		0xE11B
#define DKY_F2		0xE11C
#define DKY_F3		0xE11D
#define DKY_F4		0xE11E
#define DKY_F5		0xE11F
#define DKY_F6		0xE120
#define DKY_F7		0xE121
#define DKY_F8		0xE122
#define DKY_F9		0xE123
#define DKY_F10		0xE124
#define DKY_F11		0xE125
#define DKY_F12		0xE126

/* cursor block */

#define DKY_INS		0xE127
#define DKY_DEL		0xE128
#define DKY_HOME	0xE129
#define DKY_END		0xE12A
#define DKY_PGUP	0xE12B
#define DKY_PGDN	0xE12C
#define DKY_UP		0xE12D
#define DKY_DOWN	0xE12E
#define DKY_LEFT	0xE12F
#define DKY_RIGHT	0xE130

/* shift keys */

#define DKY_L_ALT	0xE131
#define DKY_R_ALT	0xE132
#define DKY_L_CTRL	0xE133
#define DKY_R_CTRL	0xE134
#define DKY_L_SHIFT	0xE135
#define DKY_R_SHIFT	0xE136
#define DKY_NUM		0xE137
#define DKY_SCROLL	0xE138
#define DKY_CAPS	0xE139
#define DKY_MODE_SWITCH	DKY_R_ALT /* for now */

/* special keys */
#define DKY_PRTSCR	0xE13A
#define DKY_PAUSE	0xE13B
#define DKY_SYSRQ	0xE13C
#define DKY_BREAK	0xE13D

/* more */
#define DKY_PAD_SEPARATOR	0xE13E


/* alt (I assume this are contiguous, and in alphabetical order )  */
#define DKY_ALT_A		0xE13F
#define DKY_ALT_B		0xE140
#define DKY_ALT_C		0xE141
#define DKY_ALT_D		0xE142
#define DKY_ALT_E		0xE143
#define DKY_ALT_F		0xE144
#define DKY_ALT_G		0xE145
#define DKY_ALT_H		0xE146
#define DKY_ALT_I		0xE147
#define DKY_ALT_J		0xE148
#define DKY_ALT_K		0xE149
#define DKY_ALT_L		0xE14A
#define DKY_ALT_M		0xE14B
#define DKY_ALT_N		0xE14C
#define DKY_ALT_O		0xE14D
#define DKY_ALT_P		0xE14E
#define DKY_ALT_Q		0xE14F
#define DKY_ALT_R		0xE150
#define DKY_ALT_S		0xE151
#define DKY_ALT_T		0xE152
#define DKY_ALT_U		0xE153
#define DKY_ALT_V		0xE154
#define DKY_ALT_W		0xE155
#define DKY_ALT_X		0xE156
#define DKY_ALT_Y		0xE157
#define DKY_ALT_Z		0xE158

/* more */
#define DKY_LEFT_TAB		0xE159

#define DKY_ALTGR_LOCK		0xE15A

/* dead */
#define DKY_DEAD_ALT		0xE200
#define DKY_DEAD_CTRL		0xE201
#define DKY_DEAD_SHIFT		0xE202
#define DKY_DEAD_ALTGR		0xE203
#define DKY_DEAD_DKY_PAD	0xE204

/* dosemu special functions */
#define DKY_DOSEMU_HELP			0xE300
#define DKY_DOSEMU_REDRAW		0xE301
#define DKY_DOSEMU_SUSPEND		0xE302
#define DKY_DOSEMU_RESET		0xE303
#define DKY_DOSEMU_MONO			0xE304
#define DKY_DOSEMU_PAN_UP		0xE305
#define DKY_DOSEMU_PAN_DOWN		0xE306
#define DKY_DOSEMU_PAN_LEFT		0xE307
#define DKY_DOSEMU_PAN_RIGHT		0xE308
#define DKY_DOSEMU_REBOOT		0xE309
#define DKY_DOSEMU_EXIT			0xE30A
#define DKY_DOSEMU_VT_1			0xE30B
#define DKY_DOSEMU_VT_2			0xE30C
#define DKY_DOSEMU_VT_3			0xE30D
#define DKY_DOSEMU_VT_4			0xE30E
#define DKY_DOSEMU_VT_5			0xE30F
#define DKY_DOSEMU_VT_6			0xE310
#define DKY_DOSEMU_VT_7			0xE311
#define DKY_DOSEMU_VT_8			0xE312
#define DKY_DOSEMU_VT_9			0xE313
#define DKY_DOSEMU_VT_10		0xE314
#define DKY_DOSEMU_VT_11		0xE315
#define DKY_DOSEMU_VT_12		0xE316
#define DKY_DOSEMU_VT_NEXT		0xE317
#define DKY_DOSEMU_VT_PREV		0xE318
#define DKY_MOUSE_UP			0xE319
#define DKY_MOUSE_DOWN			0xE31A
#define DKY_MOUSE_LEFT			0xE31B
#define DKY_MOUSE_RIGHT			0xE31C
#define DKY_MOUSE_UP_AND_LEFT		0xE31D
#define DKY_MOUSE_UP_AND_RIGHT		0xE31E
#define DKY_MOUSE_DOWN_AND_LEFT		0xE31F
#define DKY_MOUSE_DOWN_AND_RIGHT	0xE320
#define DKY_MOUSE_BUTTON_LEFT		0xE321
#define DKY_MOUSE_BUTTON_MIDDLE		0xE322
#define DKY_MOUSE_BUTTON_RIGHT		0xE323
#define DKY_MOUSE_GRAB			0xE324
#define DKY_DOSEMU_X86EMU_DEBUG		0xE325
#define DKY_MOUSE			0xE326
#define DKY_DOSEMU_FREEZE		0xE327

/* keys that X has and dosemu doesn't */
#define DKY_DOSEMU_CLEAR		DKY_VOID
#define DKY_DOSEMU_BEGIN		DKY_VOID
#define DKY_DOSEMU_SELECT		DKY_VOID
#define DKY_DOSEMU_EXECUTE		DKY_VOID
#define DKY_DOSEMU_UNDO			DKY_VOID
#define DKY_DOSEMU_REDO			DKY_VOID
#define DKY_DOSEMU_MENU			DKY_VOID
#define DKY_DOSEMU_FIND			DKY_VOID
#define DKY_DOSEMU_CANCEL		DKY_VOID
#define DKY_PAD_SPACE			DKY_VOID
#define DKY_PAD_F1			DKY_VOID
#define DKY_PAD_F2			DKY_VOID
#define DKY_PAD_F3			DKY_VOID
#define DKY_PAD_F4			DKY_VOID
#define DKY_PAD_EQUAL			DKY_VOID
#define DKY_PAD_TAB			DKY_VOID

#define DKY_F13				DKY_VOID
#define DKY_F14				DKY_VOID
#define DKY_F15				DKY_VOID
#define DKY_F16				DKY_VOID
#define DKY_F17				DKY_VOID
#define DKY_F18				DKY_VOID
#define DKY_F19				DKY_VOID
#define DKY_F20				DKY_VOID
#define DKY_F21				DKY_VOID
#define DKY_F22				DKY_VOID
#define DKY_F23				DKY_VOID
#define DKY_F24				DKY_VOID
#define DKY_F25				DKY_VOID
#define DKY_F26				DKY_VOID
#define DKY_F27				DKY_VOID
#define DKY_F28				DKY_VOID
#define DKY_F29				DKY_VOID
#define DKY_F30				DKY_VOID
#define DKY_F31				DKY_VOID
#define DKY_F32				DKY_VOID
#define DKY_F33				DKY_VOID
#define DKY_F34				DKY_VOID
#define DKY_F35				DKY_VOID

#define DKY_L_META			DKY_L_ALT
#define DKY_R_META			DKY_R_ALT
#define DKY_L_SUPER			DKY_VOID
#define DKY_R_SUPER			DKY_VOID
#define DKY_L_HYPER			DKY_VOID
#define DKY_R_HYPER			DKY_VOID

#define DKY_SHIFT_LOCK			DKY_VOID
#define DKY_MULTI_KEY			DKY_VOID /* compose? */

/* Keysym value that is never a key */
#define DKY_VOID        U_VOID


/* compatiblity control defines (strange unicode names ignored...) */
#define DKY_NULL		0x0000
#define DKY_CONTROL_A		0x0001
#define DKY_CONTROL_B		0x0002
#define DKY_CONTROL_C		0x0003
#define DKY_CONTROL_D		0x0004
#define DKY_CONTROL_E		0x0005
#define DKY_CONTROL_F		0x0006
#define DKY_CONTROL_G		0x0007
#define DKY_CONTROL_H		0x0008
#define DKY_CONTROL_I		0x0009
#define DKY_CONTROL_J		0x000A
#define DKY_CONTROL_K		0x000B
#define DKY_CONTROL_L		0x000C
#define DKY_CONTROL_M		0x000D
#define DKY_CONTROL_N		0x000E
#define DKY_CONTROL_O		0x000F
#define DKY_CONTROL_P		0x0010
#define DKY_CONTROL_Q		0x0011
#define DKY_CONTROL_R		0x0012
#define DKY_CONTROL_S		0x0013
#define DKY_CONTROL_T		0x0014
#define DKY_CONTROL_U		0x0015
#define DKY_CONTROL_V		0x0016
#define DKY_CONTROL_W		0x0017
#define DKY_CONTROL_X		0x0018
#define DKY_CONTROL_Y		0x0019
#define DKY_CONTROL_Z		0x001A
#define DKY_CONTROL_LBRACK	0x001B
#define DKY_CONTROL_BACKSLASH	0x001C
#define DKY_CONTROL_RBRACK	0x001D
#define DKY_CONTROL_CARET	0x001E
#define DKY_CONTROL_UNDERSCORE	0x001F

/* dead keys */
/* overload the unicode combining characters */
#define DKY_DEAD_GRAVE		U_COMBINING_GRAVE_ACCENT	/* 0x0300 */
#define DKY_DEAD_ACUTE		U_COMBINING_ACUTE_ACCENT	/* 0x0301 */
#define DKY_DEAD_CIRCUMFLEX	U_COMBINING_CIRCUMFLEX_ACCENT	/* 0x0302 */
#define DKY_DEAD_TILDE		U_COMBINING_TILDE		/* 0x0303 */
#define DKY_DEAD_BREVE		U_COMBINING_BREVE		/* 0x0306 */
#define DKY_DEAD_ABOVEDOT	U_COMBINING_DOT_ABOVE		/* 0x0307 */
#define DKY_DEAD_DIAERESIS	U_COMBINING_DIAERESIS		/* 0x0308 */
#define DKY_DEAD_ABOVERING	U_COMBINING_RING_ABOVE		/* 0x030A */
#define DKY_DEAD_DOUBLEACUTE	U_COMBINING_DOUBLE_ACUTE_ACCENT	/* 0x030B */
#define DKY_DEAD_CEDILLA	U_COMBINING_CEDILLA		/* 0x0327 */
#if 0
#define DKY_DEAD_IOTA		U_COMBINING_???			/* doesn't exist? */
#endif
#define DKY_DEAD_OGONEK		U_COMBINING_OGONEK		/* 0x0328  */
#define DKY_DEAD_CARON		U_COMBINING_CARON		/* 0x030C */


#endif /* not _KEYBOARD_H */
