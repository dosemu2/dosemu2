/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
This file contains X keyboard tables and handling routines
for dosemu. 
  exports:  X_process_key(XKeyEvent *)
            X_process_keys(XKmapmapEvent *);
  uses:     putkey(t_scancode scan, Boolean make, uchar charcode)
    
******************************************************************
  
Part of the original code in this file was taken from pcemu written by David Hedley 
(hedley@cs.bris.ac.uk).

Since this code has been totally rewritten the pcemu license no longer applies
***********************************************************************
*/

#include "config.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#if HAVE_XKB
#include <X11/XKBlib.h>
#endif

#include "X.h"
#include "emu.h"
#include "keyb_clients.h"
#include "keyboard.h"
#include "video.h"
#include "translate.h"
#include "keysym_attributes.h"
#include "keyb_X.h"
#include "translate.h"

struct modifier_info X_mi;
static struct char_set_state X_charset;

static int get_modifier_mask(XModifierKeymap *map, int keycode)
{
	int i;
	KeyCode *kcp;
	kcp = map->modifiermap;
	for(i = 0; i < 8; i++) {
		int j;
		for(j = 0; j < map->max_keypermod; j++, kcp++) {
			if (*kcp == 0) 
				continue;
			if (*kcp == keycode)
				return 1 << i;
		}
	}
	return 0;
}

static void X_modifier_info_init(Display *display)
{
	XModifierKeymap *map;

	/* Initialize the modifier info */
	/* Only the CapsLockMask is precomputed */
	X_mi.CapsLockMask = LockMask;
	X_mi.CapsLockKeycode = XKeysymToKeycode(display, XK_Caps_Lock);
	X_mi.NumLockMask = 0; 
	X_mi.NumLockKeycode = XKeysymToKeycode(display, XK_Num_Lock);
	X_mi.ScrollLockMask = 0; 
	X_mi.ScrollLockKeycode = XKeysymToKeycode(display, XK_Scroll_Lock);
	X_mi.AltMask = 0;
	X_mi.AltGrMask = 0;
	X_mi.InsLockMask = 0;
	

	map = XGetModifierMapping(display);

	X_mi.NumLockMask = get_modifier_mask(map, X_mi.NumLockKeycode);
	X_mi.ScrollLockMask = get_modifier_mask(map, X_mi.ScrollLockKeycode);

	if (!X_mi.AltMask) {
		X_mi.AltMask = get_modifier_mask(map, XKeysymToKeycode(display, XK_Alt_L));
	}
	if (!X_mi.AltMask) {
		X_mi.AltMask = get_modifier_mask(map, XKeysymToKeycode(display, XK_Alt_R));
	}
	if (!X_mi.AltMask) {
		X_mi.AltMask = get_modifier_mask(map, XKeysymToKeycode(display, XK_Meta_L));
	}
	if (!X_mi.AltMask) {
		X_mi.AltMask = get_modifier_mask(map, XKeysymToKeycode(display, XK_Meta_R));
	}
	if (!X_mi.AltGrMask) {
		X_mi.AltGrMask = get_modifier_mask(map, XKeysymToKeycode(display, XK_Mode_switch));
	}
	if (!X_mi.AltGrMask) {
		X_mi.AltGrMask = get_modifier_mask(map, XKeysymToKeycode(display, XK_Multi_key));
	}
	if (!X_mi.InsLockMask) {
		X_mi.InsLockMask = get_modifier_mask(map, XKeysymToKeycode(display, XK_Insert));
	}
	if (!X_mi.InsLockMask) {
		X_mi.InsLockMask = get_modifier_mask(map, XKeysymToKeycode(display, XK_KP_Insert));
	}
	

	X_printf("X: CapsLockMask = 0x%x\n", X_mi.CapsLockMask);
	X_printf("X: CapsLockKeycode = 0x%x\n", X_mi.CapsLockKeycode);
	X_printf("X: NumLockMask = 0x%x\n", X_mi.NumLockMask);
	X_printf("X: NumLockKeycode = 0x%x\n", X_mi.NumLockKeycode);
	X_printf("X: ScrollLockMask = 0x%x\n", X_mi.ScrollLockMask);
	X_printf("X: ScrollLockKeycode = 0x%x\n", X_mi.ScrollLockKeycode);
	X_printf("X: AltMask = 0x%x\n", X_mi.AltMask);
	X_printf("X: AltGrMask = 0x%x\n", X_mi.AltGrMask);
	X_printf("X: InsLockMask = 0x%x\n", X_mi.InsLockMask);

	XFreeModifiermap(map);
}

