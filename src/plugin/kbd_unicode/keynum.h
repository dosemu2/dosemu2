#ifndef keynum_h
#define keynum_h
/* 
 * key numbers 
 * Keynums are unique identifiers of physical keys. (As in X)
 *
 * The names are taken from the symbols on those keys in the standard
 * us keyboard layout.
 *
 * These are different from keysyms which identify are numbers
 * identifying the labels on particular keys.
 *
 * Most applications should use keysyms, not keynums,
 * so the code still works when the keyboard is remapped.
 */
#include "keyboard.h"

/* shift keys */

#define NUM_L_ALT	0x38
#define NUM_R_ALT	(0x38 | 0x80)
#define NUM_L_CTRL	0x1d
#define NUM_R_CTRL	(0x1d | 0x80)
#define NUM_L_SHIFT	0x2a
#define NUM_R_SHIFT	0x36
#define NUM_NUM		0x45
#define NUM_SCROLL	0x46
#define NUM_CAPS	0x3a

/* main block */

#define NUM_SPACE       0x39
#define NUM_BKSP	0x0e
#define NUM_RETURN	0x1c
#define NUM_TAB		0x0f
#define NUM_A		0x1e
#define NUM_B		0x30
#define NUM_C		0x2e
#define NUM_D		0x20
#define NUM_E		0x12
#define NUM_F		0x21
#define NUM_G		0x22
#define NUM_H		0x23
#define NUM_I		0x17
#define NUM_J		0x24
#define NUM_K		0x25
#define NUM_L		0x26
#define NUM_M		0x32
#define NUM_N		0x31
#define NUM_O		0x18
#define NUM_P		0x19
#define NUM_Q		0x10
#define NUM_R		0x13
#define NUM_S		0x1f
#define NUM_T		0x14
#define NUM_U		0x16
#define NUM_V		0x2f
#define NUM_W		0x11
#define NUM_X		0x2d
#define NUM_Y		0x15
#define NUM_Z		0x2c
#define NUM_1		0x02
#define NUM_2		0x03
#define NUM_3		0x04
#define NUM_4		0x05
#define NUM_5		0x06
#define NUM_6		0x07
#define NUM_7		0x08
#define NUM_8		0x09
#define NUM_9		0x0a
#define NUM_0		0x0b
#define NUM_DASH	0x0c
#define NUM_EQUALS	0x0d
#define NUM_LBRACK	0x1a
#define NUM_RBRACK	0x1b
#define NUM_SEMICOLON	0x27
#define NUM_APOSTROPHE	0x28	/* ' */
#define NUM_GRAVE	0x29	/* ` */
#define NUM_BACKSLASH	0x2b
#define NUM_COMMA	0x33
#define NUM_PERIOD	0x34
#define NUM_SLASH	0x35
#define NUM_LESSGREATER	0x56	/* not on US keyboards */

/* keypad */

#define NUM_PAD_0	0x52
#define NUM_PAD_1	0x4f
#define NUM_PAD_2	0x50
#define NUM_PAD_3	0x51
#define NUM_PAD_4	0x4b
#define NUM_PAD_5	0x4c
#define NUM_PAD_6	0x4d
#define NUM_PAD_7	0x47
#define NUM_PAD_8	0x48
#define NUM_PAD_9	0x49
#define NUM_PAD_DECIMAL	0x53
#define NUM_PAD_SLASH	(0x35 | 0x80)
#define NUM_PAD_AST	0x37
#define NUM_PAD_MINUS	0x4a
#define NUM_PAD_PLUS	0x4e
#define NUM_PAD_ENTER	(0x1c | 0x80)


/* function keys */

#define NUM_ESC		0x01
#define NUM_F1 		0x3b
#define NUM_F2 		0x3c
#define NUM_F3 		0x3d
#define NUM_F4 		0x3e
#define NUM_F5 		0x3f
#define NUM_F6 		0x40
#define NUM_F7 		0x41
#define NUM_F8 		0x42
#define NUM_F9 		0x43
#define NUM_F10		0x44
#define NUM_F11		0x57
#define NUM_F12		0x58

/* cursor block */

#define NUM_INS		(0x52 | 0x80)
#define NUM_DEL		(0x53 | 0x80)
#define NUM_HOME	(0x47 | 0x80)
#define NUM_END		(0x4f | 0x80)
#define NUM_PGUP	(0x49 | 0x80)
#define NUM_PGDN	(0x51 | 0x80)
#define NUM_UP		(0x48 | 0x80)
#define NUM_DOWN	(0x50 | 0x80)
#define NUM_LEFT	(0x4b | 0x80)
#define NUM_RIGHT	(0x4d | 0x80)

/* New keys on a Microsoft Windows keyboard */
#define NUM_LWIN	(0x5b | 0x80)
#define NUM_RWIN	(0x5c | 0x80)
#define NUM_MENU	(0x5d | 0x80)

/* Dual scancode keys */
#define NUM_PRTSCR_SYSRQ	0x54
#define NUM_PAUSE_BREAK		(0x46 | 0x80)

/* Keynum value that is never a key */
#define NUM_VOID         0


/* These are logical rather than physical keys:
 * The scancodes for the keys exist but the keys really don't
 */
/* fake codes start at very end of our range */
#define NUM_FAKE_L_SHIFT	0xff
#define NUM_FAKE_R_SHIFT	0xfe
#define NUM_PRTSCR		0xfd
#define NUM_SYSRQ		0xfc
#define NUM_PAUSE		0xfb
#define NUM_BREAK		0xfa
#define NUM_KEY_NUMS	  256 /* actually it's less... */

extern t_keynum validate_keynum(t_keynum key);

#endif /* keynum_h */
