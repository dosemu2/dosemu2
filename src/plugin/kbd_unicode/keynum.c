#include "keynum.h"

t_keynum validate_keynum(t_keynum key)
{
	switch(key) {
		/* shift keys */
	case NUM_L_ALT:
	case NUM_R_ALT:
	case NUM_L_CTRL:
	case NUM_R_CTRL:
	case NUM_L_SHIFT:
	case NUM_R_SHIFT:
	case NUM_NUM:
	case NUM_SCROLL:
	case NUM_CAPS:

	/* main block */

	case NUM_SPACE:
	case NUM_BKSP:
	case NUM_RETURN:
	case NUM_TAB:
	case NUM_A:
	case NUM_B:
	case NUM_C:
	case NUM_D:
	case NUM_E:
	case NUM_F:
	case NUM_G:
	case NUM_H:
	case NUM_I:
	case NUM_J:
	case NUM_K:
	case NUM_L:
	case NUM_M:
	case NUM_N:
	case NUM_O:
	case NUM_P:
	case NUM_Q:
	case NUM_R:
	case NUM_S:
	case NUM_T:
	case NUM_U:
	case NUM_V:
	case NUM_W:
	case NUM_X:
	case NUM_Y:
	case NUM_Z:
	case NUM_1:
	case NUM_2:
	case NUM_3:
	case NUM_4:
	case NUM_5:
	case NUM_6:
	case NUM_7:
	case NUM_8:
	case NUM_9:
	case NUM_0:
	case NUM_DASH:
	case NUM_EQUALS:
	case NUM_LBRACK:
	case NUM_RBRACK:
	case NUM_SEMICOLON:
	case NUM_APOSTROPHE:	 /* ' */
	case NUM_GRAVE:		 /* ` */
	case NUM_BACKSLASH:
	case NUM_COMMA:
	case NUM_PERIOD:
	case NUM_SLASH:
	case NUM_LESSGREATER:	 /* not on US keyboards */

	/* keypad */

	case NUM_PAD_0:
	case NUM_PAD_1:
	case NUM_PAD_2:
	case NUM_PAD_3:
	case NUM_PAD_4:
	case NUM_PAD_5:
	case NUM_PAD_6:
	case NUM_PAD_7:
	case NUM_PAD_8:
	case NUM_PAD_9:
	case NUM_PAD_DECIMAL:
	case NUM_PAD_SLASH:
	case NUM_PAD_AST:
	case NUM_PAD_MINUS:
	case NUM_PAD_PLUS:
	case NUM_PAD_ENTER:


	/* function keys */

	case NUM_ESC:	 
	case NUM_F1:	 
	case NUM_F2:	 
	case NUM_F3:	 
	case NUM_F4:	 
	case NUM_F5:	 
	case NUM_F6:	 
	case NUM_F7:	 
	case NUM_F8:	 
	case NUM_F9:	 
	case NUM_F10:	 
	case NUM_F11:	 
	case NUM_F12:	 

	/* cursor block */

	case NUM_INS:
	case NUM_DEL:
	case NUM_HOME:
	case NUM_END:
	case NUM_PGUP:
	case NUM_PGDN:
	case NUM_UP:
	case NUM_DOWN:
	case NUM_LEFT:
	case NUM_RIGHT:

	/* New keys on a Microsoft Windows keyboard */
	case NUM_LWIN:
	case NUM_RWIN:
	case NUM_MENU:

	/* Dual scancode keys */
	case NUM_PRTSCR_SYSRQ:
	case NUM_PAUSE_BREAK:

	/* These are logical rather than physical keys:
	 * The scancodes for the keys exist but the keys really don't
	 */
	case NUM_FAKE_L_SHIFT:
	case NUM_FAKE_R_SHIFT:
	case NUM_PRTSCR:
	case NUM_SYSRQ:
	case NUM_PAUSE:
	case NUM_BREAK:

		break;
	default:
		key = NUM_VOID;
		break;
	}
	return key;
}