static void keyb_X_init(Display *display)
{
	X_modifier_info_init(display);
	init_charset_state(&X_charset, lookup_charset("X_keysym"));
}

static int use_move_key(t_keysym key)
{
	int result = FALSE;
	/* If it's some kind of function key move it
	 * otherwise just make sure it gets pressed
	 */
	if (is_keysym_function(key) ||
	    is_keysym_dosemu_key(key) ||
	    is_keypad_keysym(key) ||
	    (key == KEY_TAB) ||
	    (key == KEY_RETURN) ||
	    (key == KEY_BKSP)) {
		result = TRUE;
	}
	return result;
}

static t_modifiers map_X_modifiers(unsigned int e_state)
{
	t_modifiers modifiers = 0;
	if (e_state & ShiftMask) {
		modifiers |= MODIFIER_SHIFT;
	}
	if (e_state & ControlMask) {
		modifiers |= MODIFIER_CTRL;
	}
	if (e_state & X_mi.AltMask) {
		modifiers |= MODIFIER_ALT;
	}
	if (e_state & X_mi.AltGrMask) {
		modifiers |= MODIFIER_ALTGR;
	}
	if (e_state & X_mi.CapsLockMask) {
		modifiers |= MODIFIER_CAPS;
	}
	if (e_state & X_mi.NumLockMask) {
		modifiers |= MODIFIER_NUM;
	}
	if (e_state & X_mi.ScrollLockMask) {
		modifiers |= MODIFIER_SCR;
	}
	if (e_state & X_mi.InsLockMask) {
		modifiers |= MODIFIER_INS;
	}
	return modifiers;
}

void X_sync_shiftstate(Boolean make, KeyCode kc, unsigned int e_state)
{
	t_modifiers shiftstate = get_shiftstate();

	/* caps&num&scroll lock are special in the core X protocol: press/release means
	   turning lock on/off, not pressing/releasing the key.
	   With XKB they are no longer special.  To handle it correclty every time,
	   and to make the code simple we just sync up the shiftstates before the
	   keys are pressed.
	   Note:
	   The reason we don't generate extra internal key events when doing this
	   is that for shift keys, X doesn't report the new shift state until
	   the next key event.  So unless gets off somewhere (missed key events
	   or the strange event handling for the core X protocol, this code will
	   never have anything to do).
	   Note2:
	   This does depend upon Numlock, CapsLock, & co all being bound to
	   X modifiers, but this assumption is rarely violated.
	   Note3:
	   For NumLock & CapsLock the state doesn't change until after the release
	   event, for the disable.  So we don't sync on the release event to prevent
	   unnecessary key events from being sent.
	*/
	
	/* Check for modifiers released outside this window */
	if (!!(shiftstate & MODIFIER_SHIFT) != !!(e_state & ShiftMask)) {
		shiftstate ^= MODIFIER_SHIFT;
	}
	if (!!(shiftstate & MODIFIER_CTRL) != !!(e_state & ControlMask)) {
		shiftstate ^= MODIFIER_CTRL;
	}
	if (X_mi.AltMask && (
		!!(shiftstate & MODIFIER_ALT) != !!(e_state & X_mi.AltMask))) {
		shiftstate ^= MODIFIER_ALT;
	}

	if (!config.X_keycode) {
	/* NOTE: AltGr usually performs a layout switching, so we don't touch
	 * it here for X_keycode
	 */
	  if (X_mi.AltGrMask && (
		!!(shiftstate & MODIFIER_ALTGR) != !!(e_state & X_mi.AltGrMask))) {
		shiftstate ^= MODIFIER_ALTGR;
	  }
	}

	if (X_mi.CapsLockMask && 
	    (!!(shiftstate & MODIFIER_CAPS) != !!(e_state & X_mi.CapsLockMask))
		&& (make || (kc != X_mi.CapsLockKeycode))) {
		shiftstate ^= MODIFIER_CAPS;
	}
	if (X_mi.NumLockMask &&
	    (!!(shiftstate & MODIFIER_NUM) != !!(e_state & X_mi.NumLockMask))
		&& (make || (kc != X_mi.NumLockKeycode))) {
		shiftstate ^= MODIFIER_NUM;
	}
	if (X_mi.ScrollLockMask &&
	    (!!(shiftstate & MODIFIER_SCR) != !!(e_state & X_mi.ScrollLockMask))
		&& (make || (kc != X_mi.ScrollLockKeycode))) {
		shiftstate ^= MODIFIER_SCR;
	}
	if (X_mi.InsLockMask && (
		!!(shiftstate & MODIFIER_INS) != !!(e_state & X_mi.InsLockMask))) {
		shiftstate ^= MODIFIER_INS;
	}
	set_shiftstate(shiftstate);
}

