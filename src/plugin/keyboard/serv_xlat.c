/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * DANG_BEGIN_MODULE
 * 
 * Description: Keyboard translation
 * 
 * Maintainer: Rainer Zimmermann <zimmerm@mathematik.uni-marburg.de>
 * 
 * REMARK
 * This module contains the the translation part of the keyboard 'server',
 * which translates keysyms into the form in which they can be sent do DOS.
 *
 * The frontends will call one of the following functions to send
 * keyboard events to DOS:
 *
 * VERB
 *     putrawkey(t_rawkeycode code);
 *     putkey(Boolean make, t_keysym key)
 *     set_shiftstate(t_shiftstate s);
 * /VERB
 *
 * Interface to serv_backend.c is through write_queue(bios_key, shift, raw).
 *
 * More information about this module is in doc/README.newkbd
 *
 * /REMARK
 * DANG_END_MODULE
 *
 */

#define __KBD_SERVER

#include <ctype.h>
#include <string.h>

#include "config.h"
#include "emu.h"
#include "vc.h"
#include "mouse.h"

#include "keymaps.h"
#include "keyboard.h"
#include "keyb_server.h"
#include "keyb_clients.h"
#include "video.h"

Bit32u rawprefix;   /* used in putrawkey() */
uchar accent;       /* buffer for dead-key accents */
int alt_num_buffer; /* used for alt-keypad entry mode */ 
                                       
int lockaltmap;

/*
 * various tables
 */

/* 
 * BIOS scancode tables
 *
 * For 'unshifted', no translation is done (bios scancode=hardware scancode),
 * for SHIFT the translation is done 'by hand' in make_bios_code().
 */

static Bit8u bios_ctrl_scancodes[128] =
{
   0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07,    /* 00-07 */
   0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x0e, 0x94,    /* 08-0F */
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,    /* 10-17 */
   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x00, 0x1e, 0x1f,    /* 18-1F */
   0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x00,    /* 20-27 */
   0x00, 0x00, 0x00, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,    /* 28-2F */
   0x30, 0x31, 0x32, 0x00, 0x00, 0x00, 0x00, 0x96,    /* 30-37 */
   0x00, 0x39, 0x00, 0x5e, 0x5f, 0x60, 0x61, 0x62,    /* 38-3F */
   0x63, 0x64, 0x65, 0x66, 0x67, 0x00, 0x00, 0x77,    /* 40-47 */
   0x8d, 0x84, 0x8e, 0x73, 0x8f, 0x74, 0x90, 0x75,    /* 48-4F */
   0x91, 0x76, 0x92, 0x93, 0x00, 0x00, 0x00, 0x89,    /* 50-57 */
   0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 58-5F */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 60-67 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 68-6F */
   0x00, 0x00, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00,    /* 70-77 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 78-7F */
};

static Bit8u bios_alt_scancodes[128] =
{
   0x00, 0x01, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d,    /* 00-07 */
   0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x0e, 0xa5,    /* 08-0F */
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,    /* 10-17 */
   0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x00, 0x1e, 0x1f,    /* 18-1F */
   0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,    /* 20-27 */
   0x28, 0x29, 0x00, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,    /* 28-2F */
   0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x00, 0x37,    /* 30-37 */
   0x00, 0x39, 0x00, 0x68, 0x69, 0x6a, 0x6b, 0x6c,    /* 38-3F */
   0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x00, 0x00, 0x97,    /* 40-47 */
   0x98, 0x99, 0x4a, 0x9b, 0x00, 0x9d, 0x4e, 0x9f,    /* 48-4F */
   0xa0, 0xa1, 0xa2, 0xa3, 0x00, 0x00, 0x00, 0x8b,    /* 50-57 */
   0x8c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 58-5F */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 60-67 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 68-6F */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 70-77 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    /* 78-7F */
};

/* conversion table for scancodes prefixed with 0xe0
   index is key-E0_MINIDX.
 */

#define E0_MINIDX 0xe035
#define E0_MAXIDX 0xe053
#define E0_COUNT (E0_MAXIDX-E0_MINIDX+1)

static Bit8u bios_ctrl_e0_scancodes[E0_COUNT] =
{
   0x95, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x77, 0x8d, 0x84, 0x8e, 
   0x73, 0x8f, 0x74, 0x00, 0x75, 0x91, 0x76, 0x92, 0x93
};

