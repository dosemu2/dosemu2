/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "config.h"
#include "types.h"

/* keyboard related PUBLIC definitions (for keyboard clients) */

#ifndef __ASM__

typedef Bit8u t_rawkeycode;
typedef Bit16u t_keysym;
typedef unsigned t_modifiers;
typedef Bit8u t_keynum;

/* no, this has nothing to do with a press release ;-) */

#define PRESS 1
#define RELEASE 0

#define KEYB_QUEUE_LENGTH (255)

/* public function definitions */
void put_rawkey(t_rawkeycode code);
int move_key(Boolean make, t_keysym key);
int move_keynum(Boolean make, t_keynum keynum, t_keysym sym);
t_keynum keysym_to_keynum(t_keysym key);
void put_symbol(Boolean make, t_keysym sym);
void put_modified_symbol(Boolean make, t_modifiers modifiers, t_keysym sym);
void set_shiftstate(t_modifiers s);
t_modifiers get_shiftstate(void);
int type_in_pre_strokes(void);
void append_pre_strokes(unsigned char *s);
struct char_set;

void keyb_priv_init(void);
void keyb_init(void);
void keyb_reset(void);
void keyb_close(void);

#endif /* not __ASM__ */

#include "unicode_symbols.h"

/* modifier bits */
#define MODIFIER_SHIFT	0x0001
#define MODIFIER_CTRL	0x0002
#define MODIFIER_ALT	0x0004
#define MODIFIER_ALTGR	0x0008
#define MODIFIER_CAPS	0x0010
#define MODIFIER_NUM	0x0020
#define MODIFIER_SCR	0x0040
#define MODIFIER_INS	0x0080

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

/* dosemu defined keysyms all start with KEY_
 * Note: for some keysyms that dosemu defines also exist as unicode
 * symbols and share the same values.
 */
/* main block */

#define KEY_SPACE	U_SPACE
#define KEY_BKSP	U_BACKSPACE
#define KEY_RETURN	U_CARRIAGE_RETURN
#define KEY_TAB		U_HORIZONTAL_TABULATION
#define KEY_A		U_LATIN_SMALL_LETTER_A
#define KEY_B		U_LATIN_SMALL_LETTER_B
#define KEY_C		U_LATIN_SMALL_LETTER_C
#define KEY_D		U_LATIN_SMALL_LETTER_D
#define KEY_E		U_LATIN_SMALL_LETTER_E
#define KEY_F		U_LATIN_SMALL_LETTER_F
#define KEY_G		U_LATIN_SMALL_LETTER_G
#define KEY_H		U_LATIN_SMALL_LETTER_H
#define KEY_I		U_LATIN_SMALL_LETTER_I
#define KEY_J		U_LATIN_SMALL_LETTER_J
#define KEY_K		U_LATIN_SMALL_LETTER_K
#define KEY_L		U_LATIN_SMALL_LETTER_L
#define KEY_M		U_LATIN_SMALL_LETTER_M
#define KEY_N		U_LATIN_SMALL_LETTER_N
#define KEY_O		U_LATIN_SMALL_LETTER_O
#define KEY_P		U_LATIN_SMALL_LETTER_P
#define KEY_Q		U_LATIN_SMALL_LETTER_Q
#define KEY_R		U_LATIN_SMALL_LETTER_R
#define KEY_S		U_LATIN_SMALL_LETTER_S
#define KEY_T		U_LATIN_SMALL_LETTER_T
#define KEY_U		U_LATIN_SMALL_LETTER_U
#define KEY_V		U_LATIN_SMALL_LETTER_V
#define KEY_W		U_LATIN_SMALL_LETTER_W
#define KEY_X		U_LATIN_SMALL_LETTER_X
#define KEY_Y		U_LATIN_SMALL_LETTER_Y
#define KEY_Z		U_LATIN_SMALL_LETTER_Z
#define KEY_1		U_DIGIT_ONE
#define KEY_2		U_DIGIT_TWO
#define KEY_3		U_DIGIT_THREE
#define KEY_4		U_DIGIT_FOUR
#define KEY_5		U_DIGIT_FIVE
#define KEY_6		U_DIGIT_SIX
#define KEY_7		U_DIGIT_SEVEN
#define KEY_8		U_DIGIT_EIGHT
#define KEY_9		U_DIGIT_NINE
#define KEY_0		U_DIGIT_ZERO
#define KEY_DASH	U_HYPHEN_MINUS
#define KEY_EQUALS	U_EQUALS_SIGN
#define KEY_LBRACK	U_LEFT_SQUARE_BRACKET
#define KEY_RBRACK	U_RIGHT_SQUARE_BRACKET
#define KEY_SEMICOLON	U_SEMICOLON
#define KEY_APOSTROPHE	U_APOSTROPHE   /* ' */
#define KEY_GRAVE	U_GRAVE_ACCENT   /* ` */
#define KEY_BACKSLASH	U_BACKSLASH
#define KEY_COMMA	U_COMMA
#define KEY_PERIOD	U_PERIOD
#define KEY_SLASH	U_SLASH

