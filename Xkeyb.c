/*
This file contains X keyboard tables and handling routines
for dosemu. 
  exports:  X_process_key(XKeyEvent *)
  uses:     put_key(ushort scan, short charcode)
    
******************************************************************
  
Part of this code is taken from pcemu and is

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

#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "emu.h"

#define AltMask Mod1Mask
#define XK_X386_SysReq 0x1007FF00

typedef unsigned char byte;
/*
typedef unsigned short ushort;
*/

/* ASCII 0x20..0x7E */

static byte ascii_scan[] =
{
    0x39, 0x02, 0x28, 0x04, 0x05, 0x06, 0x08, 0x28,
    0x0a, 0x0b, 0x09, 0x0d, 0x33, 0x0c, 0x34, 0x35,
    0x0b, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x27, 0x27, 0x33, 0x0d, 0x34, 0x35,
    0x03, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22,
    0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18,
    0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11,
    0x2d, 0x15, 0x2c, 0x1a, 0x2b, 0x1b, 0x07, 0x0c,
    0x29, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22,
    0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18,
    0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11,
    0x2d, 0x15, 0x2c, 0x1a, 0x2b, 0x1b, 0x29
};

static byte cursor_scan[] = 
{
    0x47, /* Home */
    0x4b, /* Left */
    0x48, /* Up */
    0x4d, /* Right */
    0x50, /* Down */
    0x49, /* PgUp */
    0x51, /* PgDn */
    0x4f  /* End */
};

static byte keypad_scan[] = 
{
    0x37,     /* Multiply */
    0x4e,     /* Add      */
    0x53,     /* Seperator (-> decimal point?) */
    0x4a,     /* Subtract */
    0x53,     /* Decimal  */
    0,        /* Divide (2-byte-code, handled specially)  */
    0x52, 0x4F, 0x50, 0x51, 0x4b, 
    0x4c, 0x4d, 0x47, 0x48, 0x49  /* 0..9 */
};

static struct
{
    KeySym key;
    ushort scan_code;
} other_scan[] =
{
    { XK_BackSpace,     0x0e },
    { XK_Tab,           0x0f },
    { XK_Return,        0x1c },
    { XK_Escape,        0x01 },
    { XK_Delete,        0xe053 },
    { XK_Insert,        0xe052 },

    { XK_KP_Enter,      0xe01c },

    { XK_Shift_L,       0x2a },
    { XK_Shift_R,       0x36 },
    { XK_Control_L,     0x1d },
    { XK_Control_R,     0xe01d },
    { XK_Meta_L,        0x38 },
    { XK_Alt_L,         0x38 },
    { XK_Meta_R,        0xe038 },
    { XK_Alt_R,         0xe038 },

    { XK_Scroll_Lock,   0x46 },
};

/* This is a very quick'n dirty put_key...  */
void put_key(ushort scan, short charcode) {
#if 0
   printf("put_key(0x%X,'%c')\n",scan,charcode>=0x20?charcode:'?');
#else
   X_printf("put_key(0x%X,'%c')\n",scan,charcode>=0x20?charcode:'?');
#endif
   if (charcode!=-1) {
      if (scan&0x80) DOS_setscan(((scan & 0x7f)<< 8) | charcode);
   }
   else {
      if (scan & 0xFF00) DOS_setscan(scan&0xFF00);
      DOS_setscan((scan&0xFF)<<8);
   }
}

inline ushort translate(KeySym key)
{
    int i;

    /* ascii keys */
    if (key >= 0x20 && key <= 0x20+sizeof(ascii_scan))
        return (ascii_scan[key - 0x20]);

    /* function keys */
    if (key >= XK_F1 && key <= XK_F12) {
       if (key > XK_F10)
          return key-XK_F10+0x57;
       else
          return key-XK_F1+0x3b;
    }

    /* cursor keys */
    if (key >= XK_Home && key < XK_Home+sizeof(cursor_scan)) 
       return cursor_scan[key-XK_Home];
       
    /* keypad */
    
    if (key >= XK_KP_Multiply && key <= XK_KP_Multiply+sizeof(keypad_scan))
    {
       if (key==XK_KP_Divide)
	  return 0xe035;
       else
	  return keypad_scan[key-XK_KP_Multiply];
    }
       
    /* any other keys */
            
    for (i = 0; i < sizeof(other_scan); i++)
        if (other_scan[i].key == key)
            return (other_scan[i].scan_code);

    return 0;   /* unknown key */
}


#define MAXCHARS 3

void X_process_key(XKeyEvent *e)
{
    ushort scan;
    KeySym key;
    char chars[MAXCHARS];
    int count;
    short ch;
    static XComposeStatus compose_status = {NULL, 0};
    
#if 0
    key = XLookupKeysym(e,0);
    ch = -1;
#else
    count = XLookupString(e, chars, MAXCHARS, &key, &compose_status);
    ch = count? chars[0]:-1;
#endif
    X_printf("key %x %s, state=%08x, char='%c'\n",
             (int)key,(e->type==KeyPress)?"pressed":"released",
             e->state,ch);

    /* handle special things first */
    
    /* caps&num lock are special in X: press/release means
       turning lock on/off, not pressing/releasing the key.
       Therefore we have to generate both make & break code
       here.
    */
    if (key == XK_Caps_Lock) {
       put_key(0x3A,-1);
       put_key(0xba,-1);
       return;
    }
    if (key == XK_Num_Lock) {
       put_key(0x45,-1);
       put_key(0xc5,-1);
       return;
    }

    if (key == XK_Pause || key == XK_Break) {
       if (e->state & ControlMask) {
	  put_key(0xe046,-1);   /* Ctrl-Break */
          put_key(0xe0c6,-1);
       }
       else {
	  put_key(0xe11d,-1);   /* Pause */
	  put_key(0x45,-1);
	  put_key(0xe19d,-1);
	  put_key(0xc5,-1);
       }
       return;
    }

    if (key == XK_Print) {
       if (e->state & AltMask)
          scan = 0x54;       /* Alt-SysReq */
       else
          scan = 0xe037;     /* PrintScr */
    }
    else if (key == XK_X386_SysReq)
       scan = 0x54;
    else {
       scan = translate(key);
    }
    
    if (scan)
       put_key((e->type==KeyPress) ? scan : scan|0x80, ch);
}
