/*
This file contains X keyboard tables and handling routines
for dosemu. 
  exports:  X_process_key(XKeyEvent *)
  uses:     putkey(t_scancode scan, Boolean make, uchar charcode)
    
******************************************************************
  
Part of this code is taken from pcemu written by David Hedley 
(hedley@cs.bris.ac.uk) and is

  Copyright (C) 1994 University of Bristol, England

Permission is granted to use, copy, modify, and distribute this
software and its documentation for any non-commercial purpose,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in the
supporting documentation.

BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT
WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER
PARTIES PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
PROGRAM IS WITH YOU.  SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME
THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION.

IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR
DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL
DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE PROGRAM
(INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING RENDERED
INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A FAILURE OF
THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH HOLDER
OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

***********************************************************************
*/

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "emu.h"
#include "keyb_clients.h"
#include "keyboard.h"


#define AltMask Mod1Mask
#define XK_X386_SysReq 0x1007FF00

/* note: ascii_keys[] and latin1_to_dos[] are now defined in keyb_clients.c 
 * (though still used in this file)
 */

static const t_keysym cursor_keys[] = 
{
    KEY_HOME, KEY_LEFT, KEY_UP, KEY_RIGHT, 
    KEY_DOWN, KEY_PGUP, KEY_PGDN, KEY_END
};

#define CURSOR_FIRST XK_Home
#define CURSOR_LAST  XK_End

static const t_keysym keypad_keys[] = 
{
    KEY_PAD_AST,   KEY_PAD_PLUS, KEY_COMMA, KEY_PAD_MINUS,
    KEY_PAD_DECIMAL, KEY_PAD_SLASH, 
    KEY_PAD_0, KEY_PAD_1, KEY_PAD_2, KEY_PAD_3, KEY_PAD_4, KEY_PAD_5,
    KEY_PAD_6, KEY_PAD_7, KEY_PAD_8, KEY_PAD_9
};

#define KEYPAD_FIRST XK_KP_Multiply
#define KEYPAD_LAST  XK_KP_9

static const struct 
{
    KeySym xkey;
    t_keysym dosemu_key;
} other_keys[] =
{
    { XK_Return,        KEY_RETURN },
    { XK_Escape,        KEY_ESC },
    { XK_Insert,        KEY_INS },
    { XK_Delete,        KEY_DEL },

    { XK_KP_Enter,      KEY_PAD_ENTER },

    { XK_Shift_L,       KEY_L_SHIFT },
    { XK_Shift_R,       KEY_R_SHIFT },
    { XK_Control_L,     KEY_L_CTRL },
    { XK_Control_R,     KEY_R_CTRL },
    { XK_Meta_L,        KEY_L_ALT },
    { XK_Alt_L,         KEY_L_ALT },
    { XK_Meta_R,        KEY_R_ALT },
    { XK_Alt_R,         KEY_R_ALT },
    { XK_Mode_switch,   KEY_R_ALT },

    { XK_Scroll_Lock,   KEY_SCROLL },
    { XK_Break,         KEY_BREAK },
    { XK_X386_SysReq,   KEY_SYSRQ },

	/* These are what my X Server sends when numlock is off for the
	 * keypad keys. -- EB 22 Oct 1996 
	 */

    { XK_KP_Home,       KEY_PAD_7 },
    { XK_KP_Left,       KEY_PAD_4 },
    { XK_KP_Up,         KEY_PAD_8 },
    { XK_KP_Right,      KEY_PAD_6 },
    { XK_KP_Down,       KEY_PAD_2 },
    { XK_KP_Page_Up,    KEY_PAD_9 },
    { XK_KP_Page_Down,  KEY_PAD_3 },
    { XK_KP_End,        KEY_PAD_1 },
    { XK_KP_Begin,      KEY_PAD_5 },
    { XK_KP_Insert,     KEY_PAD_0 },
    { XK_KP_Delete,     KEY_PAD_DECIMAL },
};

#define NUM_OTHER (sizeof(other_keys)/sizeof(other_keys[0]))



#define MAXCHARS 3

void X_process_key(XKeyEvent *e)
{
   int    key;
   KeySym xkey;
   u_char chars[MAXCHARS];
   int count;
   short ch;
   static XComposeStatus compose_status = {NULL, 0};
   int i;
    
   count = XLookupString(e, chars, MAXCHARS, &xkey, &compose_status);
   ch = count? chars[0]:0;
   X_printf("xkey %x %s, state=%08x, char='%c'\n",
            (int)xkey,(e->type==KeyPress)?"pressed":"released",
            e->state,ch);

   key = 0;

   switch(xkey) {

   /* caps&num lock are special in X: press/release means
      turning lock on/off, not pressing/releasing the key.
      Therefore we have to generate both make & break code
      here.
   */

    case XK_Caps_Lock:
       	  presskey(KEY_CAPS,0);
       	  releasekey(KEY_CAPS);
       	  return;

    case XK_Num_Lock:
          presskey(KEY_NUM,0);
          releasekey(KEY_NUM);
          return;

    case 0x20 ... 0x7e:                     /* ascii keys */
          key = ascii_keys[xkey - 0x20];
          break;

    case XK_F13 ... XK_F22:
          xkey-=12;
          /* fall through */

    case XK_F1 ... XK_F10:
          key = (xkey-XK_F1)+KEY_F1;
          break;

    case XK_F11:
    case XK_F23:
          key = KEY_F11;
          break;

    case XK_F12:
    case XK_F24:
          key = KEY_F12;
          break;

    case CURSOR_FIRST ... CURSOR_LAST:
          key = cursor_keys[xkey-CURSOR_FIRST];
          break;
       
    case KEYPAD_FIRST ... KEYPAD_LAST:
          key = keypad_keys[xkey-KEYPAD_FIRST];
          break;

    case XK_Print:
          if (e->state & AltMask)
             key = KEY_SYSRQ;
          else
             key = KEY_PRTSCR;
          break;

    case XK_Pause:
          if (!(e->state&ControlMask))
             key = KEY_PAUSE;
          else
             key = KEY_BREAK;
          break;
          
    case XK_BackSpace:
          key = KEY_BKSP;
          ch = 8;
          break;

#if 0
    case XK_Delete:
          key = KEY_DEL;
          ch = 0;
          break;
#endif

    case XK_Return:
          key = KEY_RETURN;
          ch = (e->state & ControlMask) ? 0x0a : 0x0d;
          break;

    case XK_Tab:
          key = KEY_TAB;
          if ((e->state & (ShiftMask | ControlMask | AltMask)))
             ch = 0;
          break;

    default:
          for (i = 0; i < NUM_OTHER; i++)
             if (other_keys[i].xkey == xkey)
                key = other_keys[i].dosemu_key;

   }

   /* do latin1 translation */
   if (ch>=0xa0) ch=latin1_to_dos[ch-0xa0];
    
   if (key || ch)
      putkey((e->type==KeyPress), key, ch);

}

#if 0
void X_process_char(u_char ch)
{
   X_printf("X_process_char %c \n", ch);

   /* do latin1 translation */
   if (ch>=0xa0) ch=latin1_to_dos[ch-0xa0];

   /* ch is zero if the latin1 character can't be represented in the DOS charset. */
#if 1
   if (ch==0) ch='#';
#else
   if (ch==0) return;
#endif
   
   putkey(PRESS, 0, ch);
   putkey(RELEASE, 0, ch);
}
#endif

struct keyboard_client Keyboard_X =  {
   "X11",                       /* name */
   NULL,                        /* init */
   NULL,                        /* reset */
   NULL,                        /* close */
   NULL                         /* run */       /* the X11 event handler is run seperately */
};

