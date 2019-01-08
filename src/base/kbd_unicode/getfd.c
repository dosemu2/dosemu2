/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "getfd.h"

#ifdef HAVE_KD_H
/* this code comes from kbd-1.08 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/kd.h>
#include <sys/ioctl.h>
#include <linux/keyboard.h>
#include "priv.h"
#include "keyboard/keynum.h"
#include "translate/translate.h"

/*
 * getfd.c
 *
 * Get an fd for use with kbd/console ioctls.
 * We try several things because opening /dev/console will fail
 * if someone else used X (which does a chown on /dev/console).
 */

static int cons_fd;

static int
is_a_console(int fd) {
    char arg;

    arg = 0;
    return (ioctl(fd, KDGKBTYPE, &arg) == 0
	    && ((arg == KB_101) || (arg == KB_84)));
}

static int
open_a_console(const char *fnam) {
    int fd;
    PRIV_SAVE_AREA;

    enter_priv_on();
    fd = open(fnam, O_RDONLY);
    if (fd < 0 && errno == EACCES)
      fd = open(fnam, O_WRONLY);
    leave_priv_setting();
    if (fd < 0)
      return -1;
    if (!is_a_console(fd)) {
      close(fd);
      return -1;
    }
    return fd;
}

static int _getfd(void) {
    int fd;

    fd = open_a_console("/dev/tty");
    if (fd >= 0)
      return fd;

    fd = open_a_console("/dev/tty0");
    if (fd >= 0)
      return fd;

    fd = open_a_console("/dev/vc/0");
    if (fd >= 0)
      return fd;

    fd = open_a_console("/dev/console");
    if (fd >= 0)
      return fd;

    for (fd = 0; fd < 3; fd++)
      if (is_a_console(fd))
	return fd;

    return -1;
}

int open_console(void)
{
    cons_fd = _getfd();
    return cons_fd;
}

int getfd()
{
    return cons_fd;
}

struct keycode_map {
	unsigned char dosemu;
	unsigned char kernel;
};
static const struct keycode_map dosemu_to_kernel[] =
{
	{NUM_L_ALT,		0x38},
	{NUM_R_ALT,		100},
	{NUM_L_CTRL,		0x1d},
	{NUM_R_CTRL,		97},
	{NUM_L_SHIFT,		0x2a},
	{NUM_R_SHIFT,		0x36},
	{NUM_NUM,		0x45},
	{NUM_SCROLL,		0x46},
	{NUM_CAPS,		0x3a},
	{NUM_SPACE,		0x39},
	{NUM_BKSP,		0x0e},
	{NUM_RETURN,		0x1c},
	{NUM_TAB,		0x0f},
	{NUM_A,			0x1e},
	{NUM_B,			0x30},
	{NUM_C,			0x2e},
	{NUM_D,			0x20},
	{NUM_E,			0x12},
	{NUM_F,			0x21},
	{NUM_G,			0x22},
	{NUM_H,			0x23},
	{NUM_I,			0x17},
	{NUM_J,			0x24},
	{NUM_K,			0x25},
	{NUM_L,			0x26},
	{NUM_M,			0x32},
	{NUM_N,			0x31},
	{NUM_O,			0x18},
	{NUM_P,			0x19},
	{NUM_Q,			0x10},
	{NUM_R,			0x13},
	{NUM_S,			0x1f},
	{NUM_T,			0x14},
	{NUM_U,			0x16},
	{NUM_V,			0x2f},
	{NUM_W,			0x11},
	{NUM_X,			0x2d},
	{NUM_Y,			0x15},
	{NUM_Z,			0x2c},
	{NUM_1,			0x02},
	{NUM_2,			0x03},
	{NUM_3,			0x04},
	{NUM_4,			0x05},
	{NUM_5,			0x06},
	{NUM_6,			0x07},
	{NUM_7,			0x08},
	{NUM_8,			0x09},
	{NUM_9,			0x0a},
	{NUM_0,			0x0b},
	{NUM_DASH,		0x0c},
	{NUM_EQUALS,		0x0d},
	{NUM_LBRACK,		0x1a},
	{NUM_RBRACK,		0x1b},
	{NUM_SEMICOLON,		0x27},
	{NUM_APOSTROPHE,	0x28},
	{NUM_GRAVE,		0x29},
	{NUM_BACKSLASH,		0x2b},
	{NUM_COMMA,		0x33},
	{NUM_PERIOD,		0x34},
	{NUM_SLASH,		0x35},
	{NUM_LESSGREATER,	0x56},

	{NUM_PAD_0,		0x52},
	{NUM_PAD_1,		0x4f},
	{NUM_PAD_2,		0x50},
	{NUM_PAD_3,		0x51},
	{NUM_PAD_4,		0x4b},
	{NUM_PAD_5,		0x4c},
	{NUM_PAD_6,		0x4d},
	{NUM_PAD_7,		0x47},
	{NUM_PAD_8,		0x48},
	{NUM_PAD_9,		0x49},
	{NUM_PAD_DECIMAL,	0x53},
	{NUM_PAD_SLASH,		98},
	{NUM_PAD_AST,		0x37},
	{NUM_PAD_MINUS,		0x4a},
	{NUM_PAD_PLUS,		0x4e},
	{NUM_PAD_ENTER,		96},

	{NUM_ESC,		0x01},
	{NUM_F1, 		0x3b},
	{NUM_F2, 		0x3c},
	{NUM_F3, 		0x3d},
	{NUM_F4, 		0x3e},
	{NUM_F5, 		0x3f},
	{NUM_F6, 		0x40},
	{NUM_F7, 		0x41},
	{NUM_F8, 		0x42},
	{NUM_F9, 		0x43},
	{NUM_F10,		0x44},
	{NUM_F11,		0x57},
	{NUM_F12,		0x58},

	{NUM_INS,		110},
	{NUM_DEL,		111},
	{NUM_HOME,		102},
	{NUM_END,		107},
	{NUM_PGUP,		104},
	{NUM_PGDN,		109},
	{NUM_UP,		103},
	{NUM_DOWN,		108},
	{NUM_LEFT,		105},
	{NUM_RIGHT,		106},

	{NUM_LWIN,		125},
	{NUM_RWIN,		126},
	{NUM_MENU,		127},

	{NUM_PRTSCR_SYSRQ,	99},
	{NUM_PAUSE_BREAK,	119},
};