/* keypad */

#define KEY_PAD_0	0xE100
#define KEY_PAD_1	0xE101
#define KEY_PAD_2	0xE102
#define KEY_PAD_3	0xE103
#define KEY_PAD_4	0xE104
#define KEY_PAD_5	0xE105
#define KEY_PAD_6	0xE106
#define KEY_PAD_7	0xE107
#define KEY_PAD_8	0xE108
#define KEY_PAD_9	0xE109
#define KEY_PAD_DECIMAL	0xE10A
#define KEY_PAD_SLASH	0xE10B
#define KEY_PAD_AST	0xE10C
#define KEY_PAD_MINUS	0xE10D
#define KEY_PAD_PLUS	0xE10E
#define KEY_PAD_ENTER	0xE10F
#define KEY_PAD_HOME	0xE110
#define KEY_PAD_UP	0xE111
#define KEY_PAD_PGUP	0xE112
#define KEY_PAD_LEFT	0xE113
#define KEY_PAD_CENTER	0xE114
#define KEY_PAD_RIGHT	0xE115
#define KEY_PAD_END	0xE116
#define KEY_PAD_DOWN	0xE117
#define KEY_PAD_PGDN	0xE118
#define KEY_PAD_INS	0xE119
#define KEY_PAD_DEL	0xE11A

/* function keys */

#define KEY_ESC		U_ESCAPE
#define KEY_F1		0xE11B
#define KEY_F2		0xE11C
#define KEY_F3		0xE11D
#define KEY_F4		0xE11E
#define KEY_F5		0xE11F
#define KEY_F6		0xE120
#define KEY_F7		0xE121
#define KEY_F8		0xE122
#define KEY_F9		0xE123
#define KEY_F10		0xE124
#define KEY_F11		0xE125
#define KEY_F12		0xE126

/* cursor block */

#define KEY_INS		0xE127
#define KEY_DEL		0xE128
#define KEY_HOME	0xE129
#define KEY_END		0xE12A
#define KEY_PGUP	0xE12B
#define KEY_PGDN	0xE12C
#define KEY_UP		0xE12D
#define KEY_DOWN	0xE12E
#define KEY_LEFT	0xE12F
#define KEY_RIGHT	0xE130

/* shift keys */

#define KEY_L_ALT	0xE131
#define KEY_R_ALT	0xE132
#define KEY_L_CTRL	0xE133
#define KEY_R_CTRL	0xE134
#define KEY_L_SHIFT	0xE135
#define KEY_R_SHIFT	0xE136
#define KEY_NUM		0xE137
#define KEY_SCROLL	0xE138
#define KEY_CAPS	0xE139
#define KEY_MODE_SWITCH	KEY_R_ALT /* for now */

/* special keys */
#define KEY_PRTSCR	0xE13A
#define KEY_PAUSE	0xE13B
#define KEY_SYSRQ	0xE13C
#define KEY_BREAK	0xE13D

/* more */
#define KEY_PAD_SEPARATOR	0xE13E


/* alt (I assume this are contiguous, and in alphabetical order )  */
#define KEY_ALT_A		0xE13F
#define KEY_ALT_B		0xE140
#define KEY_ALT_C		0xE141
#define KEY_ALT_D		0xE142
#define KEY_ALT_E		0xE143
#define KEY_ALT_F		0xE144
#define KEY_ALT_G		0xE145
#define KEY_ALT_H		0xE146
#define KEY_ALT_I		0xE147
#define KEY_ALT_J		0xE148
#define KEY_ALT_K		0xE149
#define KEY_ALT_L		0xE14A
#define KEY_ALT_M		0xE14B
#define KEY_ALT_N		0xE14C
#define KEY_ALT_O		0xE14D
#define KEY_ALT_P		0xE14E
#define KEY_ALT_Q		0xE14F
#define KEY_ALT_R		0xE150
#define KEY_ALT_S		0xE151
#define KEY_ALT_T		0xE152
#define KEY_ALT_U		0xE153
#define KEY_ALT_V		0xE154
#define KEY_ALT_W		0xE155
#define KEY_ALT_X		0xE156
#define KEY_ALT_Y		0xE157
#define KEY_ALT_Z		0xE158

