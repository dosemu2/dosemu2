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

#include "bios.h"
#include "emu.h"
#include "memory.h"
#include "video.h" 
#include "terminal.h"
#include "slang.h"

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
static int Attribute_Map[256];		       /* if negative, char is invisible */

extern char *DOSemu_Keyboard_Keymap_Prompt;

int cursor_blink = 1;
static unsigned char *The_Charset = charset_latin;
static int slang_update (void);

static int Slsmg_is_not_initialized = 0;
static int Use_IBM_Codes = 0;

/* The following initializes the terminal.  This should be called at the
 * startup of DOSEMU if it's running in terminal mode.
 */ 
int
terminal_initialize()
{
   SLtt_Char_Type sltt_attr, fg, bg, attr;
   int is_color = config.term_color, i;
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
     
   /* This maps (r,g,b) --> (b,g,r) */
   rotate[0] = 0; rotate[1] = 4; 
   rotate[2] = 2; rotate[3] = 6;
   rotate[4] = 1; rotate[5] = 5;
   rotate[6] = 3; rotate[7] = 7;
   
   Video_term.update_screen = slang_update;
   
   SLtt_get_terminfo();
   SLtt_Screen_Rows = 25;   /* was: li */
   SLtt_Screen_Cols = 80;   /* was: co */
   
   SLtt_Use_Blink_For_ACS = 1;
   SLtt_Blink_Mode = 1;
   SLtt_Use_Ansi_Colors = is_color;
   
   SLsmg_init_smg ();
   
   for (attr = 0; attr < 256; attr++)
     {
	Attribute_Map[attr] = attr;
	sltt_attr = 0;
	if (attr & 0x80) sltt_attr |= SLTT_BLINK_MASK;
	if (attr & 0x08) sltt_attr |= SLTT_BOLD_MASK;
	bg = (attr >> 4) & 0x07;
	fg = (attr & 0x07);
	if (is_color)
	  {
	     sltt_attr |= (rotate[bg] << 16) | (rotate[fg] << 8);
	     SLtt_set_color_object (attr, sltt_attr);
	  }
	else
	  {
	     if ((fg == 0x01) && (bg == 0x00)) sltt_attr |= SLTT_ULINE_MASK;
	     if (bg & 0x7) sltt_attr |= SLTT_REV_MASK;
	     else if (fg == 0)
	       {
		  /* Invisible */
		  Attribute_Map[attr] = -attr;
	       }
	     SLtt_set_mono (attr, NULL, sltt_attr);
	  }
     }
   
   /* object 0 is special.  It is normal video.  Lets fix that now. */   
   Attribute_Map[0x7] = 0;
   if (is_color) SLtt_set_color_object (0, 0x000700);
   else SLtt_set_mono (0, NULL, 0x000700);
   
   SLsmg_refresh ();
   
   switch (config.term_charset) 
     {
      case CHARSET_FULLIBM:
	error("WARNING: 'charset fullibm' doesn't work.  Use 'charset ibm' instead.\n");
	/* The_Charset = charset_fullibm; */
	/* drop */
      case CHARSET_IBM:     	
	The_Charset = charset_ibm;
	Use_IBM_Codes = 1;
	break;
      case CHARSET_LATIN:
      default:
	The_Charset = charset_latin;
	break; 
     }
   
   /* The fact is that this code is used to talk to a terminal.  Control 
    * sequences 0-31 and 128-159 are reserved for the terminal.  Here I fixup 
    * the character set map to reflect this fact (only if not ibmpc codes).
    */
   
   if (!Use_IBM_Codes) for (i = 0; i < 256; i++)
     {
	if ((The_Charset[i] & 0x7F) < 32) The_Charset[i] |= 32;
     }
   
    /* The following turns on the IBM character set mode of virtual console
     * The same code is echoed twice, then just in case the escape code
     * not recognized and was printed, erase it with spaces.
     */
   if (Use_IBM_Codes) SLtt_write_string ("\033(U\033(U\r        \r");

   return 0;
}

void terminal_close (void)
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
     }
}

void
v_write(int fd, unsigned char *ch, int len)
{
  if (!config.console_video && !config.usesX)
    DOS_SYSCALL(write(fd, ch, len));
  else
    error("ERROR: (video) v_write deferred for console_video\n");
}

/* global variables co and li determine the size of the screen.  Also, use
 * the short pointers prev_screen and screen_adr for updating the screen.
 */
static int slang_update (void)
{
   register unsigned short *line, *prev_line, *line_max, char_attr;
   int i, n, row_len = co * 2;
   unsigned char buf[256], *bufp;
   int last_obj = 1000, this_obj;
   int changed = 0;
   
   static int last_row, last_col;
   static char *last_prompt = NULL;
   
   SLtt_Blink_Mode = char_blink;
   line = screen_adr;
   n = 0;
   for (i = 0; i < li; i++)
     {
	if (memcmp (screen_adr + n, prev_screen + n, row_len))
	  {
	     line = screen_adr + n;
	     prev_line = prev_screen + n;
	     line_max = line + co;
	     bufp = buf;
	     SLsmg_gotorc (i, 0);
	     while (line < line_max)
	       {
		  *prev_line = char_attr = *line++;
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
	n += co;
     }
   
   if (changed || (last_col != cursor_col) || (last_row != cursor_row)
       || (DOSemu_Keyboard_Keymap_Prompt != last_prompt))
     {
	if (DOSemu_Keyboard_Keymap_Prompt != NULL)
	  {
	     last_row = li - 1;
	     SLsmg_gotorc (last_row, 0);
	     last_col = strlen (DOSemu_Keyboard_Keymap_Prompt);
	     SLsmg_set_color (0);
	     SLsmg_write_nchars (DOSemu_Keyboard_Keymap_Prompt, last_col);
	     last_col -= 1;
	     memset ((char *) (prev_screen + (last_row * co)),
		     co * 2, 0xFF);
	  }
	else if (cursor_blink == 0) last_row = last_col = 0;
	else
	  {
	     last_row = cursor_row;
	     last_col = cursor_col;
	  }
	
	SLsmg_gotorc (last_row, last_col);
	SLsmg_refresh ();
	last_prompt = DOSemu_Keyboard_Keymap_Prompt;
     }
   return 1;
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
