/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * video/terminal.c - contains the video-functions for terminals 
 *
 * This module has been extensively updated by Mark Rejhon at: 
 * ag115@freenet.carleton.ca.
 *
 * Please send patches and bugfixes for this module to the above Email
 * address.  Thanks!
 * 
 * Now, who can write a VGA emulator for SVGALIB and X? :-)
 */

/* Both FAST and NCURSES support has been replaced by calls to the SLang
 * screen management routines.  Now, METHOD_FAST and METHOD_NCURSES are both
 * synonyms for SLang.  The result is a dramatic increase in speed and the 
 * code size has dropped by a factor of three.
 * The slang library is available from amy.tch.harvard.edu in pub/slang.
 * John E. Davis (Nov 17, 1994).
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>

#include "bios.h"
#include "emu.h"
#include "memory.h"
#include "video.h" 
#include "terminal.h"
#include "keyb_clients.h"
#include "serial.h"
#include <slang.h>

/* The interpretation of the DOS attributes depend upon if the adapter is 
 * color or not.
 * If color:
 *   Bit: 0   Foreground blue
 *   Bit: 1   Foreground green
 *   Bit: 2   Foreground red
 *   Bit: 3   Foreground bold (intensity bit)
 *   Bit: 4   Background blue
 *   Bit: 5   Background green
 *   Bit: 6   Background red
 *   Bit: 7   blinking bit  (see below)
 * 
 * and if mono bits 3 and 7 have the same interpretation.  However, the 
 * Foreground and Background bits have a different interpretation:
 * 
 *    Foreground   Background    Interpretation
 *      111          000           Normal white on black
 *      000          111           Reverse video (white on black)
 *      000          000           Invisible characters
 *      001          000           Underline
 *     anything else is invalid.
 */    
static int BW_Attribute_Map[256];
static int Color_Attribute_Map[256];

static int *Attribute_Map;
/* if negative, char is invisible */

int cursor_blink = 1;
static unsigned char *The_Charset = charset_latin;
static int slang_update (void);

static int Slsmg_is_not_initialized = 1;
static int Use_IBM_Codes = 0;

/* I think this is what is assumed. */
static int Rows = 25;
static int Columns = 80;

#if SLANG_VERSION < 10000
static void sl_exit_error (char *err)
{
   error("%s\n", err);
   leavedos (32);
}
#else
#include <stdarg.h>
static void sl_exit_error (char *fmt, va_list args)
{
   verror (fmt, args);
   leavedos (32);
}
#endif

void get_screen_size (void)
{
  struct winsize ws;		/* buffer for TIOCSWINSZ */

   SLtt_Screen_Rows = 0;
   SLtt_Screen_Cols = 0;
   if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) >= 0) 
     {
	SLtt_Screen_Rows = ws.ws_row;
	SLtt_Screen_Cols = ws.ws_col;
     }
   if ((SLtt_Screen_Rows <= 0) 
       || (SLtt_Screen_Cols <= 0))
     {
	SLtt_Screen_Cols = 80;
	SLtt_Screen_Rows = 24;
     }
   Rows = SLtt_Screen_Rows;
   Columns = SLtt_Screen_Cols;
   if (Rows < 25) Rows = 25;
}

static void set_char_set (int cs)
{
   switch (cs)
     {
		 case CHARSET_KOI8:     	
			 if (Use_IBM_Codes) 
				 SLtt_write_string ("\033(K\033(K\r        \r");
			 The_Charset = charset_koi8;
			 Use_IBM_Codes = 0;
			 SLsmg_Display_Eight_Bit = 0x80;
			 break;
			 
      case CHARSET_FULLIBM:
	error("WARNING: 'charset fullibm' doesn't work.  Use 'charset ibm' instead.\n");
	/* The_Charset = charset_fullibm; */
	/* drop */
      case CHARSET_IBM:     	
	The_Charset = charset_ibm;
	Use_IBM_Codes = 1;
 	SLsmg_Display_Eight_Bit = 0x80;
	break;
	
      case CHARSET_LATIN1:
	if (Use_IBM_Codes) 
	  {
	     SLtt_write_string ("\n\033(B\033(B\r         \r");
	  }
	SLsmg_Display_Eight_Bit = 160;
	Use_IBM_Codes = 0;
	The_Charset = charset_latin1;
	break; 

      case CHARSET_LATIN2:
        if (Use_IBM_Codes)
          {
             SLtt_write_string ("\n\033(B\033(B\r         \r");
          }
        SLsmg_Display_Eight_Bit = 160;
        Use_IBM_Codes = 0;
        The_Charset = charset_latin2;
        break;

      case CHARSET_LATIN:
      default:
        if (Use_IBM_Codes)
          {
             SLtt_write_string ("\n\033(B\033(B\r         \r");
          }
        SLsmg_Display_Eight_Bit = 160;
        Use_IBM_Codes = 0;
        The_Charset = charset_latin;
        break;
     }

   
   /* The fact is that this code is used to talk to a terminal.  Control 
    * sequences 0-31 and 128-159 are reserved for the terminal.  Here I fixup 
    * the character set map to reflect this fact (only if not ibmpc codes).
    */
   
#if 0 /* XXX (by solt) this breaks pseudographics.*/
   if (!Use_IBM_Codes) for (i = 0; i < 256; i++)
     {
	if ((The_Charset[i] & 0x7F) < 32) The_Charset[i] |= 32;
     }
#endif
   
    /* The following turns on the IBM character set mode of virtual console
     * The same code is echoed twice, then just in case the escape code
     * not recognized and was printed, erase it with spaces.
     */
   if (Use_IBM_Codes) SLtt_write_string ("\033(U\033(U\r        \r");
}

