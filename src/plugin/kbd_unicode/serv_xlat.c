/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 
 * DANG_BEGIN_MODULE
 * 
 * Description: Keyboard translation
 * 
 * Maintainer: Eric Biederman <ebiederm@xmission.com>
 * 
 * REMARK
 * This module contains the the translation part of the keyboard 'server',
 * which translates key events into the form in which they can be sent to DOS.
 *
 * The frontends will call one of the following functions to send
 * keyboard events to DOS:
 *
 * VERB
 *     put_rawkey(t_rawkeycode code);
 *     move_key(Boolean make, t_keysym key)
 *     put_symbol(Boolean make, t_keysym sym)
 *
 *     set_shiftstate(t_shiftstate s);
 * /VERB
 *
 * Interface to serv_backend.c is through write_queue(raw).
 *
 * More information about this module is in doc/README.newkbd
 *
 * /REMARK
 * DANG_END_MODULE
 *
 */

 /* Note:
  *  While it is very bad to hard code keys, dos has already
  *  hard coded the shift keys, the number pad keys, and
  *  virtually everthing but the letter keys.  Dosemu cannot
  *  remove this hard coding without confusing dos programs.
  *  The point of dosemu's configurability is primarily to
  *  allow non-us keyboards to work as they would in dos.  Not
  *  to make a nice place to customize your keyboard. 
  *
  * The keys currently hard coded are:
  * NUM_TAB
  * NUM_RETURN
  * NUM_BKSP
  * NUM_ESC
  * NUM_PAD_ENTER
  * NUM_PAD_SLASH
  * NUM_SPACE
  * NUM_LBRACK		-- fixme 
  * NUM_BACKSLASH	-- fixme 
  * NUM_RBRACK		-- fixme 
  * NUM_6		-- fixme?
  * NUM_DASH		-- fixme
  * NUM_FAKE_L_SHIFT
  * NUM_FAKE_R_SHIFT
  * NUM_L_SHIFT
  * NUM_R_SHIFT
  * NUM_L_CTRL
  * NUM_R_CTRL
  * NUM_L_ALT
  * NUM_R_ALT
  * NUM_CAPS
  * NUM_NUM
  * NUM_SCROLL
  * NUM_PTRSCR_SYSRQ
  * NUM_PAUSE_BREAK
  * NUM_PAUSE
  * NUM_BREAK
  * NUM_PRTSCR
  * NUM_SYSRQ
  * NUM_PAD_7	NUM_PAD_8	NUM_PAD_9
  * NUM_PAD_4	NUM_PAD_5	NUM_PAD_6
  * NUM_PAD_1	NUM_PAD_2	NUM_PAD_3
  * NUM_PAD_0
  * NUM_PAD_MINUS	-- fixme?
  * NUM_PAD_PLUS	-- fixme?
  * NUM_PAD_DECIMAL	-- fixme?
  * NUM_INS	NUM_DEL
  * NUM_HOME	NUM_END
  * NUM_PGUP	NUM_PGDN
  * NUM_UP	NUM_DOWN
  * NUM_LEFT	NUM_RIGHT
  * NUM_TAB		--fixme?
  * NUM_F1
  * NUM_F2
  * NUM_F3
  * NUM_F4
  * NUM_F5
  * NUM_F6
  * NUM_F7
  * NUM_F8
  * NUM_F9
  * NUM_F10
  * NUM_F11
  * NUM_F12
  * 
  */

#define __KBD_SERVER

#include <ctype.h>
#include <string.h>

#include "config.h"
#include "emu.h"
#include "bitops.h"

#include "keymaps.h"
#include "keyboard.h"
#include "keyb_server.h"

#include "keynum.h"
#include "keystate.h"
#include "keysym_attributes.h"

#include "translate.h"

#include "video.h" /* for charset defines */

#if NUM_KEYSYMS != 0x10000
#error NUM_KEYSYMS needs to be 0x10000
#endif

static void sync_shift_state(t_modifiers desired, struct keyboard_state *state);
static t_shiftstate translate_shiftstate(t_shiftstate cur_shiftstate,
	struct translate_rule *rule, t_keynum key, t_shiftstate *mask);

/*
 * various tables
 */

/* 
 * BIOS scancode tables
 *
 * The key num is used as the index
 * For 'unshifted', no translation is done (bios scancode=hardware scancode),
 * for SHIFT the translation is done 'by hand' in make_bios_code().
 */


static const Bit8u bios_ctrl_scancodes[NUM_KEY_NUMS] =
{
   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,    /* 00-07 */
   0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x94,    /* 08-0F */
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,    /* 10-17 */
   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x00, 0x1e, 0x1f,    /* 18-1F */
   0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,    /* 20-27 */
   0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,    /* 28-2F */
   0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x96,    /* 30-37 */
   0x38, 0x39, 0x3a, 0x5e, 0x5f, 0x60, 0x61, 0x62,    /* 38-3F */
   0x63, 0x64, 0x65, 0x66, 0x67, 0x45, 0x44, 0x77,    /* 40-47 */
   0x8d, 0x84, 0x8e, 0x73, 0x8f, 0x74, 0x90, 0x75,    /* 48-4F */
   0x91, 0x76, 0x92, 0x93, 0x54, 0x55, 0x56, 0x89,    /* 50-57 */
   0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 58-5F */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 60-67 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 68-6F */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 70-77 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 78-7F */

   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 80-87 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 88-8F */ 
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 90-97 */
   0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00,    /* 98-9F */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* A0-A7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* A8-AF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x95, 0x00, 0x00,    /* B0-B7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* B8-BF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77,    /* C0-C7 */
   0x8d, 0x84, 0x00, 0x73, 0x00, 0x74, 0x00, 0x75,    /* C8-CF */
   0x91, 0x76, 0x92, 0x93, 0x00, 0x00, 0x00, 0x00,    /* D0-D7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* D8-DF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* E0-E7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* E8-EF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* F0-F7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00,    /* F8-FF */
};

static const Bit8u bios_alt_scancodes[NUM_KEY_NUMS] =
{
   0x00, 0x01, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d,    /* 00-07 */
   0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x0e, 0xa5,    /* 08-0F */
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,    /* 10-17 */
   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x00, 0x1e, 0x1f,    /* 18-1F */
   0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,    /* 20-27 */
   0x28, 0x29, 0x00, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,    /* 28-2F */
   0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x00, 0x37,    /* 30-37 */
   0x00, 0x39, 0x00, 0x68, 0x69, 0x6a, 0x6b, 0x6c,    /* 38-3F */
   0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x00, 0x00, 0x97,    /* 40-47 */
   0x98, 0x99, 0x4a, 0x9b, 0x00, 0x9d, 0x4e, 0x9f,    /* 48-4F */
   0xa0, 0xa1, 0xa2, 0x00, 0x00, 0x00, 0x00, 0x8b,    /* 50-57 */
   0x8c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 58-5F */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 60-67 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 68-6F */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 70-77 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 78-7F */

   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 80-87 */
   0x00, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 88-8F */ 
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 90-97 */
   0x00, 0x00, 0x00, 0x00, 0xa6, 0x00, 0x00, 0x00,    /* 98-9F */ 
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* A0-A7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* A8-AF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0xa4, 0x00, 0x00,    /* B0-B7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* B8-BF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97,    /* C0-C7 */
   0x98, 0x99, 0x00, 0x9b, 0x00, 0x9d, 0x00, 0x9f,    /* C8-CF */
   0xa0, 0xa1, 0xa2, 0xa3, 0x00, 0x00, 0x00, 0x00,    /* D0-D7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* D8-DF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* E0-E7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* E8-EF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* F0-F7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* F8-FF */
};

static const Bit8u bios_alt_mapped_scancodes[] = 
{
   0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23,    /* A-H */
   0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19,    /* I-P */
   0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D,    /* Q-W */
   0x15, 0x2C                                         /* Y-Z */
};

/******************************************************************************************
 * State initialization
 ******************************************************************************************/

/*
 *  Scancode to Character translation
 * =============================================================================
 */

static void init_misc_all_maps(t_keysym *rule)
{
	rule[NUM_L_ALT] = KEY_L_ALT;
	rule[NUM_R_ALT] = KEY_R_ALT;
	rule[NUM_L_CTRL] = KEY_L_CTRL;
	rule[NUM_R_CTRL] = KEY_R_CTRL;
	rule[NUM_L_SHIFT] = KEY_L_SHIFT;
	rule[NUM_R_SHIFT] = KEY_R_SHIFT;
	rule[NUM_NUM] = KEY_NUM;
	rule[NUM_CAPS] = KEY_CAPS;
}

static void init_misc_plain_map(t_keysym *rule)
{
	rule[NUM_TAB] = KEY_TAB;
	rule[NUM_RETURN] = KEY_RETURN;
	rule[NUM_BKSP] = KEY_BKSP;
	rule[NUM_ESC] = KEY_ESC;
	rule[NUM_PAD_ENTER] = KEY_PAD_ENTER;
	rule[NUM_PAD_SLASH] = KEY_PAD_SLASH;
	rule[NUM_SCROLL] = KEY_SCROLL;

	rule[NUM_PAD_0] = KEY_PAD_INS;
	rule[NUM_PAD_1] = KEY_PAD_END;
	rule[NUM_PAD_2] = KEY_PAD_DOWN;
	rule[NUM_PAD_3] = KEY_PAD_PGDN;
	rule[NUM_PAD_4] = KEY_PAD_LEFT;
	rule[NUM_PAD_5] = KEY_PAD_CENTER;
	rule[NUM_PAD_6] = KEY_PAD_RIGHT;
	rule[NUM_PAD_7] = KEY_PAD_HOME;
	rule[NUM_PAD_8] = KEY_PAD_UP;
	rule[NUM_PAD_9] = KEY_PAD_PGUP;
	rule[NUM_PAD_DECIMAL] = KEY_PAD_DEL;
	rule[NUM_PAD_SLASH] = KEY_PAD_SLASH;
	rule[NUM_PAD_AST] = KEY_PAD_AST;
	rule[NUM_PAD_MINUS] = KEY_PAD_MINUS;
	rule[NUM_PAD_PLUS] = KEY_PAD_PLUS;
	rule[NUM_PAD_ENTER] = KEY_PAD_ENTER;

	rule[NUM_F1] = KEY_F1;
	rule[NUM_F2] = KEY_F2;
	rule[NUM_F3] = KEY_F3;
	rule[NUM_F4] = KEY_F4;
	rule[NUM_F5] = KEY_F5;
	rule[NUM_F6] = KEY_F6;
	rule[NUM_F7] = KEY_F7;
	rule[NUM_F8] = KEY_F8;
	rule[NUM_F9] = KEY_F9;
	rule[NUM_F10] = KEY_F10;
	rule[NUM_F11] = KEY_F11;
	rule[NUM_F12] = KEY_F12;

	rule[NUM_INS] = KEY_INS;
	rule[NUM_DEL] = KEY_DEL;
	rule[NUM_HOME] = KEY_HOME;
	rule[NUM_END] = KEY_END;
	rule[NUM_PGUP] = KEY_PGUP;
	rule[NUM_PGDN] = KEY_PGDN;
	rule[NUM_UP] = KEY_UP;
	rule[NUM_DOWN] = KEY_DOWN;
	rule[NUM_LEFT] = KEY_LEFT;
	rule[NUM_RIGHT] = KEY_RIGHT;

	rule[NUM_PRTSCR_SYSRQ] = KEY_PRTSCR;
	rule[NUM_PAUSE_BREAK] = KEY_PAUSE;
}

