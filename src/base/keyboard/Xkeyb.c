/*
 * DANG_BEGIN_MODULE
 * 
 * This file contains X keyboard tables and handling routines
 * for dosemu. 
 *   exports:  X_process_key(XKeyEvent *)
 *   exports:  X_process_char(char)
 *   uses:     put_key(ushort scan, short charcode)
 *    
******************************************************************
 *   
 * Part of this code is taken from pcemu written by David Hedley 
 * (hedley@cs.bris.ac.uk) and is
 * 
 *   Copyright (C) 1994 University of Bristol, England
 * 
 * DANG_END_MODULE
 * Permission is granted to use, copy, modify, and distribute this
 * software and its documentation for any non-commercial purpose,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in the
 * supporting documentation.
 * 
 * BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
 * FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT
 * WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER
 * PARTIES PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
 * PROGRAM IS WITH YOU.  SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME
 * THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION.
 * 
 * IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
 * WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
 * REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR
 * DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE PROGRAM
 * (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR DATA BEING RENDERED
 * INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A FAILURE OF
 * THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH HOLDER
 * OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * 
***********************************************************************
 * DANG_BEGIN_CHANGELOG
 *
 * 1995-08-30: Added X_process_char to process latin1-characters send by 
 * selection (TODO: This is not X specific, do that with the standard 
 * input too, so move it keyboard-server?), Extended X_scan and made it
 * more error tolerant
 *
 *
 * (bon@elektron.ikp.physik.th-darmstadt.de)
 *
 * DANG_END_CHANGELOG
*/

#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "emu.h"
#include "termio.h"
#include "memory.h"
#include "bios.h"

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

static byte ascii_shift[] =  /* 1 means shifted char */
/* Fixme- may contain errors 23.7.95*/
{ 
    0,1,1,1,1,1,1,0,
    1,1,1,1,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,1,0,1,0,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,0,0,0,1,1,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,1,1,1,1,1
};

static const u_char latin1_to_dos[] = {
    0,    0xad, 0x9b, 0x9c, 0,    0x9d, 0x7c, 0x15,  /* A0-A7 */
    0x22, 0,    0xa6, 0xae, 0xaa, 0x2d, 0,    0,     /* A8-AF */
    0xf8, 0xf1, 0xfd, 0xfc, 0x27, 0xe6, 0x14, 0xf9,  /* B0-B7 */
    0x2c, 0,    0xa7, 0xaf, 0xac, 0xab, 0,    0xa8,  /* B8-BF */
    0,    0,    0,    0,    0x8e, 0x8f, 0x92, 0x80,  /* C0-C7 */
    0,    0x90, 0,    0,    0,    0,    0,    0,     /* C8-CF */
    0,    0xa5, 0,    0,    0,    0,    0x99, 0,     /* D0-D7 */
    0xed, 0,    0,    0,    0x9a, 0,    0,    0xe1,  /* D8-DF */
    0x85, 0xa0, 0x83, 0,    0x84, 0x86, 0x91, 0x87,  /* E0-E7 */
    0x8a, 0x82, 0x88, 0x89, 0x8d, 0xa1, 0x8c, 0x8b,  /* E8-EF */
    0xeb, 0xa4, 0x95, 0xa2, 0x93, 0,    0x94, 0xf6,  /* F0-F7 */
    0xed, 0x97, 0xa3, 0x96, 0x81, 0,    0,    0x98   /* F8-FF */
};

