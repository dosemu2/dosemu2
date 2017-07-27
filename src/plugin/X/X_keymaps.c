/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "config.h"

#include <limits.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#include "emu.h"
#include "utilities.h"
#include "keyboard/keyboard.h"
#include "keyboard/keymaps.h"
#include "translate/translate.h"
#include "keyb_X.h"

static t_unicode keysym_to_unicode(t_unicode ch)
{
	struct char_set *keyb_charset;
	unsigned char buff[1];
	struct char_set_state keyb_state;

	/* 0xef00 - 0xefff is a pass through range to the current
	 * character set.
	 */
	if ((ch < 0xef00) || (ch > 0xefff))
		return ch;

	keyb_charset = trconfig.keyb_config_charset;
	init_charset_state(&keyb_state, keyb_charset);
	buff[0] = ch & 0xFF;
	charset_to_unicode(&keyb_state, &ch, buff, 1);
	cleanup_charset_state(&keyb_state);
	return ch;
}

/*
 * This is an analogue to XkbKeycodeToKeysym() but uses the data from
 * XGetKeyboardMapping() rather than Xkb
 */
#ifndef HAVE_XKB
static KeySym X11_KeycodeToKeysym(KeySym *map, int width, int min, int keycode, int group, int index) {
	int offset;

	if(group == 0) {	// primary
		offset = ((keycode - min) * width) + index;
	} else {		// plain X only supports a single alternate group
		offset = ((keycode - min) * width) + 2 + index;
	}
	return map[offset];
}
#endif

/* This function is borrowed from Wine (LGPL'ed)
   http://source.winehq.org/source/dlls/x11drv/keyboard.c
   with adjustments to match dosemu

   The idea is, if $_layout="auto", to match each keyboard map with
   the X keymap and choose the one that matches best.

   It is used when we can access display, not just for xdosemu,
   but also in xterms. Remote users using terminals will have
   to specify $_layout explicitly though.

   The console map is just another map in this scheme that may
   or may not be the best one.
*/
int X11_DetectLayout (void)
{
  Display *display;
  unsigned match, mismatch, seq, i, alternate;
  int score, keyc, key, pkey, ok = 0;
  KeySym keysym;
  unsigned max_seq[3] = {0, 0};
  int max_score[3] = {INT_MIN, INT_MIN};
  int ismatch = 0;
  int min_keycode, max_keycode;
  t_unicode ckey[2] = {0, 0};
  t_keysym lkey[2] = {0, 0};
  struct keytable_entry *kt;
  struct char_set_state X_charset;

  char *display_name = config.X_display ? config.X_display : getenv("DISPLAY");
  display = XOpenDisplay(display_name);
  if (display == NULL) return 1;

  XDisplayKeycodes(display, &min_keycode, &max_keycode);

#ifndef HAVE_XKB
  int keysyms_per_keycode;
  KeySym *key_mapping;

  /* get data for keycode from X server */
  key_mapping = XGetKeyboardMapping(display, min_keycode,
			    max_keycode + 1 - min_keycode,
			    &keysyms_per_keycode);
#endif

  init_charset_state(&X_charset, lookup_charset("X_keysym"));
  for (kt = keytable_list, alternate = 0; kt->name; ) {
    k_printf("Attempting to match against \"%s\"\n", kt->name);
    match = 0;
    mismatch = 0;
    score = 0;
    seq = 0;
    pkey = -1;
    for (keyc = min_keycode; keyc <= max_keycode; keyc++) {

      for (i = 0; i < 2; i++) {
#ifdef HAVE_XKB
	keysym = XkbKeycodeToKeysym(display, keyc, alternate, i);
#else
	keysym = X11_KeycodeToKeysym(key_mapping, keysyms_per_keycode,
			min_keycode, keyc, alternate, i);
#endif
	charset_to_unicode(&X_charset, &ckey[i],
                (const unsigned char *)&keysym, sizeof(keysym));
      }

      if (ckey[0] != U_VOID && (ckey[0] & 0xf000) != 0xe000) {
        /* search for a match in layout table */
        /* right now, we just find an absolute match for defined positions */
        /* (undefined positions are ignored, so if it's defined as "3#" in */
        /* the table, it's okay that the X server has "3#£", for example) */
        /* however, the score will be higher for longer matches */
        for (key = 0; key < kt->sizemap; key++) {
	  lkey[0] = keysym_to_unicode(kt->key_map[key]);
	  lkey[1] = keysym_to_unicode(kt->shift_map[key]);
          for (ok = 0, i = 0; (ok >= 0) && (i < 2); i++) {
            if (lkey[i] != U_VOID) {
	      if (lkey[i] == ckey[i])
		ok++;
	      else if (ckey[i] != U_VOID)
		ok = -1;
	    }
          }
	  if (debug_level('k') > 5)
	    k_printf("key: % 3d score % 2d for keycode % 3d, %04x %04x, "
		     "got %04x %04x\n",
		     key, ok, keyc, lkey[0], lkey[1], ckey[0], ckey[1]);
          if (ok > 0) {
            score += ok;
            break;
          }
        }
        /* count the matches and mismatches */
        if (ok > 0) {
          match++;
          /* and how much the keycode order matches */
          if (key > pkey) seq++;
          pkey = key;
        } else {
          /* print spaces instead of \0's */
          for (i = 0; i < ARRAY_SIZE(ckey); i++) if (!ckey[i]) ckey[i] = ' ';
          mismatch++;
          score -= 2;
        }
      }
    }
    k_printf("matches=%d, mismatches=%d, seq=%d, score=%d\n",
           match, mismatch, seq, score);
    if (score > max_score[alternate] ||
       (score == max_score[alternate] &&
	((seq > max_seq[alternate]) ||
	 (seq == max_seq[alternate] && kt->keyboard == KEYB_AUTO)))) {
      /* best match so far */
      if (alternate) {
	/* alternate keyboards are optional so a threshold is used */
	if (score > 20) config.altkeytable = kt;
      }
      else
	config.keytable = kt;
      max_score[alternate] = score;
      max_seq[alternate] = seq;
      ismatch = !mismatch;
    }
    alternate = !alternate;
    if (!alternate)
      kt++;
  }
  cleanup_charset_state(&X_charset);

#ifndef HAVE_XKB
  XFree(key_mapping);
#endif

  /* we're done, report results if necessary */
  if (!ismatch)
    k_printf("Using closest match (%s) for scan/virtual codes mapping.\n",
	   config.keytable->name);

  c_printf("CONF: detected layout is \"%s\"\n", config.keytable->name);
  if (config.altkeytable)
    c_printf("CONF: detected alternate layout: %s\n", config.altkeytable->name);
  XCloseDisplay(display);
  return 0;
}