/* The following initializes the terminal.  This should be called at the
 * startup of DOSEMU if it's running in terminal mode.
 */ 
static int terminal_initialize(void)
{
   SLtt_Char_Type sltt_attr, fg, bg, attr, color_sltt_attr, bw_sltt_attr;
   int is_color = config.term_color;
   int rotate[8];

   v_printf("VID: terminal_initialize() called \n");
   /* I do not know why this routine is called if our update is not
    * called.  Oh well.... 
    */
   if (config.console_video) 
     {
	Slsmg_is_not_initialized = 1;
	return 0;
     }
   Slsmg_is_not_initialized = 0;
   
   /* This maps (r,g,b) --> (b,g,r) */
   rotate[0] = 0; rotate[1] = 4; 
   rotate[2] = 2; rotate[3] = 6;
   rotate[4] = 1; rotate[5] = 5;
   rotate[6] = 3; rotate[7] = 7;
   
   if(no_local_video!=1) {
     Video_term.update_screen = slang_update;
   }
   else
     Video_term.update_screen = NULL;

   SLang_Exit_Error_Hook = sl_exit_error;
   if( !DOSemu_Slang_Got_Terminfo )
   {
      SLtt_get_terminfo ();
      DOSemu_Slang_Got_Terminfo = 1;
   }
   
   get_screen_size ();

   /* Now set the variables li and co back to their dos defaults. */

#if 0
   if (li < 25)
     {
        li = Rows;
        co = Columns;
     }
#endif
   
   SLtt_Use_Blink_For_ACS = 1;
   SLtt_Blink_Mode = 1;
   
   SLtt_Use_Ansi_Colors = is_color;
   
   if (is_color) Attribute_Map = Color_Attribute_Map;
   else Attribute_Map = BW_Attribute_Map;

#if SLANG_VERSION < 10000
   if (!SLsmg_init_smg ())
#else
   if (SLsmg_init_smg() == -1) 
#endif
     {
       error ("Unable to initialze SMG routines.");
       leavedos(32);
     }
   
   for (attr = 0; attr < 256; attr++)
     {
	BW_Attribute_Map[attr] = Color_Attribute_Map[attr] = attr;
#if 1   /* As Jim Powers <powers@dtedi.wpafb.af.mil> reported,
	 * this leads to pure "black and white" on dumb terminals.
	 * Commenting out the below statement results in getting visual
	 * attributes on dumb terminal, but produces an invers screen image.
	 * Forcing a configuration for a monochrome video adapter
	 * (config.cardtype = CARD_MDA) solves this problem, but leads to
	 * other problems.
	 * ... have think more deeply about this.  --Hans
	 */
	BW_Attribute_Map[attr] = 0;
#endif
	
	sltt_attr = 0;
	if (attr & 0x80) sltt_attr |= SLTT_BLINK_MASK;
	if (attr & 0x08) sltt_attr |= SLTT_BOLD_MASK;
	
	bw_sltt_attr = color_sltt_attr = sltt_attr;
	
	bg = (attr >> 4) & 0x07;
	fg = (attr & 0x07);
	
	/* color information */
	color_sltt_attr |= (rotate[bg] << 16) | (rotate[fg] << 8);
	SLtt_set_color_object (attr, color_sltt_attr);
	
	/* Monochrome information */
	if ((fg == 0x01) && (bg == 0x00)) bw_sltt_attr |= SLTT_ULINE_MASK;
	if (bg & 0x7) bw_sltt_attr |= SLTT_REV_MASK;
	else if (fg == 0)
	  {
	     /* Invisible */
	     BW_Attribute_Map [attr] = -attr;
	  }
	
	SLtt_set_mono (attr, NULL, bw_sltt_attr);
     }
   
   /* object 0 is special.  It is normal video.  Lets fix that now. */   
   BW_Attribute_Map[0x7] = Color_Attribute_Map[0x7] = 0;
   
   SLtt_set_color_object (0, 0x000700);
   SLtt_set_mono (0, NULL, 0x000700);
   
   SLsmg_refresh ();
   /* This goes after the refresh because it might try 
    * resetting the character set.
    */
   set_char_set (config.term_charset);
   return 0;
}