/* more */
#define KEY_LEFT_TAB		0xE159

#define KEY_ALTGR_LOCK		0xE15A

/* dead */
#define KEY_DEAD_ALT		0xE200
#define KEY_DEAD_CTRL		0xE201
#define KEY_DEAD_SHIFT		0xE202
#define KEY_DEAD_ALTGR		0xE203
#define KEY_DEAD_KEY_PAD	0xE204

/* dosemu special functions */
#define KEY_DOSEMU_HELP			0xE300
#define KEY_DOSEMU_REDRAW		0xE301
#define KEY_DOSEMU_SUSPEND		0xE302
#define KEY_DOSEMU_RESET		0xE303
#define KEY_DOSEMU_MONO			0xE304
#define KEY_DOSEMU_PAN_UP		0xE305
#define KEY_DOSEMU_PAN_DOWN		0xE306
#define KEY_DOSEMU_PAN_LEFT		0xE307
#define KEY_DOSEMU_PAN_RIGHT		0xE308
#define KEY_DOSEMU_REBOOT		0xE309
#define KEY_DOSEMU_EXIT			0xE30A
#define KEY_DOSEMU_VT_1			0xE30B
#define KEY_DOSEMU_VT_2			0xE30C
#define KEY_DOSEMU_VT_3			0xE30D
#define KEY_DOSEMU_VT_4			0xE30E
#define KEY_DOSEMU_VT_5			0xE30F
#define KEY_DOSEMU_VT_6			0xE310
#define KEY_DOSEMU_VT_7			0xE311
#define KEY_DOSEMU_VT_8			0xE312
#define KEY_DOSEMU_VT_9			0xE313
#define KEY_DOSEMU_VT_10		0xE314
#define KEY_DOSEMU_VT_11		0xE315
#define KEY_DOSEMU_VT_12		0xE316
#define KEY_DOSEMU_VT_NEXT		0xE317
#define KEY_DOSEMU_VT_PREV		0xE318
#define KEY_MOUSE_UP			0xE319
#define KEY_MOUSE_DOWN			0xE31A
#define KEY_MOUSE_LEFT			0xE31B
#define KEY_MOUSE_RIGHT			0xE31C
#define KEY_MOUSE_UP_AND_LEFT		0xE31D
#define KEY_MOUSE_UP_AND_RIGHT		0xE31E
#define KEY_MOUSE_DOWN_AND_LEFT		0xE31F
#define KEY_MOUSE_DOWN_AND_RIGHT	0xE320
#define KEY_MOUSE_BUTTON_LEFT		0xE321
#define KEY_MOUSE_BUTTON_MIDDLE		0xE322
#define KEY_MOUSE_BUTTON_RIGHT		0xE323
#define KEY_MOUSE_GRAB			0xE324
#define KEY_DOSEMU_X86EMU_DEBUG		0xE325
#define KEY_MOUSE			0xE326
#define KEY_DOSEMU_FREEZE		0xE327

/* keys that X has and dosemu doesn't */
#define KEY_DOSEMU_CLEAR		KEY_VOID
#define KEY_DOSEMU_BEGIN		KEY_VOID
#define KEY_DOSEMU_SELECT		KEY_VOID
#define KEY_DOSEMU_EXECUTE		KEY_VOID
#define KEY_DOSEMU_UNDO			KEY_VOID
#define KEY_DOSEMU_REDO			KEY_VOID
#define KEY_DOSEMU_MENU			KEY_VOID
#define KEY_DOSEMU_FIND			KEY_VOID
#define KEY_DOSEMU_CANCEL		KEY_VOID
#define KEY_PAD_SPACE			KEY_VOID
#define KEY_PAD_F1			KEY_VOID
#define KEY_PAD_F2			KEY_VOID
#define KEY_PAD_F3			KEY_VOID
#define KEY_PAD_F4			KEY_VOID
#define KEY_PAD_EQUAL			KEY_VOID
#define KEY_PAD_TAB			KEY_VOID