static void init_misc_shifted_map(t_keysym *rule)
{
	rule[NUM_TAB] = KEY_LEFT_TAB;
	rule[NUM_RETURN] = KEY_RETURN;
	rule[NUM_BKSP] = KEY_BKSP;
	rule[NUM_ESC] = KEY_ESC;
	rule[NUM_PAD_ENTER] = KEY_PAD_ENTER;
	rule[NUM_PAD_SLASH] = KEY_PAD_SLASH;
}

static void init_misc_keypad_map(t_keysym *rule)
{
	int i;
	/* replace generic characters with their keysym variety */
	for(i = 0x47; i <= 0x53; i++ ) {
		t_keysym keysym = rule[i];
		switch(keysym) {
		case U_DIGIT_ZERO:	keysym = KEY_PAD_0; break;
		case U_DIGIT_ONE:	keysym = KEY_PAD_1; break;
		case U_DIGIT_TWO:	keysym = KEY_PAD_2; break;
		case U_DIGIT_THREE:	keysym = KEY_PAD_3; break;
		case U_DIGIT_FOUR:	keysym = KEY_PAD_4; break;
		case U_DIGIT_FIVE:	keysym = KEY_PAD_5; break;
		case U_DIGIT_SIX:	keysym = KEY_PAD_6; break;
		case U_DIGIT_SEVEN:	keysym = KEY_PAD_7; break;
		case U_DIGIT_EIGHT:	keysym = KEY_PAD_8; break;
		case U_DIGIT_NINE:	keysym = KEY_PAD_9; break;
		case U_PERIOD:		keysym = KEY_PAD_DECIMAL; break;
		case U_COMMA:		keysym = KEY_PAD_SEPARATOR; break;
		case U_SLASH:		keysym = KEY_PAD_SLASH; break;
		case U_PLUS_SIGN:	keysym = KEY_PAD_PLUS; break;
		case U_ASTERISK:	keysym = KEY_PAD_AST; break;
		case U_HYPHEN_MINUS:	keysym = KEY_PAD_MINUS; break;
		}
		rule[i] = keysym;
	}
}
static void init_heuristics_alt_map(t_keysym *rule, t_keysym *plain_rule)
{
	int i, j;

	/* Heuristic compute the alt code based on the 
	 * values on the other keycaps.
	 */
	for(i = 1; i < 27; i++) {
		for(j = 0; j < NUM_KEY_NUMS; j++) {
			/* lower case letters */
			if (plain_rule[j] == (i + 0x60)) {
				rule[j] = i + KEY_ALT_A -1;
			}
			/* upper case letters */
			if (plain_rule[j] == (i + 0x40)) {
				rule[j] = i + KEY_ALT_A -1;
			}
		}
	}
}
static void init_misc_alt_map(t_keysym *rule)
{
	rule[NUM_SPACE] = KEY_SPACE;
	rule[NUM_PRTSCR_SYSRQ] = KEY_SYSRQ;
}
static void init_misc_altgr_map(t_keysym *rule)
{
	rule[NUM_SPACE] = KEY_SPACE;
}
static void init_misc_shift_altgr_map(t_keysym *rule)
{
	rule[NUM_SPACE] = KEY_SPACE;
}
static void init_misc_ctrl_alt_map(t_keysym *rule)
{
	rule[NUM_PGUP] = KEY_DOSEMU_X86EMU_DEBUG;
	rule[NUM_DEL] = KEY_DOSEMU_REBOOT;

	rule[NUM_PGDN] = KEY_DOSEMU_EXIT;
	rule[NUM_PAD_3] = KEY_DOSEMU_EXIT;
	rule[NUM_P] = KEY_DOSEMU_FREEZE;

	rule[NUM_F1] = KEY_DOSEMU_VT_1;
	rule[NUM_F2] = KEY_DOSEMU_VT_2;
	rule[NUM_F3] = KEY_DOSEMU_VT_3;
	rule[NUM_F4] = KEY_DOSEMU_VT_4;
	rule[NUM_F5] = KEY_DOSEMU_VT_5;
	rule[NUM_F6] = KEY_DOSEMU_VT_6;
	rule[NUM_F7] = KEY_DOSEMU_VT_7;
	rule[NUM_F8] = KEY_DOSEMU_VT_8;
	rule[NUM_F9] = KEY_DOSEMU_VT_9;
	rule[NUM_F10] = KEY_DOSEMU_VT_10;
	rule[NUM_F11] = KEY_DOSEMU_VT_11;
	rule[NUM_F12] = KEY_DOSEMU_VT_12;

	rule[NUM_PAD_8] = KEY_MOUSE_UP;
	rule[NUM_PAD_2] = KEY_MOUSE_DOWN;
	rule[NUM_PAD_4] = KEY_MOUSE_LEFT;
	rule[NUM_PAD_6] = KEY_MOUSE_RIGHT;
	rule[NUM_PAD_7] = KEY_MOUSE_UP_AND_LEFT;
	rule[NUM_PAD_9] = KEY_MOUSE_UP_AND_RIGHT;
	rule[NUM_PAD_1] = KEY_MOUSE_DOWN_AND_LEFT;
#if 0
	rule[NUM_PAD_3] = KEY_MOUSE_DOWN_AND_RIGHT;
#endif
}

static void init_heuristics_ctrl_map(t_keysym *rule, t_keysym *plain_rule)
{
	int i, j;

	/* Heuristic compute the ctrl code based on the 
	 * values on the other keycaps.
	 */
	for(i = 1; i < 27; i++) {
		for(j = 0; j < NUM_KEY_NUMS; j++) {
			/* lower case letters */
			if (plain_rule[j] == (i + 0x60)) {
				rule[j] = i;
				break;
			}
			/* upper case letters */
			if (plain_rule[j] == (i + 0x40)) {
				rule[j] = i;
				break;
			}
		}
	}
	/* funky heuristics based on keycaps don't do as well for the
	 * these as just hardcoding the positions to those used on the
	 * us keyboard.
	 */
	rule[NUM_2] = 0x0;
	rule[NUM_LBRACK]    = 0x1b;
	rule[NUM_BACKSLASH] = 0x1c;
	rule[NUM_RBRACK]    = 0x1d;
	rule[NUM_6]         = 0x1e;
	rule[NUM_DASH]      = 0x1f;
}

static void init_misc_ctrl_map(t_keysym *rule)
{
	rule[NUM_RETURN]    = 0x0a;
	rule[NUM_PAD_ENTER] = 0x0a;
	rule[NUM_SPACE]     = 0x20;
	rule[NUM_ESC]       = 0x1b;
	rule[NUM_BKSP]      = 0x7f;
	rule[NUM_PAUSE_BREAK] = KEY_BREAK;
}

static void init_translate_rule(t_keysym *rule, 
				int map_size, t_keysym *key_map,
				int key_bias)
{
	t_unicode ch;
	int i;
	struct char_set *keyb_charset = trconfig.keyb_config_charset;

	for( i = 0; i < map_size; i++) {
		ch = key_map[i];
		if (ch == KEY_VOID) 
			continue;
		/* 0xef00 - 0xefff is a pass through range to the current
		 * character set.
		 */
		if ((ch >= 0xef00) && (ch <= 0xefff)) {
			unsigned char buff[1];
			size_t result;
			struct char_set_state keyb_state;
			init_charset_state(&keyb_state, keyb_charset);
			buff[0] = ch & 0xFF;
			result = charset_to_unicode(&keyb_state, &ch, buff, 1);
			cleanup_charset_state(&keyb_state);
			if (ch == KEY_VOID)
				continue;
		}
		rule[i + key_bias] = ch;
	}
}