/* 
   KP_0=0x52="R"
   KP_1=0x4f="O"
   KP_2=0x50="P"
   KP_3=0x51="Q"
   KP_4=0x4b="K"
   KP_5=0x4c="L"
   KP_6=0x4d="M"
   KP_7=0x47="G"
   KP_8=0x38="H"
   KP_9=0x48="I"
*/
static const u_char latin1_to_scan[][3] = {
    /*"000","173","155","156","000","157","124","021",    A0-A7 */
    /*"034","000","166","174","170","045","000","000",   A8-AF */
    /*"248","241","253","252","039","230","020","249",   B0-B7 */
    /*"044","000","167","175","172","171","000","168",   B8-BF */
    /*"000","000","000","000","142","143","145","128",   C0-C7 */
    /*"000","144","000","000","000","000","000","000",   C8-CF */
    /*"000","165","000","000","000","000","153","000",   D0-D7 */
    /*"237","000","000","000","154","000","000","225",   D8-DF */
    /*"133","160","131","000","132","134","145","135",   E0-E7 */
    /*"138","130","136","137","141","161","140","139",   E8-EF */
    /*"235","164","149","162","147","000","148","246",   F0-F7 */
    /*"237","151","163","150","129","000","000","152"    F8-FF */
    "RRR","OGQ","OLL","OLM","RRR","OLG","OPK","RPO",  /* A0-A7 */
    "RQK","RRR","OMM","OGK","OGR","RKL","RRR","RRR",  /* A8-AF */
    "PKH","PKO","PLQ","PLP","RQI","PQR","RPR","PKI",  /* B0-B7 */
    "RKK","RRR","OMG","OGL","OGP","OGO","RRR","OMH",  /* B8-BF */
    "RRR","RRR","RRR","RRR","OKP","OKQ","OKL","OPH",  /* C0-C7 */
    "RRR","OKK","RRR","RRR","RRR","RRR","RRR","RRR",  /* C8-CF */
    "RRR","OML","RRR","RRR","RRR","RRR","OLQ","RRR",  /* D0-D7 */
    "PQG","RRR","RRR","RRR","OLK","RRR","RRR","PPL",  /* D8-DF */
    "OQQ","OMR","OQO","RRR","OQP","OQK","OKL","OQL",  /* E0-E7 */
    "OQH","OQR","OQM","OQG","OKO","OMO","OKR","OQI",  /* E8-EF */
    "PQL","OMK","OMP","OKI","OKG","RRR","OKH","PKM",  /* F0-F7 */
    "PQG","OLO","OMQ","OLR","OPI","RRR","RRR","OLP"   /* F8-FF */
};

static const byte cursor_scan[] = 
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

static const byte keypad_scan[] = 
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