static Bit8u bios_alt_e0_scancodes[E0_COUNT] =
{
   0xa4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97, 0x98, 0x99, 0x4a, 
   0x9b, 0x00, 0x9d, 0x4e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3
};

/* uppercase conversion table for CAPS LOCK.
   This is probably insufficient as many uppercase characters are
   not part of the standard extended ascii code - it would need to 
   be code page dependent.
   Use with index [char-0x80].
*/
static uchar uppercase_table[37] =
{ 0x80, 0x9a, 0x90, 0x83, 0x8e, 0x85, 0x8f, 0x80, 
  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
  0x90, 0x92, 0x92, 0x93, 0x99, 0x95, 0x96, 0x97,
  0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
  0xa0, 0xa1, 0xa2, 0xa3, 0xa5
};


/******************************************************************************************
 * Queue front end (keycode translation etc.)
 ******************************************************************************************/

/*
 * translate a keyboard event into a 16-bit BIOS scancode word (LO=ascii char, HI="scancode").
 * This is sometimes rather messy business.
 * For character keys, the ascii parameter is used, otherwise the output ascii code is
 * generated (well, that's the way it oughta be, at least).
 */

static Bit16u make_bios_code(Boolean make, t_keysym key, uchar ascii) 
{
   Bit8u bios_scan = 0;
   Bit16u special = 0;

#if 0
   printf("KBD: make_bios_code(%s, %04x, '%c') called\n",
            make?"PRESS":"RELEASE",(unsigned int)key,ascii?ascii:' ');
#endif
#if 0
   k_printf("KBD: make_bios_code(%s, %04x, '%c') called\n",
            make?"PRESS":"RELEASE",(unsigned int)key,ascii?ascii:' ');
#endif
   
   if (make) {

      /* filter out shift keys */
      switch(key) {
       case KEY_L_SHIFT:
       case KEY_R_SHIFT:
       case KEY_L_CTRL:
       case KEY_R_CTRL:
       case KEY_L_ALT:
       case KEY_R_ALT:
       case KEY_CAPS:
       case KEY_NUM:
       case KEY_SCROLL:
               /*k_printf("KBD: make_bios_code() discarding shift key\n");*/
               return 0;
      }

      /* check for special handling */
      /* XXX - should these be independent of shift state ? */
      switch(key) {
       case KEY_PAUSE:  special=SP_PAUSE; break;
       case KEY_BREAK:  special=SP_BREAK; break;
       case KEY_PRTSCR: special=SP_PRTSCR; break;
       case KEY_SYSRQ:  special=SP_SYSRQ_MAKE; break;
      }

      /* convert to BIOS INT16 scancode, depending on shift state 
       */

      if (shiftstate&ALT) {
         switch(key)  {
          case KEY_BKSP:
            bios_scan=0x0e; ascii=0; break;
          case KEY_PAD_ENTER:
            bios_scan=0xa6; ascii=0; break;
          case KEY_PAD_DECIMAL:
            bios_scan=0; ascii=0; break; /* Turn this key off! */
          case E0_MINIDX ... E0_MAXIDX:
            bios_scan=bios_alt_e0_scancodes[key-E0_MINIDX];
            ascii=0;
            break;

          case KEY_PAD_7:                   /* alt-keypad */
          case KEY_PAD_8:
          case KEY_PAD_9:
          case KEY_PAD_4:
          case KEY_PAD_5:
          case KEY_PAD_6:
          case KEY_PAD_1:
          case KEY_PAD_2:
          case KEY_PAD_3:
          case KEY_PAD_0:
	  {
	    int val = 0;
	    switch(key) {
	    case KEY_PAD_7: val = 7; break;
	    case KEY_PAD_8: val = 8; break;
	    case KEY_PAD_9: val = 9; break;
	    case KEY_PAD_4: val = 4; break;
	    case KEY_PAD_5: val = 5; break;
	    case KEY_PAD_6: val = 6; break;
	    case KEY_PAD_1: val = 1; break;
	    case KEY_PAD_2: val = 2; break;
	    case KEY_PAD_3: val = 3; break;
	    case KEY_PAD_0: val = 0; break;
	    }
	    alt_num_buffer = (alt_num_buffer*10 + val) & 0xff;
            k_printf("KBD: alt-keypad key=%08d ascii=%c buffer=%d\n",key,ascii,alt_num_buffer);
            ascii=0;
            bios_scan=0;
            break;
	  }
            
          default:
            if (key&0xff00)  {
               bios_scan=0;
               ascii=0;
            } else if (!(config.keytable->flags & KT_USES_ALTMAP) || (shiftstate&L_ALT)) {
               bios_scan=bios_alt_scancodes[key];
               ascii=0;
            }
            else {
               bios_scan=key&0x007f;
            }
         }
      }
      else if (shiftstate&CTRL) {

         if (key==KEY_PAD_ENTER) {
            bios_scan=0xe0;
         }
         else if (key&0xff00) {
            if (key>=E0_MINIDX && key<=E0_MAXIDX) {
               bios_scan=bios_ctrl_e0_scancodes[key-E0_MINIDX];
			   ascii=0xe0;
			}
         }
         else {
            bios_scan=bios_ctrl_scancodes[key&0xff];
         }

         /* if table entry was 0, generate nothing, otherwise generate
            ascii codes of control keys or use ascii control codes
            that were passed.
          */
         if (bios_scan) {
            if (ascii&~0x1f && ascii != 0xe0) 
	       ascii=0;  /* allow control keys to be passed */
            if (bios_scan) {
               switch(key) {
                  case KEY_BKSP:       ascii=0x7f; break;
                  case KEY_TAB:        ascii=0;    break;
                  case KEY_RETURN:
                  case KEY_PAD_ENTER:  ascii=0x0a; break;
                  case KEY_ESC:        ascii=0x1b; break;
                  case KEY_6:          ascii=0x1e; break;
                  case KEY_PAD_SLASH:  ascii=0;    break;
                  case KEY_PAD_MINUS:  ascii=0;    break;
               }
            }
         }
         else {
            ascii=0;
         }
	 switch (key) {  /* The cursor block */
	 case KEY_INS:	 case KEY_DEL:
	 case KEY_HOME:	 case KEY_END:
	 case KEY_PGUP:	 case KEY_PGDN:
	 case KEY_UP:	 case KEY_DOWN:
	 case KEY_LEFT:	 case KEY_RIGHT:
	   ascii = 0xe0; break;
	 }
      }
      else if (shiftstate&SHIFT) {
         switch(key) {
          case KEY_ESC:
                  bios_scan=0x01; ascii=0x1b; break;
          case KEY_BKSP:
                  bios_scan=0x0e; ascii=0x08; break;
          case KEY_TAB:
                  bios_scan=0x0f; ascii=0; break;
          case KEY_RETURN:
                  bios_scan=0x1c; ascii=0x0d; break;
          case KEY_F1 ... KEY_F10:
                  bios_scan=key+(0x54-KEY_F1); break;
          case KEY_F11:
                  bios_scan=0x87; break;
          case KEY_F12:
                  bios_scan=0x88; break;
	  case KEY_PAD_ENTER:
		  bios_scan=0xe0; break;
          case KEY_PRTSCR:
		  bios_scan=0; break;
	  case 0xe047 ... 0xe053:
		  bios_scan=key&0x7f; ascii=0xe0; break;
	  case KEY_PAD_SLASH:
		  bios_scan=0xe0; ascii='/'; break;
          default:
                  bios_scan=key&0x7f; break;
         }
      }
      else {   /* unshifted */
         switch(key) {
          case KEY_ESC:
                  bios_scan=0x01; ascii=0x1b; break;
          case KEY_BKSP:
                  bios_scan=0x0e; ascii=0x08; break;
          case KEY_RETURN:
                  bios_scan=0x1c; ascii=0x0d; break;
          case KEY_F11:
                  bios_scan=0x85; break;
          case KEY_F12:
                  bios_scan=0x86; break;
          case KEY_PAD_ENTER:
                  bios_scan=0xe0; break;
          case KEY_PRTSCR:
                  bios_scan=0; break;
          case 0xe047 ... 0xe053:
                  bios_scan=key&0x7f; ascii=0xe0; break;
	  case KEY_PAD_SLASH:
		  bios_scan=0xe0; ascii='/'; break;
          default:
                  bios_scan=key&0x7f; break;
         }
      }
      /* all shiftstates */
      if (key==KEY_SPACE) ascii=' ';
   }
   else { /* !make */
      if (key==KEY_SYSRQ) {
         special=SP_SYSRQ_BREAK;
      }
      else if (alt_num_buffer && (key==KEY_L_ALT || key==KEY_R_ALT)) {
         if (alt_num_buffer<256) {
           bios_scan=0;
           ascii=alt_num_buffer;
           alt_num_buffer=0;
         }
      }
      else {
         ascii=0;
      }
   }

#if 0
   k_printf("KBD: bios_scan=%02x ascii=%02x special=%04x\n",
            bios_scan,ascii,special);
#endif
   
   if (special!=0)
      return special;
   else
      return (bios_scan<<8)|ascii;

}