/*
 * Try to translate a keycode to a DOSEMU keycode...
 */

static t_keysym dosemu_val(unsigned k)
{
	struct char_set *keyb_charset;
	unsigned char buff[1];
	struct char_set_state keyb_state;
	t_unicode ch;

	unsigned t = KTYP(k), v = KVAL(k);
	t_keysym d;

	d = DKY_VOID;
	if (t >= 14)
		/* NR_TYPES is 14 in the kernel but keyboard.h doesn't
		   give it. Gives the Unicode value ^ 0xf000 */
		return k ^ 0xf000;

	switch(t) {
	case KT_LATIN:
	case KT_LETTER: /* is this correct for all KT_LETTERS? */
		keyb_charset = trconfig.keyb_charset;
		init_charset_state(&keyb_state, keyb_charset);
		buff[0] = v;
		charset_to_unicode(&keyb_state, &ch, buff, 1);
		cleanup_charset_state(&keyb_state);
		d = ch;
		break;

#if 0
	case KT_FN:
		switch(k) {
			/* Function keys are hardcodes so ignored... */
		case K_F1:
		case K_F2:
		case K_F3:
		case K_F4:
		case K_F5:
		case K_F6:
		case K_F7:
		case K_F8:
		case K_F9:
		case K_F10:
		case K_F11:
		case K_F12:
			break;
		case K_FIND:
			/* home */
		case K_INSERT:
		case K_REMOVE:
			/* delete */
		case K_SELECT:
			/* end */
		case K_PGUP:
		case K_PGDN:
		case K_MACRO:
			/* menu */
		case K_HELP:
		case K_DO:
			/* execute */
		case K_PAUSE:
		case K_UNDO:
			break;
		default:
			break;
		}
		break;
#endif


	case KT_SPEC:
		switch(k) {
		case K_HOLE:  d = DKY_VOID; break;
		case K_ENTER: d = DKY_RETURN; break;
		default:      d = DKY_VOID; /* K_CAPS, K_NUM, ... ??? */
		}
		break;

	case KT_PAD:
		switch(k) {
		case K_P0:	d = DKY_PAD_INS; break;
		case K_P1:	d = DKY_PAD_END; break;
		case K_P2:	d = DKY_PAD_DOWN; break;
		case K_P3:	d = DKY_PAD_PGDN; break;
		case K_P4:	d = DKY_PAD_LEFT; break;
		case K_P5:	d = DKY_PAD_CENTER; break;
		case K_P6:	d = DKY_PAD_RIGHT; break;
		case K_P7:	d = DKY_PAD_HOME; break;
		case K_P8:	d = DKY_PAD_UP; break;
		case K_P9:	d = DKY_PAD_PGUP; break;
		case K_PPLUS:	d = DKY_PAD_PLUS; break;
		case K_PMINUS:	d = DKY_PAD_MINUS; break;
		case K_PSTAR:	d = DKY_PAD_AST; break;
		case K_PSLASH:	d = DKY_PAD_SLASH; break;
		case K_PENTER:	d = DKY_PAD_ENTER; break;
		case K_PCOMMA:	d = DKY_PAD_SEPARATOR; break;
		case K_PDOT:	d = DKY_PAD_DECIMAL; break;
		default:	d = DKY_VOID; break;
		}
		break;

	case KT_SHIFT:
		switch(k) {
		case K_SHIFT:	d = DKY_L_SHIFT; break;
		case K_SHIFTL:	d = DKY_L_SHIFT; break;
		case K_SHIFTR:	d = DKY_R_SHIFT; break;
		case K_CTRL:	d = DKY_L_CTRL; break;
		case K_CTRLL:	d = DKY_L_CTRL; break;
		case K_CTRLR:	d = DKY_R_CTRL; break;
		case K_ALT:	d = DKY_L_ALT; break;
		case K_ALTGR:	d = DKY_MODE_SWITCH; break;
		}
		break;
	case KT_META:
		d = DKY_VOID;
		break;

	case KT_DEAD:
		switch(k) {
		case K_DGRAVE: d = DKY_DEAD_GRAVE; break;
		case K_DACUTE: d = DKY_DEAD_ACUTE; break;
		case K_DCIRCM: d = DKY_DEAD_CIRCUMFLEX; break;
		case K_DTILDE: d = DKY_DEAD_TILDE; break;
		case K_DDIERE: d = DKY_DEAD_DIAERESIS; break;
		case K_DCEDIL: d = DKY_DEAD_CEDILLA; break;
		}
		break;

	case KT_LOCK:
		switch(k) {
		case K_ALTGRLOCK: d = DKY_ALTGR_LOCK; break;
		}
		break;

	default:
		break;
	}

	return d;
}