static void dump_translate_rules(struct scancode_translate_rules *rules)
{
	int i;
#define LOOP(type)  \
	do { \
		k_printf(#type ":\n"); \
		for(i = 0; i < NUM_KEY_NUMS; i++) { \
			t_keysym keysym = type[i]; \
			t_keynum keynum = validate_keynum(i); \
			if (keysym == KEY_VOID || keynum == NUM_VOID) \
				continue; \
			k_printf("keynum->keysym: %02x->%04x\n", \
				 keynum, keysym); \
		} \
	} while(0)

	for(i = 0; i < NUM_RULES; i++) {
		LOOP(rules->trans_rules.rule_arr[i].rule_map);
	}
}

/* FIXME: I need external input for ctrl mappings!
 * I have a clever hack that get's it right most of the time,
 * but it's not quite right.  Also some ctrl keys are hardwired to
 * their position on a US keyboard.
 */

static void 
init_scancode_translation_rules(struct scancode_translate_rules *maps,
			      struct keytable_entry *key_table)
{
	struct scancode_translate_rules *rules=NULL;
	int i, j;
	
	for(i = 0; i < MAPS_MAX; i++) {
		if(maps[i].keyboard==KEYB_NO) {
			rules = &maps[i];
			break;
		}
	}
	if(rules==NULL) {
		k_printf("init: maximum keymaps limit exceeded\n");
		return;
	}

	rules->keyboard = key_table->keyboard;

	/* Initialize everything to a known value */
	rules->trans_rules.rule_structs.plain.modifiers = 0;
	rules->trans_rules.rule_structs.shift.modifiers = MODIFIER_SHIFT;
	rules->trans_rules.rule_structs.ctrl.modifiers = MODIFIER_CTRL;
	rules->trans_rules.rule_structs.alt.modifiers = MODIFIER_ALT;
	rules->trans_rules.rule_structs.altgr.modifiers = MODIFIER_ALTGR;
	rules->trans_rules.rule_structs.shift_altgr.modifiers = MODIFIER_SHIFT | MODIFIER_ALTGR;
	rules->trans_rules.rule_structs.ctrl_alt.modifiers = MODIFIER_CTRL | MODIFIER_ALT;

	for(j = 0; j < NUM_RULES; j++) {
		for(i = 0; i < NUM_KEY_NUMS; i++)
			rules->trans_rules.rule_arr[j].rule_map[i] = KEY_VOID;
	}

	/* plain keys */
	init_translate_rule(rules->trans_rules.rule_structs.plain.rule_map,
			    key_table->sizemap, key_table->key_map, 0);
	init_misc_plain_map(rules->trans_rules.rule_structs.plain.rule_map);

	/* shifted keys */
	init_translate_rule(rules->trans_rules.rule_structs.shift.rule_map,
			     key_table->sizemap, key_table->shift_map, 0);
	init_misc_shifted_map(rules->trans_rules.rule_structs.shift.rule_map);

	/* ctrl keys */
	if (key_table->ctrl_map) {
		init_translate_rule(rules->trans_rules.rule_structs.ctrl.rule_map,
				     key_table->sizemap, key_table->ctrl_map, 0);
	} else {
		/* FIXME!!! (unreliable heuristic used) */
		init_heuristics_ctrl_map(rules->trans_rules.rule_structs.ctrl.rule_map,
				    rules->trans_rules.rule_structs.plain.rule_map);
	}
	init_misc_ctrl_map(rules->trans_rules.rule_structs.ctrl.rule_map);

	/* alt keys */
	init_heuristics_alt_map(rules->trans_rules.rule_structs.alt.rule_map,
		rules->trans_rules.rule_structs.plain.rule_map); /* FIXME (heuristic used) */
	init_misc_alt_map(rules->trans_rules.rule_structs.alt.rule_map); 

	/* altgr keys */
	if (key_table->alt_map) {
		init_translate_rule(rules->trans_rules.rule_structs.altgr.rule_map,
				     key_table->sizemap, key_table->alt_map, 0);
	} 
	init_misc_altgr_map(rules->trans_rules.rule_structs.altgr.rule_map);

	/* shift alt keys */
	if (key_table->shift_alt_map) {
		init_translate_rule(rules->trans_rules.rule_structs.shift_altgr.rule_map,
				     key_table->sizemap, key_table->shift_alt_map, 0);
	}
	init_misc_shift_altgr_map(rules->trans_rules.rule_structs.shift_altgr.rule_map);

	/* ctrl alt keys */
	if (key_table->ctrl_alt_map) {
		init_translate_rule(rules->trans_rules.rule_structs.ctrl_alt.rule_map,
				     key_table->sizemap, key_table->ctrl_alt_map, 0);
	}
	init_misc_ctrl_alt_map(rules->trans_rules.rule_structs.ctrl_alt.rule_map);

	/* shift num_pad key is the same as num_lock num_pad key */
	/* keypad */
	init_translate_rule(rules->trans_rules.rule_structs.shift.rule_map,
			     key_table->sizepad, key_table->num_table, 0x47);
	init_misc_keypad_map(rules->trans_rules.rule_structs.shift.rule_map);

	for(i = 0; i < NUM_RULES; i++)
		init_misc_all_maps(rules->trans_rules.rule_arr[i].rule_map);

	dump_translate_rules(rules);
}

/*
 *  Character to Scancode sequence translation
 * =============================================================================
 */

static void init_charset_keymap(struct character_translate_rules *charset,
				struct scancode_translate_rules *rules, int mapnum)
{
	t_keynum key;
	int i, j, k;
	t_keysym ch;
	t_modifiers shiftstate;
	t_shiftstate mask;
	struct translate_rule *rule, *rule1;

	for(j = 0; j < NUM_RULES; j++) {
	    rule = &rules->trans_rules.rule_arr[j];
	    for (i = 0; i < NUM_KEY_NUMS; i++) {
		ch = rule->rule_map[i];
		shiftstate = rule->modifiers;
		if (ch == KEY_VOID) {
			continue;
		}
		key = validate_keynum(i);
		if (key == NUM_VOID) {
			continue;
		}
		if (charset->keys[ch].key != NUM_VOID) {
			continue;
		}
		charset->keys[ch].key = key;
		charset->keys[ch].shiftstate = shiftstate;
		charset->keys[ch].map = mapnum;
		charset->keys[ch].shiftstate_mask = ~0;
		for(k = 0; k < NUM_RULES; k++) {
			rule1 = &rules->trans_rules.rule_arr[k];
			if (rule1->rule_map[i] != ch) {
				charset->keys[ch].shiftstate_mask &= ~rule1->modifiers;
			}
			if (~get_modifiers_r(translate_shiftstate(
			    ~0, &rules->trans_rules.rule_structs.plain, key, &mask)) &
			    ~charset->keys[ch].shiftstate_mask) {
				charset->keys[ch].shiftstate_mask &= ~get_modifiers_r(mask);
			}
		}
	    }
	}
}

static void init_one_deadkey(void *p, t_keysym dead_sym, t_keysym in, t_keysym out)
{
	struct character_translate_rules *charset = p;
	
	if (charset->keys[dead_sym].key == NUM_VOID) {
		/* If we can't type the dead key we can't use it
		 * to type anything else either.
		 */
		return;
	}
	if ((charset->keys[out].key != NUM_VOID) ||
		(charset->keys[in].key == NUM_VOID) 
				/* Only one dead key can be active at a time 
				 * but it may be needed to press one dead key
				 * to get another (a truly silly case)
				 */
		) {
		return;
	}
	/* Note: The shiftstate is the same going for the
	 * deadsym and for the result symbol
	 */
	charset->keys[out].key = 
		charset->keys[in].key;
	charset->keys[out].shiftstate =
		charset->keys[in].shiftstate;
	charset->keys[out].deadsym = dead_sym;
}

static void init_charset_deadmap(struct character_translate_rules *charset)
{
	traverse_dead_key_list(charset, init_one_deadkey);
}

static void check_video_mem_charset(struct character_translate_rules *charset)
{
	int i;
	struct char_set *vmem_charset = trconfig.video_mem_charset;

	/* any mapping from < 0x20 to Unicode >= 0x20 ? */
	for (i = 0x20; i < NUM_KEYSYMS; i++) {
		unsigned char buff[1];
		size_t result;
		struct char_set_state vmem_state;
		init_charset_state(&vmem_state, vmem_charset);
		result = unicode_to_charset(&vmem_state, i, buff, 1);
		if (result == 1 && buff[0] < 0x20 &&
		    charset->keys[buff[0]].key != NUM_VOID) {
			charset->keys[i] = charset->keys[buff[0]];
		}
		cleanup_charset_state(&vmem_state);
	}
}

static void init_one_approximation(void *p, t_unicode symbol, t_unicode approximation)
{
	struct character_translate_rules *charset = p;
	
	if ((symbol >= NUM_KEYSYMS) || (approximation >= NUM_KEYSYMS)) 
		return;

	if ((charset->keys[symbol].key == NUM_VOID) &&
		(charset->keys[approximation].key != NUM_VOID) &&
		(charset->keys[approximation].character == charset->keys[symbol].character)) {
		/* Copy the code from the approximate symbol to the symbol */
		charset->keys[symbol] = charset->keys[approximation];
	}
}
static void init_charset_approximations(struct character_translate_rules *charset)
{
	traverse_approximation_list(charset, init_one_approximation);
}

static void dump_charset(struct character_translate_rules *charset)
{
	int i;
	for(i = 0; i < NUM_KEYSYMS; i++) {
		if (charset->keys[i].key == NUM_VOID)
			continue;
		k_printf("sym: %04x key: %02x shiftstate: %04x shiftstate_mask: %04x deadsym: %04x character: %02x -> '%c'\n",
			 i,
			 charset->keys[i].key,
			 charset->keys[i].shiftstate,
			 charset->keys[i].shiftstate_mask,
			 charset->keys[i].deadsym,
			 charset->keys[i].character,
			 charset->keys[i].character);
	}
}

static void init_charset_keys(struct character_translate_rules *charset, 
		       struct scancode_translate_rules maps[])
{
	int i;
	struct char_set *keyb_charset = trconfig.keyb_config_charset;

	/* first initialize the charset_keys to nothing */
	for (i = 0; i < NUM_KEYSYMS; i++) {
		unsigned char buff[1];
		size_t result;
		struct char_set_state keyb_state;
		init_charset_state(&keyb_state, keyb_charset);
		/* FIXME: handle smaller tables ?*/
		result = unicode_to_charset(&keyb_state, i, buff, 1);
		charset->keys[i].key = NUM_VOID;
		charset->keys[i].shiftstate = 0;
		charset->keys[i].deadsym = KEY_VOID;
		charset->keys[i].character = buff[0];
		cleanup_charset_state(&keyb_state);
	}

	/* unmapped characters have an ascii key of 0 */
	charset->keys[KEY_VOID].character = 0;

	for(i=0;i<MAPS_MAX;i++) {
	/* Now find sequences of keypresses that generate them */
		init_charset_keymap(charset, &maps[i], i);
	}

	/* dead keys */
	init_charset_deadmap(charset);

	/* check for any keys between 0 and 31 that are mapped in the
	   video mem character set */
	check_video_mem_charset(charset);

	/* approximations for cut & paste */
	init_charset_approximations(charset);

	/* When debugging show the translations */
	dump_charset(charset);
}

/*
 * Active keyboard state
 * =============================================================================
 */
static void init_active_keyboard_state(
	struct keyboard_state *state, struct keyboard_rules *rules)
{
	memset(state, 0, sizeof(*state));
	state->rules = rules;
/* for now, we just assume that NumLock=off, Caps=off, Scroll=off. If the raw frontend
 * is running, it'll tell us the actual shiftstate later, otherwise we'll have to
 * live with this.
 */
	state->shiftstate = 0;
	state->alt_num_buffer = 0;
	state->accent = KEY_VOID;
	state->raw_state.rawprefix = 0;
}

static void init_rules(struct keyboard_rules *rules)
{
	int i;

	rules->activemap = 0;
	for(i = 0; i < MAPS_MAX; i++) {
		rules->maps[i].keyboard=KEYB_NO;
	}
}

/* 
 * State management
 * =============================================================================
 */ 
static struct keyboard_rules keyboard_rules;
struct keyboard_state input_keyboard_state;
struct keyboard_state dos_keyboard_state;

static void keyb_init_state(void)
{
	init_rules(&keyboard_rules);
	init_scancode_translation_rules(keyboard_rules.maps, config.keytable);
	if(config.altkeytable) {
		init_scancode_translation_rules(keyboard_rules.maps, config.altkeytable);
	}
	init_charset_keys(&keyboard_rules.charset, keyboard_rules.maps);
}
static void keyb_reset_state(void)
{
	init_active_keyboard_state(&input_keyboard_state, &keyboard_rules);
	init_active_keyboard_state(&dos_keyboard_state, &keyboard_rules);
}
/******************************************************************************************
 * Queue front end (keycode translation etc.)
 ******************************************************************************************/

/*
 * translate a keyboard event into a 16-bit BIOS scancode word (LO=ascii char, HI="scancode").
 * Always trust the lower level routines to generat the correct ascii
 * code itself, unless the ascii is a flag to mark the key as an extended key.
 */

static Bit16u make_bios_code_r(Boolean make, t_keynum key,
			       unsigned ascii, t_keysym keysym,
			       struct keyboard_state *state) 
{
	t_shiftstate shiftstate = state->shiftstate;
	Bit8u bios_scan = 0;
	Bit16u special = 0;

	/* Note:
	 *  While it is very bad to hard code keys, dos has already
	 *  hard coded the shift keys, the number pad keys, and
	 *  virtually everthing but the letter keys.  Dosemu cannot
	 *  remove this hard coding without confusing dos programs.
	 *  The point of dosemu's configurability is primarily to
	 *  allow non-us keyboards to work as they would in dos.  Not
	 *  to make a nice place to customize your keyboard. 
	 */

	if (make) {
		/* filter out hidden keys (shift keys & new keys) */
		switch(key) {
		default:
			/*k_printf("KBD: make_bios_code() hidden key\n");*/
			return 0;
			break;
			/* main block */
		case NUM_SPACE: case NUM_BKSP: case NUM_RETURN: case NUM_TAB: 
		case NUM_A: case NUM_B: case NUM_C: case NUM_D: case NUM_E: 
		case NUM_F: case NUM_G: case NUM_H: case NUM_I: case NUM_J: 
		case NUM_K: case NUM_L: case NUM_M: case NUM_N: case NUM_O: 
		case NUM_P: case NUM_Q: case NUM_R: case NUM_S: case NUM_T: 
		case NUM_U: case NUM_V: case NUM_W: case NUM_X: case NUM_Y: 
		case NUM_Z: case NUM_1: case NUM_2: case NUM_3: case NUM_4:
		case NUM_5: case NUM_6: case NUM_7: case NUM_8: case NUM_9: 
		case NUM_0: case NUM_DASH: case NUM_EQUALS: case NUM_LBRACK: 
		case NUM_RBRACK: case NUM_SEMICOLON: case NUM_APOSTROPHE:
		case NUM_GRAVE: case NUM_BACKSLASH: case NUM_COMMA: 
		case NUM_PERIOD: case NUM_SLASH: case NUM_LESSGREATER:

			/* keypad */
		case NUM_PAD_0: case NUM_PAD_1: case NUM_PAD_2: case NUM_PAD_3: 
		case NUM_PAD_4: case NUM_PAD_5: case NUM_PAD_6: case NUM_PAD_7:
		case NUM_PAD_8: case NUM_PAD_9: case NUM_PAD_DECIMAL:
		case NUM_PAD_SLASH: case NUM_PAD_AST: case NUM_PAD_MINUS: 
		case NUM_PAD_PLUS: case NUM_PAD_ENTER:

			/* function keys */
		case NUM_ESC: case NUM_F1: case NUM_F2: case NUM_F3: case NUM_F4:
		case NUM_F5: case NUM_F6: case NUM_F7: case NUM_F8: case NUM_F9:
		case NUM_F10: case NUM_F11: case NUM_F12:

			/* cursor block */
		case NUM_INS: case NUM_DEL: case NUM_HOME: case NUM_END: 
		case NUM_PGUP: case NUM_PGDN: case NUM_UP: case NUM_DOWN: 
		case NUM_LEFT: case NUM_RIGHT:

			/* special */
		case NUM_PAUSE: case NUM_BREAK: case NUM_PRTSCR: case NUM_SYSRQ:
			break;
		}
		/* check for special handling */
		switch(key) {
		case NUM_PAUSE:		special=SP_PAUSE; break;
		case NUM_BREAK:		special=SP_BREAK; break;
		case NUM_PRTSCR:	special=SP_PRTSCR; break;
		case NUM_SYSRQ:		special=SP_SYSRQ_MAKE; break;
		}

		/* convert to BIOS INT16 scancode, depending on shift state
		 */
		if (shiftstate & ANY_ALT) {
			switch(key) {
		   /* alt-keypad */
			case NUM_PAD_7:	case NUM_PAD_8:	case NUM_PAD_9:
			case NUM_PAD_4:	case NUM_PAD_5:	case NUM_PAD_6:
			case NUM_PAD_1:	case NUM_PAD_2:	case NUM_PAD_3:
			case NUM_PAD_0:
			{
				int val = 0;
				switch(key) {
				case NUM_PAD_9: val = 9; break;
				case NUM_PAD_8: val = 8; break;
				case NUM_PAD_7: val = 7; break;
				case NUM_PAD_6: val = 6; break;
				case NUM_PAD_5: val = 5; break;
				case NUM_PAD_4: val = 4; break;
				case NUM_PAD_3: val = 3; break;
				case NUM_PAD_2: val = 2; break;
				case NUM_PAD_1: val = 1; break;
				case NUM_PAD_0: val = 0; break;
				}
				state->alt_num_buffer = 
					(state->alt_num_buffer*10 + val) & 0xff;
				k_printf("KBD: alt-keypad key=%08d ascii=%c buffer=%d\n",
					 key, ascii, state->alt_num_buffer);
				ascii=0;
				bios_scan=0;
				break;
			}
			
			default:
				if ((keysym >= KEY_ALT_A)&&(keysym <= KEY_ALT_Z)) {
					bios_scan=bios_alt_mapped_scancodes[keysym - KEY_ALT_A];
				} else {
					bios_scan=bios_alt_scancodes[key];
				}
				if (ascii != 0) {
					/* This gets the scancodes right for AltGr keys */
					bios_scan = key & 0x7f;
				}
			}
		}
		else if (shiftstate & ANY_CTRL) {
			bios_scan = bios_ctrl_scancodes[key];
			
			if (key == NUM_PRTSCR) {
				special = 0;
			}
			/* Mark the extended keys */
			switch (key) {	
				/* The cursor block */
			case NUM_INS:	 case NUM_DEL:
			case NUM_HOME:	 case NUM_END:
			case NUM_PGUP:	 case NUM_PGDN:
			case NUM_UP:	 case NUM_DOWN:
			case NUM_LEFT:	 case NUM_RIGHT:
				ascii = 0xe0; break;
			}
			if ((bios_scan == key) && (keysym == KEY_VOID)) {
				ascii = 0;
				bios_scan = 0;
			}
		}
		else if (shiftstate & ANY_SHIFT) {
			switch(key) {
			case NUM_TAB:
				bios_scan=0x0f; break;
			case NUM_F1:
				bios_scan=0x54; break;
			case NUM_F2:
				bios_scan=0x55; break;
			case NUM_F3:
				bios_scan=0x56; break;
			case NUM_F4:
				bios_scan=0x57; break;
			case NUM_F5:
				bios_scan=0x58; break;
			case NUM_F6:
				bios_scan=0x59; break;
			case NUM_F7:
				bios_scan=0x5a; break;
			case NUM_F8:
				bios_scan=0x5b; break;
			case NUM_F9:
				bios_scan=0x5c; break;
			case NUM_F10:
				bios_scan=0x5d; break;
			case NUM_F11:
				bios_scan=0x87; break;
			case NUM_F12:
				bios_scan=0x88; break;
			case NUM_PAD_ENTER:
				bios_scan=0xe0; break;
			case NUM_PRTSCR:
				bios_scan=0; break;
			case NUM_PAD_SLASH:
				bios_scan=0xe0; break;
			default:
				bios_scan=key&0x7f; break;
			}
			/* mark the extended keys */
			switch(key) {
				/* cursor block keys */
			case NUM_HOME:	case NUM_UP:	case NUM_PGUP: 
			case NUM_LEFT:	case NUM_RIGHT: 
			case NUM_END:	case NUM_DOWN:	case NUM_PGDN:
			case NUM_INS:	case NUM_DEL:
				ascii = 0xe0;
			}
		}
		else {	 /* unshifted */
			switch(key) {
			case NUM_F11:
				bios_scan=0x85; break;
			case NUM_F12:
				bios_scan=0x86; break;
			case NUM_PAD_ENTER:
				bios_scan=0xe0; break;
			case NUM_PRTSCR:
				bios_scan=0; break;
			case NUM_PAD_SLASH:
				bios_scan=0xe0; break;
			default:
				bios_scan=key&0x7f; break;
			}
			/* mark the extended keys */
			switch(key) {
				/* cursor block keys */
			case NUM_HOME:	case NUM_UP:	case NUM_PGUP: 
			case NUM_LEFT:	case NUM_RIGHT: 
			case NUM_END:	case NUM_DOWN:	case NUM_PGDN:
			case NUM_INS:	case NUM_DEL:
				ascii = 0xe0;
			}
		}
	} 
	else { /* !make */
		if (key == NUM_SYSRQ) {
			special = SP_SYSRQ_BREAK;
		}
		else if (state->alt_num_buffer && 
			 (key == NUM_L_ALT || key == NUM_R_ALT)) {
			bios_scan = 0;
			ascii = state->alt_num_buffer;
			state->alt_num_buffer = 0;
		}
		else {
			ascii = 0;
		}
	}
#if 0
	k_printf("KBD: bios_scan=%02x ascii=%02x special=%04x\n",
		 bios_scan,ascii,special);
#endif
	if (special != 0) {
		return special;
	}
	else {
		return (bios_scan << 8)| ascii;
	}
}

static void do_shift_keys_r(Boolean make, t_keysym keysym, t_shiftstate *shiftstate_ret) 
{
	t_shiftstate mask=0, togglemask=0, shiftstate = *shiftstate_ret;
	
	/* Note:
	 *  While it is very bad to hard code keys, dos has already
	 *  hard coded the shift keys.  Dosemu cannot remove this
	 *  hard coding without confusing dos programs.   The point of
	 *  dosemu's configurability is primarily to allow non-us
	 *  keyboards to work as they would in dos.  Not to make a
	 *  nice place to customize your keyboard. 
	 */
	switch(keysym) {
	case KEY_L_ALT:    mask=L_ALT; break;
	case KEY_R_ALT:    mask=R_ALT; break;
	case KEY_L_CTRL:   mask=L_CTRL; break;
	case KEY_R_CTRL:   mask=R_CTRL; break;
	case KEY_L_SHIFT:  mask=L_SHIFT; break;
	case KEY_R_SHIFT:  mask=R_SHIFT; break;
	case KEY_CAPS:     mask=CAPS_PRESSED; togglemask=CAPS_LOCK; break;
	case KEY_NUM:      mask=NUM_PRESSED;  togglemask=NUM_LOCK;  break;
	case KEY_SCROLL:   mask=SCR_PRESSED;  togglemask=SCR_LOCK;  break;
		/* FIXME INS_PRESSED */
	case KEY_INS:      mask=INS_PRESSED;  togglemask=INS_LOCK;  break;
	case KEY_PAD_INS:  mask=INS_PRESSED;  togglemask=INS_LOCK;  break;
	case KEY_SYSRQ:    mask=SYSRQ_PRESSED; break;
	default:           break;
	}
	if (make) {
		/* don't toggle on repeat events */
		if (!(shiftstate &mask)) {
			shiftstate ^= togglemask;
		}
		shiftstate |= mask;
	}
	else {
		shiftstate &= ~mask;
	}
	
	if (mask)
		k_printf("KBD: do_shift_keys(%s): key=%02x new shiftstate=%04x\n",
			 (shiftstate_ret == &input_keyboard_state.shiftstate)? "input":
			 (shiftstate_ret == &dos_keyboard_state.shiftstate)? "dos":
			 "unknown", 
			 keysym, shiftstate);
	
#if 0
	if (make && config.shiftclearcaps && 
	    (keysym==KEY_L_SHIFT || keysym==KEY_R_SHIFT)) 
	{
		shiftstate &= ~CAPS_LOCK;
	}
#endif

	*shiftstate_ret = shiftstate;
	return;
}

static Boolean is_keypad_key(t_keynum key)
{
	Boolean result = FALSE;
	/* Note:
	 *  While it is very bad to hard code keys, dos has already
	 *  hard coded the number pad keys.  Dosemu cannot remove this
	 *  hard coding without confusing dos programs.  The point of 
	 *  dosemu's configurabiltiy is primairy to allow non-us
	 *  keyboards to work as they would in dos.  Not to make a nice
	 *  place to customize your keyboard.
	 */

	switch(key) {
		/* keypad keys */
	case NUM_PAD_7:	case NUM_PAD_8:	case NUM_PAD_9:	case NUM_PAD_MINUS:
	case NUM_PAD_4:	case NUM_PAD_5:	case NUM_PAD_6:	case NUM_PAD_PLUS:
	case NUM_PAD_1:	case NUM_PAD_2:	case NUM_PAD_3:
	case NUM_PAD_0:	case NUM_PAD_DECIMAL:
		result = TRUE;
	}
	return result;
}

static t_shiftstate translate_shiftstate(t_shiftstate cur_shiftstate,
	struct translate_rule *rule, t_keynum key, t_shiftstate *mask)
{
	t_shiftstate shiftstate = cur_shiftstate;

	if (mask)
		*mask = 0;

	if (is_keypad_key(key)) {
		/* just modifying my local copy of shiftstate... */
		if ((!!(shiftstate & NUM_LOCK)) ^ (!!(shiftstate & ANY_SHIFT))) {
			shiftstate |= ANY_SHIFT;
		} else {
			shiftstate &= ~ANY_SHIFT;
		}
		if (mask && ((shiftstate & ANY_SHIFT) != (cur_shiftstate & ANY_SHIFT)))
			*mask |= NUM_LOCK;
	}
	if (is_keysym_letter(rule->rule_map[key])) {
		/* Use the shift mappings to handle CAPS_LOCK */
		/* just modifying my local copy of shiftstate... */
		if ((!!(shiftstate & CAPS_LOCK)) ^ (!!(shiftstate & ANY_SHIFT))) {
			shiftstate |= ANY_SHIFT;
		} else {
			shiftstate &= ~ANY_SHIFT;
		}
		if (mask && ((shiftstate & ANY_SHIFT) != (cur_shiftstate & ANY_SHIFT)))
			*mask |= CAPS_LOCK;
	}
	k_printf("KBD: translate(old_shiftstate=%04x, shiftstate=%04x, key=%04x)\n", 
		 cur_shiftstate, shiftstate, key);

	return shiftstate;
}

/* translate a keynum to its ASCII equivalent. 
 * (*is_accent) returns TRUE if the if char was produced with an accent key.
 */

static t_keysym translate_r(Boolean make, t_keynum key, Boolean *is_accent,
	struct keyboard_state *state) 
{
	t_shiftstate shiftstate = translate_shiftstate(state->shiftstate,
		&state->rules->maps[state->rules->activemap].trans_rules.rule_structs.plain, key, NULL);
	t_keysym ch = KEY_VOID;

	*is_accent=FALSE;

	if ((shiftstate & ANY_ALT) && (shiftstate & ANY_CTRL)) {
		ch = state->rules->maps[state->rules->activemap].trans_rules.rule_structs.ctrl_alt.rule_map[key];
	}
	else if ((shiftstate & R_ALT) && (shiftstate & ANY_SHIFT)) {
		ch = state->rules->maps[state->rules->activemap].trans_rules.rule_structs.shift_altgr.rule_map[key];
	}
	else if (shiftstate & R_ALT) {
		ch = state->rules->maps[state->rules->activemap].trans_rules.rule_structs.altgr.rule_map[key];
	}
	else if (shiftstate & ANY_ALT) {
		ch = state->rules->maps[state->rules->activemap].trans_rules.rule_structs.alt.rule_map[key];
	}
	else if (shiftstate & ANY_CTRL) {
		ch = state->rules->maps[state->rules->activemap].trans_rules.rule_structs.ctrl.rule_map[key];
	}
	else if (shiftstate & ANY_SHIFT) {
		ch = state->rules->maps[state->rules->activemap].trans_rules.rule_structs.shift.rule_map[key];
	}
	else /* unshifted */ {
		ch = state->rules->maps[state->rules->activemap].trans_rules.rule_structs.plain.rule_map[key];
	}
	if (make && (state->accent != KEY_VOID)) {
		t_keysym new_ch = keysym_dead_key_translation(state->accent, ch);
		if ((new_ch != ch) && (state->rules->charset.keys[ch].character)) {
			/* ignore accented characters that aren't
			 * in the current character set.
			 */
			ch = new_ch;
		}
		state->accent = KEY_VOID;
		*is_accent = TRUE;
	}
	if (make && is_keysym_dead(ch)) {
		state->accent = ch;
		k_printf("KBD: got accent %04x\n", ch);
	}
	k_printf("KBD: translated key = %04x\n", ch);
	return ch;
}


/***********************************************************************************************
 * The key press work horse 
 ***********************************************************************************************/

static inline void send_scan_code_byte(Boolean make, unsigned char c)
{
	if (c) {
		if (!make) {
			c |= 0x80;
		}
		write_queue(&keyb_queue, c);
	}
}

static void send_scan_code_string(Boolean make, keystring scan_code_string)
{
	unsigned char c1, c2, c3, c4;

	c1 = (scan_code_string & 0xFF000000) >> 24;
	c2 = (scan_code_string & 0x00FF0000) >> 16;
	c3 = (scan_code_string & 0x0000FF00) >> 8;
	c4 = (scan_code_string & 0x000000FF);
	send_scan_code_byte(make, c1);
	send_scan_code_byte(make, c2);
	send_scan_code_byte(make, c3);
	send_scan_code_byte(make, c4);
}

static inline void assign_bit(Boolean value, int nr, void *addr)
{
	if (value) {
		set_bit(nr, addr);
	} else {
		clear_bit(nr, addr);
	}
}

t_keynum compute_functional_keynum(Boolean make, t_keynum keynum,
				   struct key_pressed_state *keystate)
{
	unsigned char *keys = keystate->keys;
	Boolean old_make;
	/* first handle the virutal keys */
	switch(keynum) {
	case NUM_PRTSCR_SYSRQ:
		/* If the key is already pressed find out what key it
		   really is */
		if (test_bit(NUM_PRTSCR_SYSRQ, keys)) {
			if (test_bit(NUM_PRTSCR, keys)) {
				keynum = NUM_PRTSCR;
			} else if (test_bit(NUM_SYSRQ, keys)) {
				keynum = NUM_SYSRQ;
			} else {
				/* should never happen */
				keynum = NUM_VOID;
			}
		} else if (make) {
			if (!test_bit(NUM_L_ALT, keys) && 
			    !test_bit(NUM_R_ALT, keys)) {
				keynum = NUM_PRTSCR;
			} else {
				keynum = NUM_SYSRQ;
			}
		} else {
			/* ignore extra breaks */
			keynum = NUM_VOID;
		}
		break;
	case NUM_PAUSE_BREAK:
		/* If the key is already pressed find out what key it
		   really is */
		if (test_bit(NUM_PAUSE_BREAK, keys)) {
			if (test_bit(NUM_PAUSE, keys)) {
				keynum = NUM_PAUSE;
			} else if (test_bit(NUM_BREAK, keys)) {
				keynum = NUM_BREAK;
			} else {
				/* should never happen */
				keynum = NUM_VOID;
			}
		} else if (make) {
			if (!test_bit(NUM_L_CTRL, keys) && 
			    !test_bit(NUM_R_CTRL, keys)) {
				keynum = NUM_PAUSE;
			} else {
				keynum = NUM_BREAK;
			}
		} else {
			/* ignore extra breaks */
			keynum = NUM_VOID;
		}
		break;
		/* pseudo keys aren't real kill them */
	case NUM_FAKE_L_SHIFT:
	case NUM_FAKE_R_SHIFT:
	case NUM_PRTSCR:
	case NUM_SYSRQ:
	case NUM_PAUSE:
	case NUM_BREAK:
		keynum = NUM_VOID;
		break;
	default:
		break;
	}
	/* Then suppress duplicate key releases */

	old_make = test_bit(keynum, keys);
	if (old_make == make) {
		if (!make) {
			/* don't send key releases when the key isn't pressed! */
			keynum = NUM_VOID;
		} else {
			/* auto repeats are o.k. */
		}
	}
	return keynum;
}

static void update_keypressed_state(Boolean make, t_keynum keynum,
				    struct key_pressed_state *keys)
{
	assign_bit(make, keynum, keys);
	switch(keynum) {
	case NUM_PAUSE:
	case NUM_BREAK:
		assign_bit(make, NUM_PAUSE_BREAK, keys);
		break;
	case NUM_PRTSCR:
	case NUM_SYSRQ:
		assign_bit(make, NUM_PRTSCR_SYSRQ, keys);
		break;
	}
}

/* additionaly strings */
#define SCN_FAKE_L_SHIFT	0xe02a
#define SCN_FAKE_R_SHIFT	0xe036
#define SCN_PAUSE		0xe11d45
#define SCN_BREAK		0xe046
#define SCN_PRTSCR		0xe037
#define SCN_SYSRQ		0x54
#define SCN_INS			0xe052
#define SCN_DEL			0xe053
#define SCN_HOME		0xe047
#define SCN_END			0xe04f
#define SCN_PGUP		0xe049
#define SCN_PGDN		0xe051
#define SCN_UP			0xe048
#define SCN_DOWN		0xe050
#define SCN_LEFT		0xe04b
#define SCN_RIGHT		0xe04d
#define SCN_PAD_SLASH		0xe035
#define SCN_L_SHIFT		0x002a
#define SCN_R_SHIFT		0x0036
#define SCN_PAUSE_MAKE		0xe11d45
#define SCN_PAUSE_BREAK		0xe19dc5

static keystring compute_scancode_string(Boolean make, t_keynum keynum)
{
	keystring scan_code_string;

	scan_code_string = keynum & 0x7f;
	if (keynum & 0x80) {
		scan_code_string |= 0xe000;
	}
	/* Fixup for virtual keys */
	switch(keynum) {
	case NUM_PAUSE:
		scan_code_string = SCN_PAUSE;
		break;
	case NUM_BREAK:
		scan_code_string = SCN_BREAK;
		break;
	case NUM_PRTSCR:
		scan_code_string = SCN_PRTSCR;
		break;
	case NUM_SYSRQ:
		scan_code_string = SCN_SYSRQ;
		break;
	}
	return scan_code_string;
}

/*
 * All key presses eventually go through put_keynum_r before they are
 * put into the queue and fed to dos.  This routine carefully keeps
 * track of the state before we queue the keys so we can accurately
 * predict the key translation dos, or our bios eventually makes.
 *
 * Also multiple key releases are filtered out, and work is put in to
 * add the appropriate dummy scancodes, so we can be indistinguishable
 * from a real keyboard.
 *
 * We also filter out keys here for dosemu's own personal use.
 * 
 * Predicting the key translation is important for put_character, and
 * cut and paste that rests on top of put_character.
 *
 * Not saving the translation is important for some rare but legal
 * applications of int15. 
 *
 * Also doing it this way ensures we generate the correct scancodes on non-us
 * keyboards for the characters pressed.  An important advantage.
 */

static void put_keynum_r(Boolean make, t_keynum input_keynum, struct keyboard_state *state)
{
	t_keynum keynum;
	Boolean old_make;
	keystring scan_code_string;
	struct key_pressed_state *keys = &state->keys_pressed;

	keynum = compute_functional_keynum(make, input_keynum, keys);
	if (keynum == NUM_VOID) {
		k_printf("put_keynum_r(%s) called with invalid keynum %02x:%02x\n", 
			 (make?"make":"break"), input_keynum, keynum);
		return;
	}
	old_make = test_bit(keynum, keys);

	k_printf("put_keynum_r: old_make:%d make:%d key:%02x\n",
		 !!old_make, !!make, keynum);

	/* update various bits of keyboard state */
	if (translate_key(make, keynum, state) == (Bit16u)-1)
		return;
	
	scan_code_string = compute_scancode_string(make, keynum);

	/* A real pc keyboard for backwards compatibility inserts
	 * fake presses and releases of the left and right shift keys
	 */

	/* I check here for keys that need SCN_FAKE_L_SHIFT or
	 * SCN_FAKE_R_SHIFT in front of them.
	 */
	switch(scan_code_string) {
	case SCN_INS:	case SCN_HOME:	case SCN_PGUP:
	case SCN_DEL:	case SCN_END:	case SCN_PGDN:
			case SCN_UP:
	case SCN_DOWN:	case SCN_LEFT:	case SCN_RIGHT:
	case SCN_PAD_SLASH:
		if (make) {
	/* Real PC keyboards use the state of the NumLock light, for
	 * the numlock state.  Since I depend on being able to
	 * pretranslate the keyboard for cut and paste, I'll just use
	 * my expected numlock state.
	 * At any rate this is just a heuristic, and not garanteed to
	 * be correct so any reasonable approximation should be fine.
	 */
			Boolean num_state = !!(state->shiftstate & NUM_LOCK);
			Boolean fake_lshift = !!test_bit(NUM_FAKE_L_SHIFT, keys);
			Boolean fake_rshift = !!test_bit(NUM_FAKE_R_SHIFT, keys);
			if (scan_code_string != SCN_PAD_SLASH) {
				if (num_state && !fake_lshift && !fake_rshift) {
					send_scan_code_string(PRESS, SCN_FAKE_L_SHIFT);
					set_bit(NUM_FAKE_L_SHIFT, keys);
				}
				fake_lshift = fake_lshift && !num_state;
				fake_rshift = fake_rshift && !num_state;
			}
			if (fake_lshift) {
				send_scan_code_string(RELEASE, SCN_FAKE_L_SHIFT);
				clear_bit(NUM_FAKE_L_SHIFT, keys);
			}
			if (fake_rshift) {
				send_scan_code_string(RELEASE, SCN_FAKE_R_SHIFT);
				clear_bit(NUM_FAKE_R_SHIFT, keys);
			}
		}
		send_scan_code_string(make, scan_code_string);
		if (!make) {
			Boolean lshift = test_bit(NUM_L_SHIFT, keys);
			Boolean rshift = test_bit(NUM_R_SHIFT, keys);
			if (!!test_bit(NUM_FAKE_L_SHIFT, keys) != !!lshift) {
				send_scan_code_string(lshift, SCN_FAKE_L_SHIFT);
				assign_bit(lshift, NUM_FAKE_L_SHIFT, keys);
			}
			if (!!test_bit(NUM_FAKE_R_SHIFT, keys) != !!rshift) {
				send_scan_code_string(rshift, SCN_FAKE_R_SHIFT);
				assign_bit(rshift, NUM_FAKE_R_SHIFT, keys);
			}
		}
		break;
	case SCN_PRTSCR:
		if (make) {
			Boolean fake_lshift = test_bit(NUM_FAKE_L_SHIFT, keys);
			Boolean fake_rshift = test_bit(NUM_FAKE_R_SHIFT, keys);
			if (!test_bit(NUM_L_CTRL, keys) && 
			    !test_bit(NUM_R_CTRL, keys) &&
			    !fake_lshift && !fake_rshift) {
				send_scan_code_string(PRESS, SCN_FAKE_L_SHIFT);
				set_bit(NUM_FAKE_L_SHIFT, keys);
			}
		}
		send_scan_code_string(make, scan_code_string);
		if (!make) {
			Boolean lshift = test_bit(NUM_L_SHIFT, keys);
			Boolean rshift = test_bit(NUM_R_SHIFT, keys);
			if (!!test_bit(NUM_FAKE_L_SHIFT, keys) != !!lshift) {
				send_scan_code_string(lshift, SCN_FAKE_L_SHIFT);
				assign_bit(lshift, NUM_FAKE_L_SHIFT, keys);
			}
			if (!!test_bit(NUM_FAKE_R_SHIFT, keys) != !!rshift) {
				send_scan_code_string(rshift, SCN_FAKE_R_SHIFT);
				assign_bit(rshift, NUM_FAKE_R_SHIFT, keys);
			}
		}
		break;
	case SCN_L_SHIFT:
	{
		Boolean rshift = test_bit(NUM_R_SHIFT, keys);
		if (!!test_bit(NUM_FAKE_R_SHIFT, keys) != !!rshift) {
			send_scan_code_string(rshift, SCN_FAKE_R_SHIFT);
			assign_bit(rshift, NUM_FAKE_R_SHIFT, keys);
		}
		/* Clear fake shift actions if applicable */
		if ((!!old_make != !!make) && 
		    (!!test_bit(NUM_FAKE_L_SHIFT, keys) == !!make)) {
			send_scan_code_string(!make, SCN_FAKE_L_SHIFT);
		}
		assign_bit(make, NUM_FAKE_L_SHIFT, keys);
		send_scan_code_string(make, scan_code_string);
		break;
	}
	case SCN_R_SHIFT:
	{
		Boolean lshift = test_bit(NUM_L_SHIFT, keys);
		if (!!test_bit(NUM_FAKE_L_SHIFT, keys) != !!lshift) {
			send_scan_code_string(lshift, SCN_FAKE_L_SHIFT);
			assign_bit(lshift, NUM_FAKE_L_SHIFT, keys);
		}
		/* Clear fake shift actions if applicable */
		if ((!!old_make != !!make) && 
		    (!!test_bit(NUM_FAKE_R_SHIFT, keys) == !!make)) {
			send_scan_code_string(!make, SCN_FAKE_R_SHIFT);
		}
		assign_bit(make, NUM_FAKE_R_SHIFT, keys);
		send_scan_code_string(make, scan_code_string);
		break;
	}
	default:
	{
		Boolean lshift = test_bit(NUM_L_SHIFT, keys);
		Boolean rshift = test_bit(NUM_R_SHIFT, keys);
		if (!!test_bit(NUM_FAKE_L_SHIFT, keys) != !!lshift) {
			send_scan_code_string(lshift, SCN_FAKE_L_SHIFT);
			assign_bit(lshift, NUM_FAKE_L_SHIFT, keys);
		}
		if (!!test_bit(NUM_FAKE_R_SHIFT, keys) != !!rshift) {
			send_scan_code_string(rshift, SCN_FAKE_R_SHIFT);
			assign_bit(rshift, NUM_FAKE_R_SHIFT, keys);
		}
		send_scan_code_string(make, scan_code_string);
		break;
	}
	}
	/* although backend_run() is called periodically anyway, for faster response
	 * it's a good idea to call it straight away.
	 */
	backend_run();
}

static void put_keynum(Boolean make, t_keynum input_keynum, t_keysym sym, struct keyboard_state *state)
{
	if (sym != KEY_VOID) {
		/* switch active keymap if needed */
		state->rules->activemap = state->rules->charset.keys[sym].map;
	}
	put_keynum_r(make, input_keynum, state);
}

 /***********************************************************************************************
  * Intermediate level interface functions 
  ***********************************************************************************************/
  
/*
 * DANG_BEGIN_FUNCTION compute_keynum
 *
 * The task of compute_keynum() is to 'collect' keyboard bytes (e.g.
 * 0xe0 prefixes) until it thinks it has assembled an entire keyboard
 * event. The entire keyboard event is then returned, otherwise
 * NUM_VOID is returned.
 * 
 * DANG_END_FUNCTION
 */

t_keynum compute_keynum(Boolean *make_ret, 
			t_rawkeycode code, struct raw_key_state *state) 
{
	t_scancode scan;
	t_keynum key;
	Boolean make = FALSE;
	*make_ret = FALSE;
	
	k_printf("KBD: compute_keynum(%x, %x, %s) called\n",
		 (int)code, state->rawprefix, 
		 (state ==&input_keyboard_state.raw_state)? "input":
		 (state ==&dos_keyboard_state.raw_state)? "dos":
		 "unknown");
	
	if (code==0xe0 || code==0xe1) {
		state->rawprefix=code;
		return NUM_VOID;
	}
	
	if (state->rawprefix==0xe1) {     /* PAUSE key: expext 2 scancode bytes */
		state->rawprefix = 0xe100 | code;
		return NUM_VOID;
	}
	
	scan = (state->rawprefix<<8) | code;
	state->rawprefix=0;
	
	if ((scan&0xff0000) == 0xe10000)
		k_printf("KBD: E1 scancode 0x%06x\n",(int)scan);
	
	/* for BIOS, ignore the fake shift scancodes 0xe02a, 0xe0aa 0xe036, 0xe0b6
	   sent by the keyboard for fn/numeric keys */
	
	if (scan==0xe02a || scan==0xe0aa || scan==0xe036 || scan==0xe0b6) {
		key = NUM_VOID;
	}
	else {
		if (scan==SCN_PAUSE_MAKE) {
			key = NUM_PAUSE_BREAK;
			make = TRUE;
		}
		else if (scan==SCN_PAUSE_BREAK) {
			key = NUM_PAUSE_BREAK;
			make = FALSE;
		}
		else if ((scan & (~0x80)) == SCN_PRTSCR) {
			key = NUM_PRTSCR_SYSRQ;
			make = ((scan & 0x80) == 0);
		} else {
			key = scan & 0x7f;
			if ((scan & 0xff00) != 0) {
				key |= 0x80;
			}
			make = ((scan & 0x80) == 0);
		}
	}
	if (!validate_keynum(key)) {
		key = NUM_VOID;
	}
	*make_ret = make;
	return key;
}
/* 
 * DANG_BEGIN_FUNCTION translate_key
 * translate_key takes a keysym event and calculates the appropriate
 * bios translation.
 *
 * As a side effect translate_key updates the apropriate pieces of state
 * to reflect the current keyboard state.
 * 
 * Calling translate_key twice on the same data is likely to be hazardous.
 *
 * DANG_END_FUNCTION
 */

Bit16u translate_key(Boolean make, t_keynum key, 
		   struct keyboard_state *state)
{
	Boolean is_accent;
	Bit16u bios_key;
	Bit8u ascii;
	t_keysym keysym;

	k_printf("translate_key: make=%d, key=%04x, %s\n", 
		 make, key,
		 (state == &input_keyboard_state)? "input":
		 (state == &dos_keyboard_state)? "dos":
		 "unknown");

	/* Each of the following functions:
	 * update_keypressed_state, 
	 * do_shift_keys_r, translate_r & make_bios_code_r can potentially
	 * change the keyboard state.
	 */
	
	update_keypressed_state(make, key, &state->keys_pressed);
				     	
	is_accent = FALSE;
	keysym = translate_r(make, key, &is_accent, state);
	do_shift_keys_r(make, keysym, &state->shiftstate);
	if (handle_dosemu_keys(make, keysym)) {
		return -1;
	}
	ascii = state->rules->charset.keys[keysym].character;

	/* translate to a BIOS (soft) scancode.  For accented characters, the BIOS
	 * code is 0, i.e. bios_key == ((0 << 8)|ascii) == ascii
	 * just like ALT-numpad sequences.
	 */
	bios_key = state->accent != KEY_VOID ? 0 :
	  is_accent ? ascii : make_bios_code_r(make, key, ascii, keysym, state);

	if (bios_key == 0x23e0 || bios_key == 0x18e0)  /* Cyrillic_er work around */
		bios_key &= 0x00FF;  /* ^^^^^^^^^^^^^^^^^^ Oacute */
#if 0
	k_printf("translate_key: keysym=%04x bios_key=%04x\n",
		keysym, bios_key);
#endif

	return bios_key;
}



/* typing a specifc character */


/* get the logical modifiers from the keyboard state */
t_modifiers get_modifiers_r(t_shiftstate shiftstate)
{
	t_modifiers modifiers = 0;
	if (shiftstate & ANY_SHIFT)	modifiers |= MODIFIER_SHIFT;
	if (shiftstate & ANY_CTRL)	modifiers |= MODIFIER_CTRL;
	if (shiftstate & ANY_ALT)	modifiers |= MODIFIER_ALT;
	/* Note: when R_ALT is active both MODIFIER_ALT & MODIFIER_ALTGR are
	 * active 
	 */
	if (shiftstate & R_ALT)		modifiers |= MODIFIER_ALTGR;
	if (shiftstate & CAPS_LOCK)	modifiers |= MODIFIER_CAPS;
	if (shiftstate & NUM_LOCK)	modifiers |= MODIFIER_NUM;
	if (shiftstate & SCR_LOCK)	modifiers |= MODIFIER_SCR;
	if (shiftstate & INS_LOCK)	modifiers |= MODIFIER_INS;
	return modifiers;
}

/*
 * sync_shift_state takes the current shiftstate, and the shiftstate
 * we want to set and generates the appropriate key strokes to cause
 * the new shiftstate to come into effect.
 */

static void sync_shift_state(t_modifiers desired, struct keyboard_state *state) 
{
	t_modifiers current = get_modifiers_r(state->shiftstate);
	struct key_pressed_state *keys = &state->keys_pressed;

	/* altgr implies alt */
	if (desired & MODIFIER_ALTGR)	desired |= MODIFIER_ALT;

	/* To keep some bits constant copy them from state->shiftstate into 
	 * desired.
	 */
	if (current == desired) {
		return;
	}
	k_printf("KBD: sync_shift_state(%s): current=%04x desired=%04x\n",
		 ((state == &input_keyboard_state)? "input":
		  (state == &dos_keyboard_state)? "dos":
		  "unknown"),
		 current, desired);

	if (!!(current & MODIFIER_INS) != !!(desired & MODIFIER_INS)) {
		t_keynum keynum1 = state->rules->charset.keys[KEY_INS].key;
		t_keynum keynum2 = state->rules->charset.keys[KEY_PAD_INS].key;

		/* by preference toggle something that is already held down */
		if (test_bit(keynum1, state)) {
			put_keynum_r(RELEASE, keynum1, state);
			put_keynum_r(PRESS, keynum1, state);
		} else if (!(current & MODIFIER_NUM) && test_bit(keynum2, state)) {
			put_keynum_r(RELEASE, keynum2, state);
			put_keynum_r(PRESS, keynum2, state);
		} else {
			put_keynum_r(PRESS, keynum1, state);
			put_keynum_r(RELEASE, keynum1, state);
		}
	}
	if (!!(current & MODIFIER_CAPS) != !!(desired & MODIFIER_CAPS)) {
		t_keynum keynum = state->rules->charset.keys[KEY_CAPS].key;
		if (test_bit(keynum, keys)) {
			put_keynum_r(RELEASE, keynum, state);
			put_keynum_r(PRESS, keynum, state);
		} else {
			put_keynum_r(PRESS, keynum, state);
			put_keynum_r(RELEASE, keynum, state);
		}
	}
	if (!!(current & MODIFIER_NUM) != !!(desired & MODIFIER_NUM)) {
		t_keynum keynum = state->rules->charset.keys[KEY_NUM].key;
		if (test_bit(keynum, keys)) {
			put_keynum_r(RELEASE, keynum, state);
			put_keynum_r(PRESS, keynum, state);
		} else {
			put_keynum_r(PRESS, keynum, state);
			put_keynum_r(RELEASE, keynum, state);
		}
	}
	if (!!(current & MODIFIER_SCR) != !!(desired & MODIFIER_SCR)) {
		t_keynum keynum = state->rules->charset.keys[KEY_SCROLL].key;
		if (test_bit(keynum, keys)) {
			put_keynum_r(RELEASE, keynum, state);
			put_keynum_r(PRESS, keynum, state);
		} else {
			put_keynum_r(PRESS, keynum, state);
			put_keynum_r(RELEASE, keynum, state);
		}
	}
	if (!!(current & MODIFIER_SHIFT) != !!(desired & MODIFIER_SHIFT)) {
		t_keynum lkeynum = state->rules->charset.keys[KEY_L_SHIFT].key;
		t_keynum rkeynum = state->rules->charset.keys[KEY_R_SHIFT].key;
		if (current & MODIFIER_SHIFT) {
			if (state->shiftstate & L_SHIFT) {
				put_keynum_r(RELEASE, lkeynum, state);
			}
			if (state->shiftstate & R_SHIFT) {
				put_keynum_r(RELEASE, rkeynum, state);
			}
		} else {
			put_keynum_r(PRESS, lkeynum, state);
		}
	}
	if (!!(current & MODIFIER_CTRL) != !!(desired & MODIFIER_CTRL)) {
		t_keynum lkeynum = state->rules->charset.keys[KEY_L_CTRL].key;
		t_keynum rkeynum = state->rules->charset.keys[KEY_R_CTRL].key;
		if (current & MODIFIER_CTRL) {
			if (state->shiftstate & L_CTRL) {
				put_keynum_r(RELEASE, lkeynum, state);
			}
			if (state->shiftstate & R_CTRL) {
				put_keynum_r(RELEASE, rkeynum, state);
			}
		} else {
			put_keynum_r(PRESS, lkeynum, state);
		}
	}
	if (!!(current & MODIFIER_ALTGR) != !!(desired & MODIFIER_ALTGR)) {
		t_keynum keynum = state->rules->charset.keys[KEY_R_ALT].key;
		if (current & MODIFIER_ALTGR) {
			put_keynum_r(RELEASE, keynum, state);
		} else {
			put_keynum_r(PRESS, keynum, state);
		}
	}
	/* reread because of interactions */
	current = get_modifiers_r(state->shiftstate);
	if (!!(current & MODIFIER_ALT) != !!(desired & MODIFIER_ALT)) {
		t_keynum lkeynum = state->rules->charset.keys[KEY_L_ALT].key;
		t_keynum rkeynum = state->rules->charset.keys[KEY_R_ALT].key;
		if (current & MODIFIER_ALT) {
			if (state->shiftstate & L_ALT) {
				put_keynum_r(RELEASE, lkeynum, state);
			}
			if (state->shiftstate & R_ALT) {
				put_keynum_r(RELEASE, rkeynum, state);
			}
		} else {
			put_keynum_r(PRESS, lkeynum, state);
		}
	}
	k_printf("KBD: sync_shift_state(%s) done: current=%04x desired=%04x\n",
		 ((state == &input_keyboard_state)? "input":
		  (state == &dos_keyboard_state)? "dos":
		  "unknown"),
		 get_modifiers_r(state->shiftstate), desired);
}

/*
 * type_alt_num() is for those occasions when we desired to type a
 * specific letter, as in cut and paste, but we don't have a normal
 * method to type that letter.  So we generate the keystrokes needed
 * to use the alt num buffer to type the desired character.
 */
static void type_alt_num(unsigned char ascii, struct keyboard_state *state)
{
	int old_alt_num_buffer;
	int amount_to_add, one,two,three;
	t_shiftstate alt_shiftstate;
	
	static const t_keynum num_key[] = 
	{ NUM_PAD_0, NUM_PAD_1, NUM_PAD_2, NUM_PAD_3, NUM_PAD_4, 
	  NUM_PAD_5, NUM_PAD_6, NUM_PAD_7, NUM_PAD_8, NUM_PAD_9 };

	k_printf("KBD: type_alt_num(%04x, '%c') called\n",
		 ascii, ascii?ascii:' ');
	/* insert these keys as alt number combinations,
	 * after appropriately setting the shift state.
	 */
	alt_shiftstate = state->shiftstate & ANY_ALT;
	if (alt_shiftstate & ANY_ALT) {
		old_alt_num_buffer = state->alt_num_buffer;
	} else {
		old_alt_num_buffer = 0;
		put_keynum_r(PRESS, NUM_L_ALT, state);
	}
	amount_to_add = (ascii - old_alt_num_buffer) & 0xff;
	three = amount_to_add %10; amount_to_add /= 10;
	two = amount_to_add %10;  amount_to_add /= 10;
	one = amount_to_add %10;

	put_keynum_r(PRESS, num_key[one], state);
	put_keynum_r(RELEASE, num_key[one], state);
	put_keynum_r(PRESS, num_key[two], state);
	put_keynum_r(RELEASE, num_key[two], state);
	put_keynum_r(PRESS, num_key[three], state);
	put_keynum_r(RELEASE, num_key[three], state);

	if (!(alt_shiftstate & ANY_ALT)) {
		put_keynum_r(RELEASE, NUM_L_ALT, state);
		return;  /* LEAVE */
	}
	/* release the alt keys to force the ascii code to appear */
	if (alt_shiftstate &L_ALT) {
		put_keynum_r(RELEASE, NUM_L_ALT, state);
	}
	if (alt_shiftstate &R_ALT) {
		put_keynum_r(RELEASE, NUM_R_ALT, state);
	}
	/* press the alt keys again */
	if (alt_shiftstate &L_ALT) {
		put_keynum_r(PRESS, NUM_L_ALT, state);
	}
	if (alt_shiftstate &R_ALT) {
		put_keynum_r(PRESS, NUM_R_ALT, state);
	}
	
	/* press keys to restore the old alt_num_buffer */
	amount_to_add = old_alt_num_buffer;
	three = amount_to_add %10; amount_to_add /= 10;
	two = amount_to_add %10;  amount_to_add /= 10;
	one = amount_to_add %10;

	put_keynum_r(PRESS, num_key[one], state);
	put_keynum_r(RELEASE, num_key[one], state);
	put_keynum_r(PRESS, num_key[two], state);
	put_keynum_r(RELEASE, num_key[two], state);
	put_keynum_r(PRESS, num_key[three], state);
	put_keynum_r(RELEASE, num_key[three], state);

}

/*
 * put_character_symbol() uses the precalculated reverse key maps to
 * press a specific character, and release it.  If there is no
 * appropriate entry in the reverse keymap, it calls type_alt_num() to
 * type it that way instead.
 *
 * It also handles a slightly ugly case.  Sometimes it is simply known that we
 * want some symbol with extra modifiers added.  Examples are Ctrl-A or Alt-A.
 * Quite a few of these don't have their own special symbols, and it wouldn't
 * help even if they did, because we generally don't have enough information to 
 * map those hypothetical symbols to keys.
 *
 * I make an attempt to handle that ugly case here, but for symbols that you
 * require dead keys to press, or symbols that can only be pressed with an 
 * alt# combination I ignore the request for extra modifiers.  Perhaps later.
 *
 * Note: this routine does not allow for the modification of the shiftstate!
 */

static void put_character_symbol(
	Boolean make, t_modifiers modifiers, t_keysym ch, 
	struct keyboard_state *state)
{
	t_modifiers old_shiftstate, new_shiftstate;
	struct press_state *key;
	if (ch == KEY_VOID) {
		return;
	}
	old_shiftstate = get_modifiers_r(state->shiftstate);
	key = &state->rules->charset.keys[ch];
	/* switch active keymap if needed */
	state->rules->activemap = key->map;
	new_shiftstate = key->shiftstate | (old_shiftstate & key->shiftstate_mask);
	if (key->deadsym == KEY_VOID) {
		new_shiftstate |= modifiers;
	}
	if (key->key != NUM_VOID) {
		if (make) {
			/* Note: dead keys always have the same shiftstate */
			sync_shift_state(new_shiftstate, state);
			put_character_symbol(PRESS, 0, key->deadsym, state);
			put_character_symbol(RELEASE, 0, key->deadsym, state);
			put_keynum_r(PRESS, key->key, state);
		}
		else {
			put_keynum_r(RELEASE, key->key, state);
		}
	} else {
		if (make) {
			type_alt_num(state->rules->charset.keys[ch].character, state);
		}
	}
	sync_shift_state(old_shiftstate, state);
}

/***********************************************************************************************
 * High-level interface functions 
 ***********************************************************************************************/

/* 
 * DANG_BEGIN_FUNCTION put_rawkey
 *
 * This function sends a raw keycode byte, e.g. read directly from the hardware,
 * to DOS. It is both queued for the port60h emulation and processed for the
 * BIOS keyboard buffer, using the national translation tables etc.
 *
 * For DOS applications using int16h we will therefore not have to load
 * KEYB.EXE, others (e.g. games) need their own drivers anyway.
 *
 * This function is used if we are at the console and config.rawkeyboard=on.
 *
 * DANG_END_FUNCTION
 */

/*
 * the main task of putrawkey() is to 'collect' keyboard bytes (e.g.
 * 0xe0 prefixes) until it thinks it has assembled an entire keyboard
 * event. The combined keycode is then both translated and stored
 * in the 'raw' field of the queue.
 */

void put_rawkey(t_rawkeycode code) 
{
	Boolean make;
	t_keynum key;
	key = compute_keynum(&make, code, &input_keyboard_state.raw_state);
	if (key != NUM_VOID) {
		put_keynum_r(make, key, &input_keyboard_state);
	}
}

/* 
 * DANG_BEGIN_FUNCTION move_keynum
 *
 * This does all the work of sending a key event to DOS.
 * Either pressing a key releasing one.  The key to move is
 * the key specified by keynum.
 *  
 *   keynum - the keynum from keynum.h indicating a physical key
 *   make  - TRUE for key press, FALSE for release
 *
 * Applications using int16h will always see the appropriate ASCII code
 * for the given keyboard key and the current keyboard state.  All the
 * chracter translation is done for you to keep from reporting
 * inconsistent key events.
 *
 * An emulated hardware scancode is also sent to port60h.
 *
 * Note that you have to send both MAKE (press) and BREAK (release) events.
 * If no BREAK codes are available (e.g. terminal mode), send them
 * immediately after the MAKE codes.
 *
 * DANG_END_FUNCTION
 */

int move_keynum(Boolean make, t_keynum keynum, t_keysym sym)
{
	int result = 0;
	k_printf("move_key: keynum=%04x\n", keynum);
	keynum = validate_keynum(keynum);
	/* move_keynum only works for valid keynum... */
	if (keynum != NUM_VOID) {
		put_keynum(make, keynum, sym, &input_keyboard_state);
	} else {
		result = -1;
	}
	return result;
}

/* 
 * DANG_BEGIN_FUNCTION keysym_to_keynum
 *
 * Allows peeking into the keytables.
 * This returns the keynum a given keysym sits on.
 *
 * DANG_END_FUNCTION
 */

t_keynum keysym_to_keynum(t_keysym key)
{
	struct press_state *sym_info;
	t_keynum keynum = NUM_VOID;
	if (key != KEY_VOID) {
		sym_info = &input_keyboard_state.rules->charset.keys[key];
		keynum = sym_info->key;
	}
	return keynum;
}

/* 
 * DANG_BEGIN_FUNCTION move_key
 *
 * This does all the work of sending a key event to DOS.
 * Either pressing a key releasing one.  The key to move is
 * the key that is labeled with the specified keysym.
 *  
 *   key  - the keysym, one of the KEY_ constants from new-kbd.h
 *   make  - TRUE for key press, FALSE for release
 *
 * Applications using int16h will always see the appropriate ASCII code
 * for the given keyboard key and the current keyboard state.  All the
 * chracter translation is done for you to keep from reporting
 * inconsistent key events.
 *
 * An emulated hardware scancode is also sent to port60h.
 *
 * Note that you have to send both MAKE (press) and BREAK (release) events.
 * If no BREAK codes are available (e.g. terminal mode), send them
 * immediately after the MAKE codes.
 *
 * DANG_END_FUNCTION
 */

int move_key(Boolean make, t_keysym key)
{
	int result = 0;
	struct press_state *sym_info;
	t_keynum keynum;
	t_keysym deadsym;
	sym_info = &input_keyboard_state.rules->charset.keys[key];
	keynum = sym_info->key;
	deadsym = sym_info->deadsym;
	k_printf("move_key: key=%04x keynum=%04x\n",
		key, keynum);
	/* move_key only works for key caps, and a symbol only reachable
	 * by pressing a dead key first doesn't qualify as a key cap.
	 */
	if ((keynum != NUM_VOID) && (deadsym == KEY_VOID)) {
		put_keynum_r(make, keynum, &input_keyboard_state);
	} else {
		result = -1;
	}
	return result;
}

/* 
 * DANG_BEGIN_FUNCTION put_symbol
 *
 * This does all the work of sending a key event to DOS.
 *   sym -- The unicode value of the symbol you want to send
 *
 * Applications using int16h will always see the symbol passed
 * here, if it is representable in the current dos character set.  The
 * appropriate scancodes are generated automatically to keep the
 * keyboard code consistent.  
 *
 * An emulated hardware scancode is also sent to port60h.
 *
 * Note that you have to send both MAKE (press) and BREAK (release) events.
 * If no BREAK codes are available (e.g. terminal mode), send them
 * immediately after the MAKE codes.
 *
 * DANG_END_FUNCTION
 */

void put_symbol(Boolean make, t_keysym sym) 
{
	k_printf("put_symbol: keysym=%04x\n",
		 sym);
	put_character_symbol(make, 0, sym, &input_keyboard_state);
}


/* 
 * DANG_BEGIN_FUNCTION put_modified_symbol
 *
 * This does all the work of sending a key event to DOS.
 *   sym -- The unicode value of the symbol you want to send
 *   modifiers  -- modifiers like alt etc you what to change your symbol with.
 *
 * This function is a concession to the reality, in which key events
 * are a composed of active modifiers, and a key label.
 *
 * This function behaves as put_symbol does, except before pressing
 * the key it adds the specified modifiers to the modifiers it would
 * normally use.
 * 
 * For cases where the symbol can only be created by an alt# combination
 * or by pressing a dead key (Basically any case where more than one
 * key is requried, after setting the shiftstate) it gives up and just
 * sends the symbol.
 *
 * Note that you have to send both MAKE (press) and BREAK (release) events.
 * If no BREAK codes are available (e.g. terminal mode), send them
 * immediately after the MAKE codes.
 *
 * DANG_END_FUNCTION
 */

void put_modified_symbol(Boolean make, t_modifiers modifiers, t_keysym sym)
{
	k_printf("put_symbol: modifiers=%04x keysym=%04x\n",
		 modifiers, sym);
	put_character_symbol(make, modifiers, sym, &input_keyboard_state);
}

/* 
 * DANG_BEGIN_FUNCTION get_shiftstate
 *
 * This simply reads the keyboard server's shift state.
 * 
 * This is intended to be used in conjunction with set_shiftstate
 * to sync up a shiftstate with a source of key events.
 *
 * With the addition of this function the keyboard inteface is clean enough
 * so if needed a completly different translation engine can be dropped in
 * to support a totally different environment (windows or whatever).
 *
 * DANG_END_FUNCTION
 */

t_modifiers get_shiftstate(void)
{
	return get_modifiers_r(input_keyboard_state.shiftstate);
}

/* 
 * DANG_BEGIN_FUNCTION set_shiftstate
 *
 * This simply sets the keyboard server's shift state.
 *
 * If there are shiftstate bits you want to keep fixed simply grab them with 
 * get_shiftstate, before calling this function.
 *
 * This changes the keyboard flags by generating the appropriate 
 * shift key make/break codes that normally come along with such
 * changes. So this function should be safe in any context.
 *
 * Note also that you can't simply write to the shiftstate variable
 * instead of using this function.
 *
 * DANG_END_FUNCTION
 */

void set_shiftstate(t_modifiers s) 
{
	sync_shift_state(s, &input_keyboard_state);
} 


int keyb_server_reset(void) 
{
	k_printf("KBD: keyb_server_reset()\n");

	keyb_reset_state();
	backend_reset();
   
	return TRUE;
}

int keyb_server_init(void) 
{
	k_printf("KBD: keyb_server_init()\n");
	if (!config.keytable)
		setup_default_keytable();
	keyb_init_state();
	return 1;
}

void keyb_server_close(void) 
{
	k_printf("KBD: keyb_server_close()\n");
}

void keyb_server_run(void)
{
	backend_run();
}
 