/*
 *    Translation
 */

static Boolean do_shift_keys(Boolean make, t_keysym key) {
   t_shiftstate mask=0, togglemask=0;
   Boolean is_shift=1;

   switch(key) {
    case KEY_L_ALT:    mask=L_ALT; break;
    case KEY_R_ALT:    mask=R_ALT; break;
    case KEY_L_CTRL:   mask=L_CTRL; break;
    case KEY_R_CTRL:   mask=R_CTRL; break;
    case KEY_L_SHIFT:  mask=L_SHIFT; break;
    case KEY_R_SHIFT:  mask=R_SHIFT; break;
    case KEY_CAPS:     mask=CAPS_PRESSED; togglemask=CAPS_LOCK; break;
    case KEY_NUM:      mask=NUM_PRESSED;  togglemask=NUM_LOCK;  break;
    case KEY_SCROLL:   mask=SCR_PRESSED;  togglemask=SCR_LOCK;  break;
    case KEY_INS:      mask=INS_PRESSED;  togglemask=INS_LOCK;  is_shift=0; break;
    case KEY_SYSRQ:    mask=SYSRQ_PRESSED; is_shift=0; break;
    default:           is_shift=0; break;
   }

   if (make) {
      shiftstate |= mask;
      shiftstate ^= togglemask;
   }
   else {
      shiftstate &= ~mask;
   }

   if (mask)
      k_printf("KBD: do_shift_keys(): new shiftstate=%04x\n",shiftstate);

#if 0
   if (make && config.shiftclearcaps && 
       (key==KEY_L_SHIFT || key==KEY_R_SHIFT)) 
   {
      shiftstate &= ~CAPS_LOCK;
   }
#endif

   return is_shift;
}