static const struct 
{
    KeySym key;
    ushort scan_code;
} other_scan[] =
{
    { XK_Return,        0x1c },
    { XK_Escape,        0x01 },
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

#define NUM_OTHER (sizeof(other_scan)/sizeof(other_scan[0]))

/*
 * matching from keycode to scan code
 * codes are processed low-byte to high-byte
 */
#ifdef NEW_KEYCODES
static const int X_scan[] = {
   0x37E0,      /*  92  Print Screen */
   0x5000,      /*  93  Down */
   0x4B00,      /*  94  Left */
   0x8500,      /*  95  F11 */
   0x8600,      /*  96  F12 */
   0x0,         /*  97  */
   0x0,         /*  98  */
   0x0,         /*  99  */
   0x0,         /* 100  */
   0x0,         /* 101  */
   0x0,         /* 102  */
   0x0,         /* 103  */
   0x0,         /* 104  */
   0x0,         /* 105  */
   0x0,         /* 106  */
   0x0,         /* 107  */
   0x0,         /* 108  */
   0x0,         /* 109  */
   0x0,         /* 110  */
   0x0,         /* 111  */
   0x0,         /* 112  */
   0x0,         /* 113  */
   0x0,         /* 114  */   
   0x0,         /* 115  */
   0x0,         /* 116  */
   0x0,         /* 117  */
   0x0,         /* 118  */
   0x4900,      /* 119  Page Up */
   0x0,         /* 120  */
   0x0,         /* 121  */
   0x0,         /* 122  */
   0x0,         /* 123  */
   0x1C,        /* 124  KP enter */
   0x35,        /* 125  KP divide */
   0x0,         /* 126  */
   0x451DE1,    /* 127  Pause*/
   0x4800,      /* 128  Up */
   0x5300,      /* 129  Delete */
   0x4F00,      /* 130  End */
   0x5200,      /* 131  Insert */
   0x0,         /* 132  */
   0x4D00,      /* 133  Right */
   0x5100,      /* 134  Page Down */
   0x4700,      /* 135  Home */
   0x46E0,      /* 136  Break */
   0x0,         /* 137  */
};
#else
static const int X_scan[] = {
   0x47e0,      /*  97  Home   */
   0x48e0,      /*  98  Up     */
   0x49e0,      /*  99  PgUp   */
   0x4be0,      /* 100  Left   */
   0x4c,        /* 101  KP-5   */
   0x4de0,      /* 102  Right  */
   0x4fe0,      /* 103  End    */
   0x50e0,      /* 104  Down   */
   0x51e0,      /* 105  PgDn   */
   0x52e0,      /* 106  Ins    */
   0x53e0,      /* 107  Del    */
   0x1ce0,      /* 108  Enter  */
   0x1de0,      /* 109  Ctrl-R */
   0x451de1,    /* 110  Pause  */
   0x37e0,      /* 111  Print  */
   0x35e0,      /* 112  Divide */
   0x38e0,      /* 113  Alt-R  */
   0x46e0,      /* 114  Break  */   
   0x0,         /* 115 */
   0x0,         /* 116 */
   0x0,         /* 117 */
   0x0,         /* 118 */
   0x0,         /* 119 */
   0x0,         /* 120 */
   0x0,         /* 121 */
   0x0,         /* 122 */
   0x0,         /* 123 */
   0x0,         /* 124 */
   0x0,         /* 125 */
   0x0,         /* 126 */
   0x0,         /* 127 */
   0x0,         /* 128 */
   0x0,         /* 129 */
   0x0,         /* 130 */
   0x0,         /* 131 */
   0x0,         /* 132 */
   0x0,         /* 133 */
   0x0,         /* 134 */
   0x0,         /* 135 */
   0x47,         /* 136 KP_7 */
   0x48,         /* 137 KP_8 */
   0x49,         /* 138 KP_9 */
   0x4b,         /* 139 KP_4 */
   0x4c,         /* 140 KP_5 */
   0x4d,         /* 141 KP_6 */
   0x4f,         /* 142 KP_1 */
   0x50,         /* 143 KP_2 */
   0x51,         /* 144 KP_3 */
   0x52,         /* 145 KP_0 */
   0x53,         /* 146 KP_. */
   0x47,         /* 147 KP_HOME */
   0x48,         /* 148 KP_UP */
   0x49,         /* 149 KP_PgUp */
   0x4b,         /* 150 KP_Left */
   0x4c,         /* 151 KP_ */
   0x4d,         /* 152 KP_Right */
   0x4f,         /* 153 KP_End */
   0x50,         /* 154 KP_Down */
   0x51,         /* 155 KP_PgDn */
   0x52,         /* 156 KP_Ins */
   0x53,         /* 157 KP_Del */
};
#endif /* NEW_KEYCODES */


/* This is a very quick'n dirty put_key...  */
static void put_key(ushort scan, short charcode) {
   X_printf("put_key(0x%X,'%c'=%d)\n",scan,charcode>=0x20?charcode:'?',charcode);
   
   if (charcode!=-1) {
      add_scancode_to_queue((charcode<<8) | scan);
   }
   else {
      if (scan & 0xFF00) {
	 child_set_flags(scan>>8);
	 add_scancode_to_queue(scan>>8);
      } 
      child_set_flags(scan&0xff);
      add_scancode_to_queue(scan&0xff);
   }
}

static void put_keycode(unsigned int scan, int released)
{
   int code;

   /*
    * scan can hold up to three scancodes.
    * Send each scancode to the emulator code setting bit 7 if the
    * key was released.
    */
   X_printf("X_put_key: scan=0x%04x, released=%d\n",scan,released);
   while (scan) {
      code = released ? (scan & 0xff) | 0x80 : (scan & 0xff);
      child_set_flags(code);
      add_scancode_to_queue(code);
      scan >>= 8;	 
   }
}

static ushort translate(KeySym key)
{
    int i;

    /* ascii keys */
    if ((key <= 0x7e) && (key >= 0x20))
      return ascii_scan[key - 0x20];


    /* function keys:
       note that shift-F1..F8 actually give shift-F13..F20
       (at least on my system), so we have to do a little translation here.
    */
    if (key >= XK_F1 && key <= XK_F24) {
       if (key > XK_F12)
	  key -= 12;
       if (key > XK_F10)
	  return key-XK_F11+0x57;    /* F11,F12 */
       else
	  return key-XK_F1+0x3b;     /* F1..F10  */
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
            
    for (i = 0; i < NUM_OTHER; i++)
        if (other_scan[i].key == key)
            return (other_scan[i].scan_code);

    return 0;   /* unknown key */
}


#define MAXCHARS 3

void X_process_key(XKeyEvent *e)
{
    unsigned int    scan;
    KeySym key;
    u_char chars[MAXCHARS];
    int count;
    short ch;
    static XComposeStatus compose_status = {NULL, 0};
    int i;
    
    if (config.X_keycode) {
       scan = e->keycode;
       if (scan<9)
	  return;
       /*
	* for the the "XT-keys", the XFree86 server sends us the scancode+8
	* for the keycode, for the keys new on the AT keyboard, we have to
	* translate them via the keypad table
	*/
#ifdef NEW_KEYCODES       
       if (scan < 92)
#else
       if (scan < 97)
#endif
	  scan -= 8;
       else
#ifdef NEW_KEYCODES       
          if (scan < 138)
	  scan = X_scan[scan-92];
#else
          if (scan < 158)
	  scan = X_scan[scan-97];
#endif
	  else /* don't allow access outside array ! */
	    scan=0;
       X_printf("X_process_key: keycode=%d, scancode=%d, %s\n",
		e->keycode,scan,(e->type==KeyRelease)?"released":"pressed");
       if (scan == 0x3a || scan == 0x45) {
	  /*
	   * bring our lock flags in sync with the state of the server
	   */
	  i = kbd_flag((scan==0x3a) ? KF_CAPSLOCK : KF_NUMLOCK);
	  if ((e->type==KeyPress && !i) || (e->type==KeyRelease && i)) {
	     put_keycode(scan, 0);
	     put_keycode(scan, 1);
	  }
       } else
	  put_keycode(scan, (e->type==KeyRelease));
       return;
    }
    count = XLookupString(e, chars, MAXCHARS, &key, &compose_status);
    ch = count? chars[0]:-1;
    X_printf("key %x %s, state=%08x, char='%c'\n",
             (int)key,(e->type==KeyPress)?"pressed":"released",
             e->state,ch);

   if (key <= 0x7e) {
      scan = ascii_scan[key-0x20];
      put_keycode(scan, (e->type==KeyRelease));
      
   } else  {
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
      else if (key == XK_BackSpace) {
         scan = 0x0e;
         ch = 8;
      } else if (key == XK_Delete) {
         scan = 0x53;
         ch = 0;
      } else if (key == XK_Return)  {
         if ((e->state & ControlMask))
           ch = 0x0a;
         else
           ch = 0x0d;
         scan = 0x1c;
      } else if (key == XK_Tab) {
         scan = 0x0f;
         if ((e->state & (ShiftMask | ControlMask | AltMask)))
	    ch = 0;
      } else {
         scan = translate(key);
         if ((e->state & AltMask)) {
	    ch=0;
         }
      }
    /* do latin1 translation */
    if (ch>=0xa0)     
      {
        if (e->type!=KeyPress)
          /* We won't find most latin characters on the dos keyboard
             Lets emulate them with ALT-KEYPAD when the key is released
             and don't bother if the key is pressed */
          X_process_char(ch);
        return;
      }
    if (scan || ch)
       put_key((e->type==KeyPress) ? scan : scan|0x80, ch);
   }
}

void X_process_char(u_char ch)
{
   X_printf("X_process_char %c \n", ch);
   switch (ch) 
     {
     case 0xd : 
       put_keycode(0x1c, 0);
       put_keycode(0x1c, 1);
       break;
     case 0x20 ... 0x7E :
       if (ascii_shift[ch-0x20])
	  put_keycode(0x2a, 0);
       put_keycode(ascii_scan[ch-0x20], 0);
       put_keycode(ascii_scan[ch-0x20], 1);
       if (ascii_shift[ch-0x20])
	  put_keycode(0x2a, 1);
       break;
     case 0xa0 ... 0xff :
       if (latin1_to_scan[ch-0xa0][0] != 0x52) /* suppress zero */
	 {
	   put_keycode(0x38, 0);
	   put_keycode(latin1_to_scan[ch-0xa0][0], 0);
	   put_keycode(latin1_to_scan[ch-0xa0][0], 1);
	   put_keycode(latin1_to_scan[ch-0xa0][1], 0);
	   put_keycode(latin1_to_scan[ch-0xa0][1], 1);
	   put_keycode(latin1_to_scan[ch-0xa0][2], 0);
	   put_keycode(latin1_to_scan[ch-0xa0][2], 1);
	   put_keycode(0x38, 1);
	 }
       else
	 if (latin1_to_scan[ch-0xa0][1] != 0x52) 
	   /* suppress leading zero */
	   {
	     put_keycode(0x38, 0);
	     put_keycode(latin1_to_scan[ch-0xa0][1], 0);
	     put_keycode(latin1_to_scan[ch-0xa0][1], 1);
	     put_keycode(latin1_to_scan[ch-0xa0][2], 0);
	     put_keycode(latin1_to_scan[ch-0xa0][2], 1);
	     put_keycode(0x38, 1);
 	   }
	 else
	   if (latin1_to_scan[ch-0xa0][1] != 0x52) 
	     /* suppress leading zero */
	     {
	       put_keycode(0x38, 0);
	       put_keycode(latin1_to_scan[ch-0xa0][2], 0);
	       put_keycode(latin1_to_scan[ch-0xa0][2], 1);
	       put_keycode(0x38, 1);
	     }
       return;
     default:
       X_printf("X_process_char invalid char\n");
       break;
     }
}