#define KEY_F13				KEY_VOID
#define KEY_F14				KEY_VOID
#define KEY_F15				KEY_VOID
#define KEY_F16				KEY_VOID
#define KEY_F17				KEY_VOID
#define KEY_F18				KEY_VOID
#define KEY_F19				KEY_VOID
#define KEY_F20				KEY_VOID
#define KEY_F21				KEY_VOID
#define KEY_F22				KEY_VOID
#define KEY_F23				KEY_VOID
#define KEY_F24				KEY_VOID
#define KEY_F25				KEY_VOID
#define KEY_F26				KEY_VOID
#define KEY_F27				KEY_VOID
#define KEY_F28				KEY_VOID
#define KEY_F29				KEY_VOID
#define KEY_F30				KEY_VOID
#define KEY_F31				KEY_VOID
#define KEY_F32				KEY_VOID
#define KEY_F33				KEY_VOID
#define KEY_F34				KEY_VOID
#define KEY_F35				KEY_VOID

#define KEY_L_META			KEY_L_ALT
#define KEY_R_META			KEY_R_ALT
#define KEY_L_SUPER			KEY_VOID
#define KEY_R_SUPER			KEY_VOID
#define KEY_L_HYPER			KEY_VOID
#define KEY_R_HYPER			KEY_VOID

#define KEY_SHIFT_LOCK			KEY_VOID
#define KEY_MULTI_KEY			KEY_VOID /* compose? */

/* Keysym value that is never a key */
#define KEY_VOID        U_VOID


/* compatiblity control defines (strange unicode names ignored...) */
#define KEY_NULL		0x0000
#define KEY_CONTROL_A		0x0001
#define KEY_CONTROL_B		0x0002
#define KEY_CONTROL_C		0x0003
#define KEY_CONTROL_D		0x0004
#define KEY_CONTROL_E		0x0005
#define KEY_CONTROL_F		0x0006
#define KEY_CONTROL_G		0x0007
#define KEY_CONTROL_H		0x0008
#define KEY_CONTROL_I		0x0009
#define KEY_CONTROL_J		0x000A
#define KEY_CONTROL_K		0x000B
#define KEY_CONTROL_L		0x000C
#define KEY_CONTROL_M		0x000D
#define KEY_CONTROL_N		0x000E
#define KEY_CONTROL_O		0x000F
#define KEY_CONTROL_P		0x0010
#define KEY_CONTROL_Q		0x0011
#define KEY_CONTROL_R		0x0012
#define KEY_CONTROL_S		0x0013
#define KEY_CONTROL_T		0x0014
#define KEY_CONTROL_U		0x0015
#define KEY_CONTROL_V		0x0016
#define KEY_CONTROL_W		0x0017
#define KEY_CONTROL_X		0x0018
#define KEY_CONTROL_Y		0x0019
#define KEY_CONTROL_Z		0x001A
#define KEY_CONTROL_LBRACK	0x001B
#define KEY_CONTROL_BACKSLASH	0x001C
#define KEY_CONTROL_RBRACK	0x001D
#define KEY_CONTROL_CARET	0x001E
#define KEY_CONTROL_UNDERSCORE	0x001F

/* dead keys */
/* overload the unicode combining characters */
#define KEY_DEAD_GRAVE		U_COMBINING_GRAVE_ACCENT	/* 0x0300 */
#define KEY_DEAD_ACUTE		U_COMBINING_ACUTE_ACCENT	/* 0x0301 */
#define KEY_DEAD_CIRCUMFLEX	U_COMBINING_CIRCUMFLEX_ACCENT	/* 0x0302 */
#define KEY_DEAD_TILDE		U_COMBINING_TILDE		/* 0x0303 */
#define KEY_DEAD_BREVE		U_COMBINING_BREVE		/* 0x0306 */
#define KEY_DEAD_ABOVEDOT	U_COMBINING_DOT_ABOVE		/* 0x0307 */
#define KEY_DEAD_DIAERESIS	U_COMBINING_DIAERESIS		/* 0x0308 */
#define KEY_DEAD_ABOVERING	U_COMBINING_RING_ABOVE		/* 0x030A */
#define KEY_DEAD_DOUBLEACUTE	U_COMBINING_DOUBLE_ACUTE_ACCENT	/* 0x030B */
#define KEY_DEAD_CEDILLA	U_COMBINING_CEDILLA		/* 0x0327 */
#if 0
#define KEY_DEAD_IOTA		U_COMBINING_???			/* doesn't exist? */
#endif
#define KEY_DEAD_OGONEK		U_COMBINING_OGONEK		/* 0x0328  */
#define KEY_DEAD_CARON		U_COMBINING_CARON		/* 0x030C */


#endif /* not _KEYBOARD_H */