static void terminal_close (void)
{
   v_printf("VID: terminal_close() called\n");
   if (Slsmg_is_not_initialized == 0)
     {
	SLsmg_gotorc (SLtt_Screen_Rows - 1, 0);
	SLsmg_refresh ();
	SLsmg_reset_smg ();
	if (Use_IBM_Codes) 
	  {
	     SLtt_write_string ("\n\033(B\033(B\r         \r");
	     SLtt_flush_output ();
	  }
	else putc ('\n', stdout);
	Slsmg_is_not_initialized = 1;
     }
}

#if 0
static void v_write(int fd, unsigned char *ch, int len)
{
  if (!config.console_video)
    DOS_SYSCALL(write(fd, ch, len));
  else
    error("(video) v_write deferred for console_video\n");
}
#endif

static char *Help[] = 
{
   "NOTE: The '^@' defaults to Ctrl-^, see dosemu.conf 'terminal {escchar}' .",
   "Function Keys:",
   "    F1: ^@1      F2: ^@2 ...  F9: ^@9    F10: ^@0   F11: ^@-   F12: ^@=",
   "Key Modifiers:",
   "    Normal:  ^@s SHIFT KEY, ^@a ALT KEY, ^@c CTRL KEY, ^@g ALTGR KEY",
   "    Sticky:  ^@S SHIFT KEY, ^@A ALT KEY, ^@C CTRL KEY, ^@G ALTGR KEY",
   "Substitute keys:",
   "    ^@K0 Insert, ^@K7 Home, ^@K3 PgDn, ^@Kd Delete, ^@Kp PrtScn, etc.",
   "Notes:",
   "    The numbers are the same as those on the key on the numeric keypad.",
   "    To cancel the sticky key, press it again or use ^@ Space.",
   "Examples:",
   "    Pressing ^@s followed by ^@3 results in SHIFT-F3.",
   "    Pressing ^@C Up Up Up ^@C results in Ctrl-Up Ctrl-Up Ctrl-Up.",
   "    Pressing ^@c ^@K1  results in Ctrl-End",
   "Miscellaneous:",
   "    ^@^R : Redraw display      ^@^L : Redraw the display.",
   "    ^@^Z : Suspend dosemu      ^@b  : Select BEST monochrome mode.",    
   "    ^@ Up Arrow: Force the top of DOS screen to be displayed.",
   "    ^@ Dn Arrow: Force the bottom of DOS screen to be displayed.",
   "    ^@ Space: Reset Sticky keys and Panning to automatic panning mode.",
   "    ^@? or ^@h:  Show this help screen.",
   "    ^@^@:  Send the ^@ character to dos.",
   "",
   "PRESS THE SPACE BAR TO CONTINUE---------",
   NULL
};

static void show_help (void)
{
   int i;
   char *s;
   SLsmg_cls ();
   
   i = 0;
   while ((s = Help[i]) != NULL) 
     {
	if (*s)
	  {
	     SLsmg_gotorc (i, 0);
	     SLsmg_write_string (s);
	  }
	i++;
     }
   memset ((char *) prev_screen, 0xFF, 2 * Rows * Columns);
   SLsmg_refresh ();
}

   
   

/* global variables co and li determine the size of the screen.  Also, use
 * the short pointers prev_screen and screen_adr for updating the screen.
 */