/* translate a keysym to its ASCII equivalent. This is used only in raw
 * keyboard mode and is the only part of the keyboard code that
 * depends on the "nationality" of the keyboard.
 *
 * (*is_accent) returns TRUE if the if char was produced with an accent key.
 */

static uchar translate(t_keysym key, Boolean *is_accent) {
   uchar ch = 0;
   int i;
   int is_ctrl=0;

   k_printf("KBD: translate(key=%04x)\n",key);
   
   *is_accent=FALSE;

   if (key & 0xff00) {    /* prefixed scancodes */
      if (shiftstate & ALT) {
      }
      else if (shiftstate & CTRL) {
         switch(key) {
          case KEY_PAD_ENTER:  ch=0x0a; break;
         }
      }
      else {
         switch(key) {
          case KEY_PAD_SLASH:  ch=0x2f; break;
          case KEY_PAD_ENTER:  ch=0x0d; break;
         }
      }
   }
   else if (key>=0x47 && key<=0x53)  {          /* keypad keys */

      if (((!!(shiftstate & NUM_LOCK)) ^ (!!(shiftstate & SHIFT))) ||
          (shiftstate&ALT) || (key==KEY_PAD_PLUS) || (key==KEY_PAD_MINUS) )
      {
        ch = config.keytable->num_table[key-0x47];
      }
   }
   else {
      if (shiftstate & ALT) {
         /* it seems to me that on all keyboards except US the right ALT key 
          * is AltGr. Is this true?
          * AltGr does 'alt_map' character translation, while with left ALT no 
          * characters are generated. On US keyboards, translation is done 
          * with both ALT keys.
          */
        
         /* BASIC style hack for Polish language.  ALT+DEAD_KEY -> accent=DEAD_KEY
          * ch >= 32 should be "ch is not DEAD_KEY"
          */ 
          if (!(config.keytable->flags & KT_USES_ALTMAP) || (shiftstate & R_ALT)) {
            if (lockaltmap)
              ch = config.altkeytable->alt_map[key];
            else
            ch = config.keytable->alt_map[key];
            if (ch >= 32) goto NEXT_CONDITION;
            accent=ch;
          }
          else goto NEXT_CONDITION; 
      }
      if (shiftstate & SHIFT) {
        if (lockaltmap)
          ch = config.altkeytable->shift_map[key];
        else
	ch = config.keytable->shift_map[key];
      }
      else {   /* unshifted */
        if (lockaltmap && !(shiftstate & CTRL))
          ch = config.altkeytable->key_map[key];
        else
        ch = config.keytable->key_map[key];
      }
   }
   
 NEXT_CONDITION:
	   
   if (shiftstate & CTRL) {
      switch(ch) {
         case 0x40 ... 0x7e: ch&=0x1f; break;
         case '-':           ch=0x1f;  break;
#if 0  /* not needed, these are generated in make_bios_code */
         default:
           switch(key) {
            case KEY_2:          ch=0;    break;
            case KEY_6:          ch=0x1e; break;
            case KEY_BKSP:       ch=0x7f; break;
            case KEY_RETURN:
            case KEY_PAD_ENTER:  ch=0x0a; break;
           }
#endif
      }
      is_ctrl=1;
   }

   if (shiftstate & CAPS_LOCK) {
      /* convert to uppercase */
      ch=toupper(ch);
      if (ch>=0x80 && ch<=0xa5) ch=uppercase_table[ch-0x80];
   }

	 if (ch>=0xa0) {
		 /* Does latin need translate too? */
		 switch (config.term_charset) {
		 case CHARSET_KOI8:   ch=koi8_to_dos[ch-0xa0]; break;
		 }
	 }
   
   k_printf("KBD: translated key = %02x\n",ch);

   /* this is a little hack-- since the 'dead key' values are 1..12 (i.e. ^A..^L),
    * we don't check for them if we have a Ctrl character.
    * Alternatively, we could blow up the keytables to 'short', but let's not waste
    * memory. 
    */
   if (!is_ctrl) {
      /* check for a dead key */
      for (i = 0; config.keytable->dead_key_table[i] != 0; i++) {
        if (ch == config.keytable->dead_key_table[i]) {
          if (accent != ch) {
             accent = ch;
             k_printf("KBD: got accent %c\n",accent);
             return 0;
          }
        }
      }
   }

   if (accent) {                        /* translate dead keys */

      if (ch==' ') ch=accent;

      for (i = 0; config.keytable->dead_map[i].d_key != 0; i++) {
         if (accent == config.keytable->dead_map[i].d_key &&
             config.keytable->dead_map[i].in_key == ch) 
         {
            ch = config.keytable->dead_map[i].out_key;
            k_printf("KBD: accented key: %c + %c = %c\n",
                     accent,
                     config.keytable->dead_map[i].in_key,
                     ch);

            *is_accent=TRUE;
            break;
         }
      }
      accent = 0;
   }

   return ch;
}

