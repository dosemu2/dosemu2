/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "config.h"
#include "keyboard.h"
#include "emu.h"
#include "keyb_X.h"
#include "../../env/video/X.h"

/*
 * matching from keycode to scan code
 * codes are processed low-byte to high-byte
 */
static const int X_scan0[] = {
   0x37E0,      /*  92  Print Screen */
   0x50E0,      /*  93  Down */
   0x56,        /*  94  < > */
   0x85,        /*  95  F11 */
   0x86,        /*  96  F12 */
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
   0x4BE0,      /* 118  Left */
   0x49E0,      /* 119  Page Up */
   0x0,         /* 120  */
   0x0,         /* 121  */
   0x38E0,      /* 122  Alt-R */
   0x1DE0,      /* 123  Ctrl-R */
   0x1C,        /* 124  KP enter */
   0x35,        /* 125  KP divide */
   0x0,         /* 126  */
   0x451DE1,    /* 127  Pause*/
   0x48E0,      /* 128  Up */
   0x53E0,      /* 129  Delete */
   0x4FE0,      /* 130  End */
   0x52E0,      /* 131  Insert */
   0x0,         /* 132  */
   0x4DE0,      /* 133  Right */
   0x51E0,      /* 134  Page Down */
   0x47E0,      /* 135  Home */
   0x46E0,      /* 136  Break */
   0x0,         /* 137  */
};

static const int X_scan1[] = {
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

static void put_keycode(unsigned int scan, int released)
{
   int code;

   /*
    * scan can hold up to three scancodes.
    * Send each scancode to the emulator code setting bit 7 if the
    * key was released.
    */
   X_printf("X_put_keycode: scan=0x%04x, released=%d\n",scan,released);
   while (scan) {
      code = released ? (scan & 0xff) | 0x80 : (scan & 0xff);
      putrawkey(code);
      scan >>= 8;	 
   }
}

void X_keycode_process_key(XKeyEvent *e)
{
	static int scan_test=0;
	static const int * X_scan = 0;
	static int direct_scan=0, scan_end=0;
	unsigned int scan;

	if(!scan_test) {
		if(XKeysymToKeycode(display, XK_Down) == 93) { /* New keyboard */
			X_scan=X_scan0;
			direct_scan=92; scan_end=138;
		} else { /* XF86 keyboard */
			X_scan=X_scan1;
			direct_scan=97; scan_end=158;
		}
		scan_test = 1;
	}
	
	scan = e->keycode;
	if (scan < 9)
		return;
	/*
	* for the the "XT-keys", the XFree86 server sends us the scancode+8
	* for the keycode, for the keys new on the AT keyboard, we have to
	* translate them via the keypad table
	*/
	if (scan < direct_scan)
		scan -= 8;
	else
		if (scan < scan_end)
			scan = X_scan[scan-direct_scan];
		else /* don't allow access outside array ! */
			scan=0;
	X_printf("X_keycode_process_key: keycode=%d, scancode=%d, %s\n",
		 e->keycode,scan,(e->type==KeyRelease)?"released":"pressed");
#if 0	/* this seems to be wrong, caps/num lock doesn't works with it
	 * 			-- Herbert Xu, 2000/07/22
	 */
	if (scan == 0x3a || scan == 0x45) {
		/*
		 * bring our lock flags in sync with the state of the server
		 */
		int i = shiftstate & ((scan == 0x3a) ? CAPS_LOCK : NUM_LOCK);
		if ((e->type==KeyPress && !i) || (e->type==KeyRelease && i)) {
			put_keycode(scan, 0);
			put_keycode(scan, 1);
		}
	} else
#endif
		put_keycode(scan, (e->type==KeyRelease));
	return;
}
