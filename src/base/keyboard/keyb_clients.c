/* 
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* This file contains code on the keyboard client side which is common to all frontends.
 * In particular, the client initialisation and the paste routine.
 */
#include <string.h>
#include <malloc.h>
#include "emu.h"
#include "termio.h"
#include "keyboard.h"
#include "keyb_clients.h"
#include "video.h"
#include "vc.h"

#define uchar unsigned char

char *paste_buffer = NULL;
int paste_len = 0, paste_idx = 0;


/* keysym codes of ascii chars 0x20..0x7E */
/* XXX -change this to KEY_[A-Z] notation */

const Bit8u ascii_keys[] =
{
    0x39, 0x02, 0x28, 0x04, 0x05, 0x06, 0x08, 0x28,   /* 0x20-0x27 */
    0x0a, 0x0b, 0x09, 0x0d, 0x33, 0x0c, 0x34, 0x35,   /* 0x28-0x2F */
    0x0b, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,   /* 0x30-0x37 */
    0x09, 0x0a, 0x27, 0x27, 0x33, 0x0d, 0x34, 0x35,   /* 0x38-0x3F */
    0x03, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22,   /* 0x40-0x47 */
    0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18,   /* 0x48-0x4F */
    0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11,   /* 0x50-0x57 */
    0x2d, 0x15, 0x2c, 0x1a, 0x2b, 0x1b, 0x07, 0x0c,   /* 0x58-0x5F */
    0x29, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22,   /* 0x60-0x67 */
    0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18,   /* 0x68-0x6F */
    0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11,   /* 0x70-0x77 */
    0x2d, 0x15, 0x2c, 0x1a, 0x2b, 0x1b, 0x29          /* 0x78-0x7E */
};

static Bit8u ascii_shift[] =  /* 1 means shifted char */
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

/* iso-8859-1 -> cp437 */
const u_char latin_to_dos[] = {
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
    0,    0xa4, 0xa2, 0x95, 0x93, 0,    0x94, 0xf6,  /* F0-F7 */
    0xed, 0x97, 0xa3, 0x96, 0x81, 0,    0,    0x98   /* F8-FF */
};

/* iso-8859-1 -> cp850 */
const u_char latin1_to_dos[] = {
    0xff, 0xad, 0xbd, 0x9c, 0xcf, 0xbe, 0xdd, 0xf5,  /* A0-A7 */
    0xf9, 0xb8, 0xa6, 0xae, 0xaa, 0xf0, 0xa9, 0xee,  /* A8-AF */
    0xf8, 0xf1, 0xfd, 0xfc, 0xef, 0xe6, 0xf4, 0xfa,  /* B0-B7 */
    0xf7, 0xfb, 0xa7, 0xaf, 0xac, 0xab, 0xf3, 0xa8,  /* B8-BF */
    0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,  /* C0-C7 */
    0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,  /* C8-CF */
    0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0x9e,  /* D0-D7 */
    0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0xe1,  /* D8-DF */
    0x85, 0xa0, 0x83, 0xc6, 0x84, 0x86, 0x91, 0x87,  /* E0-E7 */
    0x8a, 0x82, 0x88, 0x89, 0x8d, 0xa1, 0x8c, 0x8b,  /* E8-EF */
    0xd0, 0xa4, 0x95, 0xa2, 0x93, 0xe4, 0x94, 0xf6,  /* F0-F7 */
    0xed, 0x97, 0xa3, 0x96, 0x81, 0xec, 0xe7, 0x98   /* F8-FF */
};

/* iso-8859-2 -> cp852 */
const u_char latin2_to_dos[] = {
    0,    0xa4, 0xf4, 0x9d, 0xcf, 0x95, 0x97, 0xf5,  /* A0-A7 */
    0xf9, 0xe6, 0xb8, 0x9b, 0x8d, 0xf0, 0xa6, 0xbd,  /* A8-AF */
    0xf8, 0xa5, 0xf7, 0x88, 0xef, 0x96, 0x98, 0xf3,  /* B0-B7 */
    0xf2, 0xe7, 0xad, 0x9c, 0xab, 0xf1, 0xa7, 0xbe,  /* B8-BF */
    0xe8, 0xb5, 0xb6, 0xc6, 0x8e, 0x91, 0x8f, 0x80,  /* C0-C7 */
    0xac, 0x90, 0xa8, 0xd3, 0xb7, 0xd6, 0xd7, 0xd2,  /* C8-CF */
    0xd1, 0xe3, 0xd5, 0xe0, 0xe2, 0x8a, 0x99, 0x9e,  /* D0-D7 */
    0xfc, 0xde, 0xe9, 0xeb, 0x9a, 0xed, 0xdd, 0xe1,  /* D8-DF */
    0xea, 0xa0, 0x83, 0xc7, 0x84, 0x92, 0x86, 0x87,  /* E0-E7 */
    0x9f, 0x82, 0xa9, 0x89, 0xd8, 0xa1, 0x8c, 0xd4,  /* E8-EF */
    0xd0, 0xe4, 0xe5, 0xa2, 0x93, 0x8b, 0x94, 0xf6,  /* F0-F7 */
    0xfd, 0x85, 0xa3, 0xfb, 0x81, 0xec, 0xee, 0xfa   /* F8-FF */
};