/* handle special dosemu keys like Ctrl-Alt-PgDn
 * This should only be called with 'make' events!
 * returns 1 if key was handled.
 */

static Boolean handle_dosemu_keys(t_keysym key) {
   int vc_num;

   if ((shiftstate&CTRL) && (shiftstate&ALT)) {
      switch(key) {
#ifdef X86_EMULATOR
       case KEY_PGUP:
             k_printf("KBD: Ctrl-Alt-PgUp\n");
             if (config.cpuemu) {
		int v;
		if (debug_level('e')<2) {
		  set_debug_level('e', 5); v=3;
		}
		else {
		  set_debug_level('e', 0); v=0;
		}
		parse_debugflags("dDvkiZT#ghExMrSmQ", v);
		fflush(dbg_fd);
             }
             return 1;
#else
#if 0	/* C-A-D is disabled */
       case KEY_DEL:
       case KEY_PGUP:
             k_printf("KBD: Ctrl-Alt-{Del|PgUp}: rebooting dosemu\n");
             dos_ctrl_alt_del();
             return 1;
#endif
#endif
       case KEY_PGDN:
             k_printf("KBD: Ctrl-Alt-PgDn: bye bye!\n");
             leavedos(123);  /* use this exitcode, so a wrapper script
				may detect Ctrl-Alt-PgDn and restart
				dosemu if if wants (pseudo-reboot) */

       case KEY_F1 ... KEY_F10:
       case KEY_F11:
       case KEY_F12:
             if (config.console_keyb) {
                vc_num = (key>KEY_F10) ? (key-KEY_F11+11) : (key-KEY_F1+1);
                k_printf("KBD: switching to console #%d\n",vc_num);
                shiftstate &= ~(L_CTRL|R_CTRL|L_ALT|R_ALT);
		user_vc_switch = 1;
                vt_activate(vc_num);
		return 1;
             }
	     /* Slang & X can have C-A-Fx */
             return 0;
         
       case KEY_LEFT:
       case KEY_RIGHT:
       case KEY_UP:
       case KEY_DOWN:
       case KEY_HOME:
       case KEY_END:
             mouse_keyboard(key);        /* mouse emulation keys */
             return 1;
         
      }
   }
   return 0;
}