static int initialized= 0;
void X_process_keys(XKeymapEvent *e)
{
	if (!initialized) {
		keyb_X_init(display);
		initialized = 1;
	}
	if (config.X_keycode) {
		X_keycode_process_keys(e);
		return;
	}
}

void map_X_event(Display *display, XKeyEvent *e, struct mapped_X_event *result)
{
	KeySym xkey;
	unsigned int modifiers;
	/* modifiers should be set to the (currently active) modifiers that were not used
	 * to generate the X KeySym.  This allows for cases like Ctrl-A to be generate
	 * the correct codes for dos, even though X does not for explicit support for
	 * that combination.
	 */

	if (!USING_XKB) {
		#define MAXCHARS 3
		unsigned char chars[MAXCHARS];
		int count;
		static XComposeStatus compose_status = {NULL, 0};

		count = XLookupString(e, chars, MAXCHARS, &xkey, &compose_status);
		/* Guess all modifiers were not used in generating the symbol.
		 * If the dos keyboard layout and the X keyboard layout are
		 * the same this has no ill effects.  If the layouts are
		 * different this is only wrong half the time.
		 * Using XKB we can do better.
		 */
		modifiers = e->state;
	}
#if HAVE_XKB
	else {
		xkey = XK_VoidSymbol;
		modifiers = 0; /* get set to the modifers to clear... */

		XkbLookupKeySym(display, e->keycode, e->state, &modifiers, &xkey);
		modifiers = e->state & (~modifiers);
	}
#endif
	charset_to_unicode(&X_charset, &result->key, 
		(const char *)&xkey, sizeof(xkey));
	result->make = (e->type == KeyPress);
	result->modifiers = map_X_modifiers(modifiers);
	X_printf("X: key_event: %02x %08x %8s sym: %04x -> %04x %08x\n",
		e->keycode,
		e->state,
		result->make?"pressed":"released",
		(unsigned)xkey,
		result->key,
		result->modifiers);
}

void X_process_key(XKeyEvent *e)
{
	struct mapped_X_event event;
	
	if (!initialized) {
		keyb_X_init(display);
		initialized = 1;
	}
	if (config.X_keycode) {
		X_keycode_process_key(e);
		return;
	}
	map_X_event(display, e, &event);

	X_sync_shiftstate(event.make, e->keycode, e->state);
	/* If the key is just a ``function'' key just wiggle the key
	 * with the appropriate keysym in the dos keyboard layout.
	 * This allows for full interpretation of currently active modifiers.
	 *
	 * Otherwise if the key can host multiple keysyms and is not fixed,
	 * make an educated guess on how to press the key.  This allows
	 * much more limited freedom for interpreting the modifiers.
	 */
	if (!use_move_key(event.key) || (move_key(event.make, event.key) < 0)) {
		put_modified_symbol(event.make, event.modifiers, event.key);
	}
}

static int probe_X_keyb(void)
{
	int result = FALSE;
	if (Video == &Video_X) {
		result = TRUE;
	}
	return result;
}

struct keyboard_client Keyboard_X =  {
	"X11",			/* name */
	probe_X_keyb,		/* probe */
	NULL,			/* init */
	NULL,			/* reset */
	NULL,			/* close */
	NULL,			/* run */       /* the X11 event handler is run seperately */
	NULL,			/* set_leds */
};