/* paste a string of (almost) arbitrary length through the DOS keyboard,
 * without danger of overrunning the keyboard queue/buffer.
 * 'text' is expected to be in latin1 charset, with unix ('\n')
 * line end convention.
 */
int paste_text(const char *text, int len) {
   /* if previous paste in progress, ignore current request */
   /* XXX - maybe this should append ? */
   k_printf("KBD: paste_text called, len=%d\n",len);
   if (paste_buffer!=NULL) {
      k_printf("KBD: paste in progress, ignoring request\n");
      return 0;
   }
   paste_buffer=malloc(len);
   memcpy(paste_buffer, text, len);
   paste_len = len;
   paste_idx = 0;
   return 1;
}

static void paste_run() 
{
   uchar ch;
   int count=0;
   t_keysym keysym;
   Boolean shift;
   
   k_printf("KBD: paste_run running\n");
   if (paste_buffer) {    /* paste in progress */
      while (keyb_queuelevel() < (KEYB_QUEUE_LENGTH*3)/4) {
	 keysym=0;
	 shift=0;
	 ch=paste_buffer[paste_idx];
	 if (ch=='\n') {
	    keysym=KEY_RETURN;
	    ch=0;
	 }
	 else if (ch>=0x20 && ch<=0x7f) {
	    keysym=ascii_keys[ch-0x20];
	    shift=ascii_shift[ch-0x20];
	 }
	 else if (ch>=0xa0) {
	    switch (config.term_charset) {
	       case CHARSET_LATIN1: ch=latin1_to_dos[ch-0xa0]; break;
	       case CHARSET_LATIN2: ch=latin2_to_dos[ch-0xa0]; break;
	       case CHARSET_LATIN:
	       default:             ch=latin_to_dos[ch-0xa0]; break;
	    }
	    /* keysym= ... */
	 }

	 shift=0;
	 if (shift) presskey(KEY_L_SHIFT, 0);
         presskey(keysym, ch);
	 releasekey(keysym);
	 if (shift) releasekey(KEY_L_SHIFT);

	 count++;
	 if (++paste_idx == paste_len) {   /* paste finished */
	    free(paste_buffer);
	    paste_buffer=NULL;
	    paste_len=paste_idx=0;
	    k_printf("KBD: paste finished\n");
	    break;
	 }
      }
      k_printf("KBD: paste_run() pasted %d chars\n",count);
   }
}

/* Figures out which keyboard client to use (raw,slang or X) and initialises it.
 */
int keyb_client_init()  {
   Boolean ok;

#ifdef X_SUPPORT
   if (config.X) {
      Keyboard = &Keyboard_X;
      config.console_keyb = 0;
   }
   else 
#endif
   if (config.usesX || (config.console_keyb && can_do_root_stuff))  {
      if (config.usesX || config.console)
         Keyboard = &Keyboard_raw;
      else {
	 Keyboard = &Keyboard_slang;
         config.console_keyb=0;
      }
   }
   else {
      Keyboard = &Keyboard_slang;
   }
   if (config.console_video && config.vga && (Keyboard == &Keyboard_slang)) {
      /* we need this for proper save/restore on console switch
       * when doing graphics but not are using raw keyboard
       */
      scr_state.current = 1;
   }
   k_printf("KBD: initialising '%s' mode keyboard client\n",Keyboard->name);
   ok = Keyboard->init ? Keyboard->init() : TRUE;
   if (ok) {
      k_printf("KBD: Keyboard init ok, '%s' mode\n",Keyboard->name);
   }
   else {
      k_printf("KBD: Keyboard init ***failed***, '%s' mode\n",Keyboard->name);
   }
   return ok;     
}


void keyb_client_close() {
   if (Keyboard!=NULL && Keyboard->close!=NULL)
      Keyboard->close();
   if (config.console_video && config.vga && (Keyboard == &Keyboard_slang)) {
      /* we need this for proper restoring the console
       * when doing graphics but not are using raw keyboard
       */
      clear_console_video();
   }
}

void keyb_client_run() {
   /* if a paste operation is currently running, give it priority over the keyboard
    * frontend, in case the user continues typing before pasting is finished.
    */
   if (paste_buffer!=NULL) {
      paste_run();
   }
   else if (Keyboard!=NULL && Keyboard->run!=NULL) {
      Keyboard->run();
   }
}

