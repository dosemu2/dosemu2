/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "config.h"
#include "types.h"


/* keyboard related PUBLIC definitions (for keyboard clients) */

#ifndef __ASM__

typedef Bit8u t_rawkeycode;
typedef Bit32u t_scancode;
typedef Bit16u t_keysym;
typedef Bit16u t_shiftstate;

extern t_shiftstate shiftstate;

/* no, this has nothing to do with a press release ;-) */

#define PRESS 1
#define RELEASE 0

#define KEYB_QUEUE_LENGTH (255)

/* public function definitions */

void putrawkey(t_rawkeycode code);
void putkey(Boolean make, t_keysym scan, unsigned char ascii);
void set_shiftstate(t_shiftstate s);
void clear_bios_keybuf(void);
void append_pre_strokes(unsigned char *s);
int type_in_pre_strokes(void);

int keyb_server_init(void);
int keyb_server_reset(void);
void keyb_server_close(void);

#define presskey(key,charcode)  putkey(PRESS,key,charcode)
#define releasekey(key)         putkey(RELEASE,key,0)


#endif /* not __ASM__ */

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
#define ALT            (L_ALT|R_ALT)
#define SHIFT          (L_SHIFT|R_SHIFT)
#undef	CTRL
#define CTRL           (L_CTRL|R_CTRL)


/* Keysyms: Constants to be passed to put_key(). While these are equal
   to the hardware scancodes (except for PAUSE), this fact 
   should be ignored by clients.
   They are functionally equivalent to but different from X11 Keysyms. 

   Note that these stand for physical keys but use the name of the keys
   on US keyboards.

   Note also that SYSRQ and BREAK have their own codes even though they
   are not individual keys on PC keyboards.
 */


/* shift keys */

#define KEY_L_ALT	0x38		
#define KEY_R_ALT	0xe038
#define KEY_L_CTRL	0x1d		
#define KEY_R_CTRL	0xe01d
#define KEY_L_SHIFT	0x2a		
#define KEY_R_SHIFT	0x36	
#define KEY_NUM		0x45		
#define KEY_SCROLL	0x46		
#define KEY_CAPS	0x3a		

/* main block */

#define KEY_SPACE	0x39	
#define KEY_BKSP	0x0e
#define KEY_RETURN	0x1c		
#define KEY_TAB		0x0f
#define KEY_A		0x1e
#define KEY_B		0x30
#define KEY_C		0x2e
#define KEY_D		0x20
#define KEY_E		0x12
#define KEY_F		0x21
#define KEY_G		0x22
#define KEY_H		0x23
#define KEY_I		0x17
#define KEY_J		0x24
#define KEY_K		0x25
#define KEY_L		0x26
#define KEY_M		0x32
#define KEY_N		0x31
#define KEY_O		0x18
#define KEY_P		0x19
#define KEY_Q		0x10
#define KEY_R		0x13
#define KEY_S		0x1f
#define KEY_T		0x14
#define KEY_U		0x16
#define KEY_V		0x2f
#define KEY_W		0x11
#define KEY_X		0x2d
#define KEY_Y		0x15
#define KEY_Z		0x2c
#define KEY_1		0x02
#define KEY_2		0x03
#define KEY_3		0x04
#define KEY_4		0x05
#define KEY_5		0x06
#define KEY_6		0x07
#define KEY_7		0x08
#define KEY_8		0x09
#define KEY_9		0x0a
#define KEY_0		0x0b
#define KEY_DASH	0x0c
#define KEY_EQUALS	0x0d
#define KEY_LBRACK	0x1a
#define KEY_RBRACK	0x1b
#define KEY_SEMICOLON	0x27
#define KEY_APOSTROPHE 0x28   /* ' */
#define KEY_GRAVE      0x29   /* ` */
#define KEY_BACKSLASH	0x2b
#define KEY_COMMA	0x33
#define KEY_PERIOD	0x34
#define KEY_SLASH	0x35
#define KEY_LESSGREATER 0x56   /* not on US keyboards */

/* keypad */

#define KEY_PAD_0	0x52		
#define KEY_PAD_1	0x4f	
#define KEY_PAD_2	0x50
#define KEY_PAD_3	0x51
#define KEY_PAD_4	0x4b	
#define KEY_PAD_5	0x4c
#define KEY_PAD_6	0x4d
#define KEY_PAD_7	0x47
#define KEY_PAD_8	0x48
#define KEY_PAD_9	0x49
#define KEY_PAD_DECIMAL	0x53
#define KEY_PAD_SLASH	0xe035
#define KEY_PAD_AST	0x37
#define KEY_PAD_MINUS	0x4a
#define KEY_PAD_PLUS	0x4e
#define KEY_PAD_ENTER	0xe01c


/* function keys */

#define KEY_ESC		0x01		
#define KEY_F1		0x3b
#define KEY_F2		0x3c
#define KEY_F3		0x3d
#define KEY_F4		0x3e
#define KEY_F5		0x3f
#define KEY_F6		0x40
#define KEY_F7		0x41
#define KEY_F8		0x42
#define KEY_F9		0x43
#define KEY_F10		0x44
#define KEY_F11		0x57		
#define KEY_F12		0x58		

/* cursor block */

#define KEY_INS		0xe052
#define KEY_DEL		0xe053
#define KEY_HOME	0xe047
#define KEY_END		0xe04f
#define KEY_PGUP	0xe049
#define KEY_PGDN	0xe051
#define KEY_UP		0xe048
#define KEY_DOWN	0xe050
#define KEY_LEFT	0xe04b
#define KEY_RIGHT	0xe04d

/* these are "functions", rather than physical keys. So e.g. for 
   Ctrl-PrtScr send KEY_SYSRQ!
 */
#define KEY_PRTSCR	0xe037
#define KEY_PAUSE	0xff02              /* "symbolic" scancode */
#define KEY_SYSRQ	0x54	
#define KEY_BREAK	0xe046

#endif /* not _KEYBOARD_H */