/***********************************************************************************************
 * High-level interface functions 
 ***********************************************************************************************/

/* 
 * DANG_BEGIN_FUNCTION putrawkey
 *
 * This function sends a raw keycode byte, e.g. read directly from the hardware,
 * to DOS. It is both queued for the port60h emulation and processed for the
 * BIOS keyboard buffer, using the national translation tables etc.
 *
 * For DOS applications using int16h we will therefore not have to load
 * KEYB.EXE, others (e.g. games) need their own drivers anyway.
 *
 * This function is used if we are at the console and config.rawkeyboard=on.
 *
 * DANG_END_FUNCTION
 */

/*
 * the main task of putrawkey() is to 'collect' keyboard bytes (e.g.
 * 0xe0 prefixes) until it thinks it has assembled an entire keyboard
 * event. The combined keycode is then both translated and stored
 * in the 'raw' field of the queue.
 */

void putrawkey(t_rawkeycode code) {
   Boolean make,is_shift,is_accent=0;
   t_scancode scan;
   t_keysym key;
   Bit16u bios_key;
   uchar ascii;

   k_printf("KBD: putrawkey(%x) called\n",(int)code);

   if (code==0xe0 || code==0xe1) {
      rawprefix=code;
      return;
   }

   if (rawprefix==0xe1) {     /* PAUSE key: expext 2 scancode bytes */
      rawprefix = 0xe100 | code;
      return;
   }
   
   scan = (rawprefix<<8) | code;
   rawprefix=0;

   if ((scan&0xff0000) == 0xe10000)
      k_printf("KBD: E1 scancode 0x%06x\n",(int)scan);

   /* for BIOS, ignore the fake shift scancodes 0xe02a, 0xe0aa 0xe036, 0xe0b6
      sent by the keyboard for fn/numeric keys */

   if (scan==0xe02a || scan==0xe0aa || scan==0xe036 || scan==0xe0b6) {
      bios_key=0;
   }
   else {
      if (scan==_SCN_PAUSE_MAKE) {
         key = KEY_PAUSE;
         make = 1;
      }
      else if (scan==_SCN_PAUSE_BREAK) {
         key = KEY_PAUSE;
         make = 0;
      }
      else {
         key = scan & 0xff7f;
         make = ((scan & 0x80) == 0);
      }

      if (make)
         if (handle_dosemu_keys(key)) return;

      is_shift = do_shift_keys(make,key);

      ascii = (make && !is_shift) ? translate(key, &is_accent) : (is_accent=0);

      /* quick hack: translate ALT+letter keys */
      if (   (config.keytable->flags & KT_USES_ALTMAP) && (shiftstate & ALT)
          && config.keytable->key_map[key] >= 'a' && config.keytable->key_map[key] <= 'z')
      {
        key = ascii_keys[config.keytable->key_map[key] - 0x20];
      }

#if 1     
      k_printf("KBD: in putrawkey(): make=%d is_shift=%d key=%08x isaccent=%d ascii=0x%02x\n",
               make,is_shift,key,is_accent,ascii);
#endif
      
      /* translate to a BIOS (soft) scancode. For accented characters, the BIOS
       * code is 0, i.e. bios_key == (0<<8)|ascii == ascii
       */
      bios_key = is_accent ? ascii : make_bios_code(make,key,ascii);
   }
   write_queue(bios_key,shiftstate,scan);
   
   /* although backend_run() is called periodically anyway, for faster response
    * it's a good idea to call it straight away.
    */
   backend_run();
}

