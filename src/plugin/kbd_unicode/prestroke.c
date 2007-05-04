/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* This is file prestroke.c
 *
 * (C) 1997 under GPL, Hans Lermen <lermen@fgan.de>
 * (put under DOSEMU-policy 1998, -Hans)
 */

#include <stdlib.h>
#include <string.h>
#include "emu.h"
#include "keyboard.h"
#include "translate.h"
#include "video.h" /* for charset defines */

static int default_stroke_pause = -1;
static int stroke_pause;


#define GETNUMBER(s) ({ \
  int ret; t_unicode *end; \
  ret = unicode_to_long(s, &end, 0); \
  s = end; \
  if (*s) s++; \
  ret; \
})

static t_unicode *type_one_key(t_unicode *in)
{
	int esc, keynum;
	t_unicode ch;
	t_unicode keysym;
	static char ctrl[]="JMGHLIK";

	stroke_pause = default_stroke_pause;
	if (!in || !in[0]) return 0;
	
	ch=*(in++);
	switch(ch) {
	case '\\': {
		ch=(*in++);
		if (!ch) return 0;
		esc = -1;
		switch(ch) {
		case 'v': esc++;
		case 't': esc++;
		case 'f': esc++;
		case 'b': esc++;
		case 'a': esc++;
		case 'r': esc++;
		case 'n': esc++;
		case '^': { /* example:  \a  == \^G  == <Ctrl>G
			       		 \r  == \^M  == <Ctrl>M == <ENTER> */
			if (esc >= 0) 
				ch = ctrl[esc];
			else
				ch = *(in++);
			keysym = ch;
			if (keysym != KEY_VOID) {
				if (keysym == KEY_M) {
					put_symbol(PRESS, KEY_RETURN);
					put_symbol(RELEASE, KEY_RETURN);
				} else {
					put_modified_symbol(PRESS, MODIFIER_CTRL, keysym);
					put_modified_symbol(RELEASE, MODIFIER_CTRL, keysym);
				}
			}
			return in;
		}
		case 'A': { /* example: \Az == <Alt>z */
			ch = *(in++);
			keysym = ch;
			if (keysym != KEY_VOID) {
				put_modified_symbol(PRESS, MODIFIER_ALT, keysym);
				put_modified_symbol(RELEASE, MODIFIER_ALT, keysym);
			}
			return in;
		}
        	case 'F': { /* example:  \F12;  == key F12
                	                 \F1;   == key F1 */
			keynum = GETNUMBER(in);
			if ((keynum > 0) && (keynum < 12)) {
				keysym = KEY_F1 + keynum -1;
				put_symbol(PRESS, keysym);	/* key pressed */
				put_symbol(RELEASE, keysym);	/* key released */
			}
			return in;
		}
		case 'p': { /* example:  \p100; == pause one second */
			keynum = GETNUMBER(in);
			if ((keynum > 0) && (keynum < 10000)) {
				stroke_pause = keynum;
			}
			return in;
		}
		case 'P': { /* example:  \P15; == set rate to 100/15 cps */
			keynum = GETNUMBER(in);
			if (keynum > 0) {
				keynum--;
				if (keynum > 100) keynum = 100;
				default_stroke_pause = keynum;
			}
			return in;
		}
		case 'M': { /* 'M' ==  as in Move, 
                       ('P' would be nicer (Pu for PageUp) but is already used...)
                       example: \Mh  == <HOME>   \Mu  == <PageUp>
                                \Me  == <END>    \Md  == <PageDown>
                                \M8  == <ArrowUp>
                                \M4  == <ArrowLeft>  \M6  == <ArrowRight>
                                \M2  == <ArrowDown>
                       */
			ch=*(in++);
			switch(ch) {
			case 'i': keynum = KEY_INS;  break;  /* KEY_INS */
			case 'h': keynum = KEY_HOME; break;  /* KEY_HOME */
			case 'u': keynum = KEY_PGUP; break;  /* KEY_PGUP */
			case 'c': keynum = KEY_DEL;  break;  /* KEY_DEL */
			case 'e': keynum = KEY_END;  break;  /* KEY_END */
			case 'd': keynum = KEY_PGDN; break;  /* KEY_PGDN */
			case '8': keynum = KEY_UP;   break;  /* KEY_UP */
			case '4': keynum = KEY_LEFT; break;  /* KEY_LEFT */
			case '6': keynum = KEY_RIGHT; break;  /* KEY_RIGHT */
			case '2':
			default:  keynum = KEY_DOWN; break;  /* KEY_DOWN */
			}
			move_key(PRESS, keynum);
			move_key(RELEASE, keynum);
			return in;
		}
		default:
                        ;
			/* fall through */
		}
	}
	default: {
		keysym = ch;
		if (keysym != KEY_VOID) {
			put_symbol(PRESS, keysym);	/* key pressed */
			put_symbol(RELEASE, keysym);	/* key release */
		}
		return in;
	}
	}
	return in;
}

static t_unicode *pre_stroke = 0, *pre_stroke_mem = 0;
int type_in_pre_strokes(void)
{
	struct char_set *keyb_charset = trconfig.keyb_charset;
	if (config.pre_stroke && !pre_stroke) {
		size_t characters, src_len;
		const char *ptr;
		struct char_set_state state;
		
		init_charset_state(&state, keyb_charset);
		src_len = strlen(config.pre_stroke) +1;

		ptr = config.pre_stroke;
		characters = character_count(&state, ptr, src_len);

		pre_stroke_mem = pre_stroke = 
			malloc(sizeof(t_unicode) * (characters +1));
		charset_to_unicode_string(&state, pre_stroke, &ptr, src_len);

		cleanup_charset_state(&state);
	}
	if (pre_stroke) {
		pre_stroke = type_one_key(pre_stroke);
		if (!pre_stroke) {
			free(config.pre_stroke);
			free(pre_stroke_mem);
			config.pre_stroke = 0;
		}
	}
	return stroke_pause;
}



void append_pre_strokes(unsigned char *s)
{
  if (config.pre_stroke) {
    int l1,l2;
    unsigned char *n;

    l1 = strlen(config.pre_stroke);
    l2 = strlen(s);
    n = realloc(config.pre_stroke, l1+l2+1);
    if (!n) return;
    memcpy(n+l1, s, l2+1);
    config.pre_stroke = n;
  }
  else {
    config.pre_stroke = strdup(s);
  }
}