int read_kbd_table(struct keytable_entry *kt,
			  struct keytable_entry *altkt)
{
	int fd, i, j = -1;
	struct kbentry ke;
	int altgr_present, altgr_lock_present;

	fd = getfd();
	if(fd < 0) {
		return 1;
	}

	altgr_present = 0;
	altgr_lock_present = 0;
	for(i = 0; i < sizeof(dosemu_to_kernel)/sizeof(dosemu_to_kernel[0]); i++) {
		unsigned vp, vs, va, vsa, vc;
		t_keysym kp, ks, ka, ksa, kc;
		int kernel, dosemu;
		kernel = dosemu_to_kernel[i].kernel;
		dosemu = dosemu_to_kernel[i].dosemu;
		ke.kb_index = kernel;

		ke.kb_table = 0;
		if ((j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) break;
		vp = ke.kb_value;

		ke.kb_table = 1 << KG_SHIFT;
		if ((j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) break;
		vs = ke.kb_value;

		ke.kb_table = 1 << KG_ALTGR;
		if ((j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) break;
		va = ke.kb_value;

		ke.kb_table = (1 << KG_SHIFT) | (1 << KG_ALTGR);
		if ((j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) break;
		vsa = ke.kb_value;

		ke.kb_table = 1 << KG_CTRL;
		if ((j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) break;
		vc = ke.kb_value;

		kp = dosemu_val(vp);
		ks = dosemu_val(vs);
		ka = dosemu_val(va);
		ksa = dosemu_val(vsa);
		kc = dosemu_val(vc);
		if ((kp == DKY_MODE_SWITCH) || (ks == DKY_MODE_SWITCH) ||
			(ka == DKY_MODE_SWITCH) || (kc == DKY_MODE_SWITCH)) {
			k_printf("mode_switch\n");
			altgr_present = 1;
		}
		if ((kp == DKY_ALTGR_LOCK) || (ks == DKY_ALTGR_LOCK) ||
			(ka == DKY_ALTGR_LOCK) || (kc == DKY_ALTGR_LOCK)) {
			k_printf("altgr lock\n");
			altgr_lock_present = 1;
			continue;
		}
		if (ka == kp) {
			ka = U_VOID;
		}
		/* Only allow control characters in the ctrl plane */
		if ((kc > 0x1f) && (kc != 0x7f)) {
			kc = U_VOID;
		}
		/* As a special case filter [ctrl][tab] */
		if ((dosemu == NUM_TAB) && (kc == 0x09)) {
			kc = U_VOID;
		}
		if (kp != U_VOID) kt->key_map[dosemu]   = kp;
		if (ks != U_VOID) kt->shift_map[dosemu] = ks;
		if (ka != U_VOID) kt->alt_map[dosemu]   = ka;
		if (kc != U_VOID) kt->ctrl_map[dosemu]  = kc;
		if (ksa != U_VOID) kt->shift_alt_map[dosemu] = ksa;
#if 0
		printf("%02x: ", dosemu);
		printf("p: %04x->%-6s ", vp, pretty_keysym(kp));
		printf("s: %04x->%-6s ", vs, pretty_keysym(ks));
		printf("a: %04x->%-6s ", va, pretty_keysym(ka));
		printf("c: %04x->%-6s ", vc, pretty_keysym(kc));
		printf("\n");
#endif
	}
	if (altgr_lock_present) {
		altkt->name = "alt auto";
		altkt->key_map = kt->alt_map;
		altkt->alt_map = kt->key_map;
		altkt->shift_map = kt->shift_alt_map;
		altkt->shift_alt_map = kt->shift_map;
	}
	if (!altgr_present) {
		for(i = 0; i < kt->sizemap; i++) {
			kt->alt_map[i] = U_VOID;
		}
	}

	/* look for numpad ',' or '.' */
	ke.kb_index = 83;
	ke.kb_table = 0;
	if(!j && !(j = ioctl(fd, KDGKBENT, (unsigned long) &ke))) {
		if(ke.kb_value == K_PDOT) kt->num_table[12] = '.';
	}

	if(fd > 2) close(fd);

	return j;
}

#else
int open_console(void) { return -1; }
int getfd(void) { return -1; }
int read_kbd_table(struct keytable_entry *kt,
			  struct keytable_entry *altkt) { return -1; }
#endif