static void put_ascii_key(Boolean make, uchar ascii)
{
	int old_alt_num_buffer;
	int amount_to_add, one,two,three;
	t_shiftstate alt_shiftstate;
	
	static const t_keysym num_key[] = 
	{ KEY_PAD_0, KEY_PAD_1, KEY_PAD_2, KEY_PAD_3, KEY_PAD_4, 
	  KEY_PAD_5, KEY_PAD_6, KEY_PAD_7, KEY_PAD_8, KEY_PAD_9 };

	k_printf("KBD: put_ascii_key(%s, %04x, '%c') called\n",
		 make?"PRESS":"RELEASE", ascii, ascii?ascii:' ');
	if (!make) {
		return;
	}
	/* insert these keys as alt number combinations,
	 * after appropriately setting the shift state.
	 */
	alt_shiftstate = shiftstate &ALT;
	if (alt_shiftstate & ALT) {
		old_alt_num_buffer = alt_num_buffer;
	} else {
		old_alt_num_buffer = 0;
		putkey(PRESS, KEY_L_ALT, 0);
	}
	amount_to_add = (ascii - old_alt_num_buffer) & 0xff;
	three = amount_to_add %10; amount_to_add /= 10;
	two = amount_to_add %10;  amount_to_add /= 10;
	one = amount_to_add %10;

	putkey(PRESS, num_key[one], one + '0');
	putkey(RELEASE, num_key[one], 0);
	putkey(PRESS, num_key[two], two + '0');
	putkey(RELEASE, num_key[two], 0);
	putkey(PRESS, num_key[three], three + '0');
	putkey(RELEASE, num_key[three], 0);

	if (!(alt_shiftstate & ALT)) {
		putkey(RELEASE, KEY_L_ALT, 0);
		return;  /* LEAVE */
	}
	/* release the alt keys to force the ascii code to appear */
	if (alt_shiftstate &L_ALT) {
		putkey(RELEASE, KEY_L_ALT, 0);
	}
	if (alt_shiftstate &R_ALT) {
		putkey(RELEASE, KEY_R_ALT, 0);
	}
	/* press the alt keys again */
	if (alt_shiftstate &L_ALT) {
		putkey(PRESS, KEY_L_ALT, 0);
	}
	if (alt_shiftstate &R_ALT) {
		putkey(PRESS, KEY_R_ALT, 0);
	}
	
	/* press keys to restore the old alt_num_buffer */
	amount_to_add = old_alt_num_buffer;
	three = amount_to_add %10; amount_to_add /= 10;
	two = amount_to_add %10;  amount_to_add /= 10;
	one = amount_to_add %10;

	putkey(PRESS, num_key[one], one + '0');
	putkey(RELEASE, num_key[one], 0);
	putkey(PRESS, num_key[two], two + '0');
	putkey(RELEASE, num_key[two], 0);
	putkey(PRESS, num_key[three], three + '0');
	putkey(RELEASE, num_key[three], 0);

}

/* 
 * DANG_BEGIN_FUNCTION putkey
 *
 * This does all the work of sending a key event to DOS.
 *   scan  - the keycode, one of the KEY_ constants from new-kbd.h
 *   make  - TRUE for key press, FALSE for release
 *   ascii - the key's ascii value, or 0 for none.
 *
 * Applications using int16h will always see the ASCII code passed here
 * independently of the scancode, so no character translation needs/should 
 * be done. As DOS expects characters in IBM Extended ASCII, the keyboard
 * clients may have to do ISO->IBM character translation or similar!
 *
 * An emulated hardware scancode is also sent to port60h.
 *
 * Note that you have to send both MAKE (press) and BREAK (release) events.
 * If no BREAK codes are available (e.g. terminal mode), send them
 * immediately after the MAKE codes.
 * Also, shift keys should be sent (with ascii = 0).
 *
 * DANG_END_FUNCTION
 */

