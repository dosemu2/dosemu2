/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "config.h"

#include <limits.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "emu.h"
#include "keyboard.h"
#include "keymaps.h"
#include "translate.h"
#include "keyb_X.h"

static t_unicode keysym_to_unicode(t_unicode ch)
{
	struct char_set *keyb_charset;
	unsigned char buff[1];
	size_t result;
	struct char_set_state keyb_state;

	/* 0xef00 - 0xefff is a pass through range to the current
	 * character set.
	 */
	if ((ch < 0xef00) || (ch > 0xefff))
		return ch;

	keyb_charset = trconfig.keyb_config_charset;
	init_charset_state(&keyb_state, keyb_charset);
	buff[0] = ch & 0xFF;
	result = charset_to_unicode(&keyb_state, &ch, buff, 1);
	cleanup_charset_state(&keyb_state);
	return ch;
}

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
  unsigned match, mismatch, seq, i, syms, startsym, alternate;
  int score, keyc, key, pkey, ok = 0;
  KeySym keysym;
  unsigned max_seq[3] = {0, 0};
  int max_score[3] = {INT_MIN, INT_MIN};
  int ismatch = 0;
  int min_keycode, max_keycode;
  t_unicode ckey[4] = {0, 0, 0, 0};
  t_keysym lkey[4] = {0, 0, 0, 0};
  struct keytable_entry *kt;
  struct char_set_state X_charset;

  char *display_name = config.X_display ? config.X_display : getenv("DISPLAY");
  display = XOpenDisplay(display_name);
  if (display == NULL) return 1;

  XDisplayKeycodes(display, &min_keycode, &max_keycode);
  /* We are only interested in keysyms_per_keycode.
     There is no need to hold a local copy of the keysyms table */
  XFree(XGetKeyboardMapping(display, min_keycode,
			    max_keycode + 1 - min_keycode, &syms));
  if (syms > 4) {
    k_printf("%d keysyms per keycode not supported, set to 4\n", syms);
    syms = 4;
  }

  init_charset_state(&X_charset, lookup_charset("X_keysym"));
  for (kt = keytable_list, alternate = 0; kt->name; ) {
    k_printf("Attempting to match against \"%s\"\n", kt->name);
    startsym = alternate << 1;
    match = 0;
    mismatch = 0;
    score = 0;
    seq = 0;
    pkey = -1;
    for (keyc = min_keycode; keyc <= max_keycode; keyc++) {
      /* get data for keycode from X server */
      for (i = startsym; i < syms; i++) {
        keysym = XKeycodeToKeysym (display, keyc, i);
	charset_to_unicode(&X_charset, &ckey[i - startsym],
                (const char *)&keysym, sizeof(keysym));
      }
      for (i = 0; i < startsym; i++)
	ckey[syms - startsym + i] = U_VOID;
      if (ckey[0] != U_VOID && (ckey[0] & 0xf000) != 0xe000) {
        /* search for a match in layout table */
        /* right now, we just find an absolute match for defined positions */
        /* (undefined positions are ignored, so if it's defined as "3#" in */
        /* the table, it's okay that the X server has "3#£", for example) */
        /* however, the score will be higher for longer matches */
        for (key = 0; key < kt->sizemap; key++) {
	  lkey[0] = keysym_to_unicode(kt->key_map[key]);
	  lkey[1] = keysym_to_unicode(kt->shift_map[key]);
	  lkey[2] = keysym_to_unicode(kt->alt_map[key]);
	  lkey[3] = U_VOID;
          for (ok = 0, i = 0; (ok >= 0) && (i < syms); i++) {
            if (lkey[i] != U_VOID) {
	      if (lkey[i] == ckey[i])
		ok++;
	      else if (ckey[i] != U_VOID)
		ok = -1;
	    }
          }
	  if (debug_level('k') > 5)
	    k_printf("key: %d score %d for keycode %d, %x %x %x, "
		     "got %x %x %x %x\n",
		     key, ok, keyc, lkey[0], lkey[1], lkey[2], 
		     ckey[0], ckey[1], ckey[2], ckey[3]);
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
          for (i = 0; i < sizeof(ckey); i++) if (!ckey[i]) ckey[i] = ' ';
          mismatch++;
          score -= syms;
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