static int slang_update (void)
{
   register unsigned short *line, *prev_line, *line_max, char_attr;
   int i, pn, sn, row_len = Columns * 2;
   unsigned char buf[256], *bufp;
   int last_obj = 1000, this_obj;
   int changed = 0;
   int imin, imax;
   
   static int last_row, last_col, help_showing;
   static char *last_prompt = NULL;
   
   SLtt_Blink_Mode = char_blink;
   
   if (DOSemu_Slang_Show_Help) 
     {
	if (help_showing == 0) show_help ();
	help_showing = 1;
	return 1;
     }
   help_showing = 0;
   
   if (((DOSemu_Terminal_Scroll == 0) && 
	(cursor_row < SLtt_Screen_Rows))
       || (DOSemu_Terminal_Scroll == -1))
     {
	imin = 0;
	imax = SLtt_Screen_Rows;
     }
   else 
     {
	imax = Rows;
	imin = imax - SLtt_Screen_Rows;
	if (imin < 0) imin = 0;
     }
   
   pn = 0;
   sn = imin * Columns;
   for (i = imin; i < imax; i++)
     {
#if 0
	if (memcmp(screen_adr + sn, prev_screen + pn, row_len))
#else
	if (MEMCMP_DOS_VS_UNIX(screen_adr + sn, prev_screen + pn, row_len))
#endif
	  {
	     line = screen_adr + sn;
	     prev_line = prev_screen + pn;
	     line_max = line + Columns;
	     bufp = buf;
	     SLsmg_gotorc (i - imin, 0);
	     while (line < line_max)
	       {
		  /* *prev_line = char_attr = *line++; */
		  *prev_line = char_attr = READ_WORD(line++);
		  prev_line++;
		  this_obj = Attribute_Map[(unsigned int) (char_attr >> 8)];
		  if (this_obj != last_obj)
		    {
		       if (bufp != buf) 
		       	 SLsmg_write_nchars ((char *) buf, (int) (bufp - buf));
		       bufp = buf;
		       SLsmg_set_color (abs(this_obj));
		       last_obj = this_obj;
		    }
		  /* take care of invisible character */
		  if (this_obj < 0) char_attr = (unsigned short) ' ';
		  *bufp++ = The_Charset [char_attr & 0xFF];
		  changed = 1;
	       }
	     SLsmg_write_nchars ((char *) buf, (int) (bufp - buf));
	  }
	pn += Columns;
	sn += Columns;
     }
   
   if (changed || (last_col != cursor_col) || (last_row != cursor_row)
       || (DOSemu_Keyboard_Keymap_Prompt != last_prompt))
     {
	if (DOSemu_Keyboard_Keymap_Prompt != NULL)
	  {
	     last_row = SLtt_Screen_Rows - 1;
	     SLsmg_gotorc (last_row, 0);
	     last_col = strlen (DOSemu_Keyboard_Keymap_Prompt);
	     SLsmg_set_color (0);
	     SLsmg_write_nchars (DOSemu_Keyboard_Keymap_Prompt, last_col);
	     memset ((char *) (prev_screen + (last_row * Columns)),
		     Columns * 2, 0xFF);
	     
	     if (*DOSemu_Keyboard_Keymap_Prompt == '[')
	       {
		  /* Sticky */
		  last_row = cursor_row - imin;
		  last_col = cursor_col;
	       }
	     else last_col -= 1;
	  }
	else if (cursor_blink == 0) last_row = last_col = 0;
	else
	  {
	     last_row = cursor_row - imin;
	     last_col = cursor_col;
	  }
	
	SLsmg_gotorc (last_row, last_col);
	SLsmg_refresh ();
	last_prompt = DOSemu_Keyboard_Keymap_Prompt;
     }
   return 1;
}

void dos_slang_redraw (void)
{
   if (Slsmg_is_not_initialized) return;
   
   SLsmg_cls ();
   memset ((char *) prev_screen, 0xFF, 2 * Rows * Columns);
   slang_update ();
}

void dos_slang_suspend (void)
{
   /*
   if (Slsmg_is_not_initialized) return;
   terminal_close();
   keyboard_close();
   
   terminal_initialize();
   keyboard_init();
    */
}


void dos_slang_smart_set_mono (void)
{
   int i, max_attr;
   unsigned int attr_count [256], max_count;
   register unsigned short *s, *smax;
   
   Attribute_Map = BW_Attribute_Map;
   s = screen_adr;
   smax = s + Rows * Columns;
   
   for (i = 0; i < 256; i++) attr_count[i] = 0;
   
   while (s < smax)
     {
	attr_count[*s >> 8] += 1;
	s++;
     }
   
   max_attr = 0;
   max_count = 0;
   
   for (i = 0; i < 256; i++)
     {
	Attribute_Map[i] = 1;
	if (attr_count[i] > max_count) 
	  {
	     max_attr = i;
	     max_count = attr_count[i];
	  }
     }
   
   SLtt_normal_video ();
   
   Attribute_Map [max_attr] = 0;
   SLtt_Use_Ansi_Colors = 0;
   
   SLtt_set_mono (1, NULL, SLTT_REV_MASK);
   SLtt_set_mono (0, NULL, 0);
   
   memset ((unsigned char *) prev_screen, 0xFF, 
	   2 * SLtt_Screen_Rows * SLtt_Screen_Cols);
   
   set_char_set (config.term_charset /*CHARSET_LATIN*/);
   
   SLsmg_cls ();
}




#define term_setmode NULL
#define term_update_cursor NULL

struct video_system Video_term = {
   0,                /* is_mapped */
   terminal_initialize, 
   terminal_close,      
   term_setmode,      
   slang_update,
   term_update_cursor
};