void putkey(Boolean make, t_keysym key, uchar ascii) 
{
   Boolean is_shift;
   Bit16u shiftprefix;
   t_scancode scan;
   Bit16u bios_key;

#if 0
   printf("KBD: putkey(%s, %04x, '%c') called\n",
            make?"PRESS":"RELEASE",(unsigned int)key,ascii?ascii:' ');
#endif
   k_printf("KBD: putkey(%s, %04x, '%c') called\n",
            make?"PRESS":"RELEASE",(unsigned int)key,ascii?ascii:' ');

   if (make)
      if (handle_dosemu_keys(key)) return;

   if (key == 0) { /* Use an alt nnn combination for the key */
      put_ascii_key(make, ascii); 
      return;
   }

   /* process the shift keys */
   is_shift = do_shift_keys(make,key);
   /*
    * now create an emulated string of hardware scancodes, which
    * will go into the raw_queue. As our 'keysyms' are defined to
    * be hardware scancodes, conversion only needs to be done for
    * a few special cases.
    */

   shiftprefix=0;
   scan=key;

   if (key==KEY_PRTSCR) {
      if (!(shiftstate & (CTRL|SHIFT))) {
         shiftprefix=0xe02a;
      }
   }
   else if (key==KEY_PAUSE) {
      scan=_SCN_PAUSE_MAKE;
   }
   else if (key==KEY_PAD_SLASH || (key>=0xe047 && key<=0xe053)) {
      if (shiftstate&NUM_LOCK) shiftprefix=0xe02a;
      if (shiftstate&L_SHIFT)  shiftprefix=0xe0aa;
      if (shiftstate&R_SHIFT)  shiftprefix=0xe0b6;
   }

   /* convert to BREAK code if appropriate */
   if (!make) {
      /* a special case, as usual... */
      if (scan==_SCN_PAUSE_MAKE) {
         scan=_SCN_PAUSE_BREAK;
      }
      else {
         /* anything else has only one actual keycode, which sits in 
            byte 0. hopefully.
          */
         if ((scan&0xffffff00) != 0xe000 && (scan&0xffffff00) != 0) {
            k_printf("KBD: dropping strange keycode %08x\n",(unsigned int)scan);
            return;
         }

         if (scan&0x80)
            k_printf("KBD: already BREAK code ??? !\n");

         scan |= 0x80;
      }
   }

/* send it all to the queue */

   bios_key = make_bios_code(make,key,ascii);

   if (make && shiftprefix)
      write_queue(0,shiftstate,shiftprefix);

   write_queue(bios_key,shiftstate,scan);

   if (!make && shiftprefix)
      write_queue(0,shiftstate,shiftprefix^0x80);

   backend_run();
}

/* 
 * DANG_BEGIN_FUNCTION set_shiftstate
 *
 * This simply sets the keyboard server's shift state.
 *
 * USE WITH CAUTION: this changes the keyboard flags without generating the
 * appropriate shift key make/break codes that normally come along with such
 * changes. This function is mostly intended for start-up shiftstate synchronisation.
 *
 * Note also that you can't simply write to the shiftstate variable instead of using
 * this function.
 *
 * DANG_END_FUNCTION
 */

void set_shiftstate(t_shiftstate s) {
   if (s!=shiftstate) {
      shiftstate = s;
      write_queue(0,s,0);     /* put it in the queue so it's copied to DOS */
   }
} 

int keyb_server_reset(void) {

   k_printf("KBD: keyb_server_reset()\n");

/* for now, we just assume that NumLock=on, Caps=off, Scroll=off. If the raw frontend
 * is running, it'll tell us the actual shiftstate later, otherwise we'll have to
 * live with this.
 */
   shiftstate=NUM_LOCK;
   rawprefix=0;
   accent=0;
   alt_num_buffer=0;
   
   backend_reset();
   
   return TRUE;
}

int keyb_server_init(void) {
   k_printf("KBD: keyb_server_init()\n");
   return keyb_server_reset();
}

void keyb_server_close(void) {
   k_printf("KBD: keyb_server_close()\n");
}

void keyb_server_run(void) {
   backend_run();
}
 
