/* 
 * video/terminal.c - contains the video-functions for terminals 
 *
 * This module has been extensively updated by Mark Rejhon at: 
 * ag115@freenet.carleton.ca  
 *
 * Please send patches and bugfixes for this module to the above Email
 * address.  Thanks!
 *
 * Now, who can write a VGA emulator for SVGALIB and X? :-)
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "bios.h"
#include "emu.h"
#include "memory.h"
#include "video.h"
#include "terminal.h" 

/* The default character set is to use the latin character set */
unsigned char *chartrans = charset_latin;
unsigned char *attrtrans = attrset_normal;

int colormap[8][8];
int attrlookup[256];
WINDOW *win;

#define NBUFSIZE  256
chtype noutbuf[NBUFSIZE + 5];
chtype *noutp = noutbuf;

#define CHBUFSIZE 1024
unsigned char outbuf[CHBUFSIZE + 5];
unsigned char *outp = outbuf;

int cursor_row, cursor_col, cursor_blink, char_blink;
int ibm_codes;

int ncurses_update();

/* The following initializes the terminal.  This should be called at the
 * startup of DOSEMU if it's running in terminal mode.
 */ 
int
terminal_initialize()
{
  int i,j,fore,back,attr,pairnum;
  
  cursor_row = 0;
  cursor_col = 0;
  cursor_blink = 1;
  char_blink = 1;

  v_printf("VID: terminal_initialize() called \n");
  if (config.term_charset == CHARSET_FULLIBM) {
     error("WARNING: 'charset fullibm' doesn't work.  Use 'charset ibm' instead.\n");
     config.term_charset = CHARSET_IBM;
  }

  switch (config.term_charset) {
  case CHARSET_IBM:     	
    chartrans = charset_ibm;
    ibm_codes = 1;
    break;
  case CHARSET_FULLIBM:
    chartrans = charset_fullibm; 
    ibm_codes = 1;
    break;
  case CHARSET_LATIN:
  default:
    chartrans = charset_latin;
    ibm_codes = 0;
    break; 
  }

  switch (config.term_color) {
  case COLOR_XTERM:
    attrtrans = attrset_xterm;
    break;
  case COLOR_NORMAL: 
  default:
    attrtrans = attrset_normal;
    break;
  }

  initscr();
  raw();
  nonl();

  if (config.console_video || config.usesX) {
    config.term_method = METHOD_FAST;
    config.term_color = 0;
    return 0;
  }
  if (config.term_method != METHOD_NCURSES) {
    fprintf(stdout,"\033[H\033[2J");
  }

  if (ibm_codes) {
    /* The following turns on the IBM character set mode of virtual console
     * The same code is echoed twice, then just in case the escape code
     * not recognized and was printed, erase it with spaces.
     */
    fprintf(stdout,"\033(U\033(U\r        \r");
  }
  
  if (config.term_method != METHOD_NCURSES) return 0;  

  Video_term.update_screen = ncurses_update;
  
  if (!has_colors()) config.term_color = 0;
  if (config.term_color) start_color();
  win = newwin(0,0,0,0);
  keypad(win,TRUE);
  wclear(win);
  wrefresh(win);

  if (config.term_color) {
    fore = 0;
    back = 0;
    pairnum = 1;
    for (i = 0; i < 8; i++) {
      switch (i) {
      case 0: back = COLOR_BLACK;   break;
      case 1: back = COLOR_BLUE;    break;
      case 2: back = COLOR_GREEN;   break;
      case 3: back = COLOR_CYAN;    break;
      case 4: back = COLOR_RED;     break;
      case 5: back = COLOR_MAGENTA; break;
      case 6: back = COLOR_YELLOW;  break;
      case 7: back = COLOR_WHITE;   break;
      }
      for (j = 0; j < 8; j++) {
        switch (j) {
        case 0: fore = COLOR_BLACK;   break;
        case 1: fore = COLOR_BLUE;    break;
        case 2: fore = COLOR_GREEN;   break;
        case 3: fore = COLOR_CYAN;    break;
        case 4: fore = COLOR_RED;     break;
        case 5: fore = COLOR_MAGENTA; break;
        case 6: fore = COLOR_YELLOW;  break;
        case 7: fore = COLOR_WHITE;   break;
        }
        if ((i != 0) || (j != 7)) {
          init_pair(pairnum,fore,back);
          colormap[j][i] = pairnum;
          pairnum++;
        }
      }
    } 
    colormap[7][0] = 0;         /*colormap[7][0];*/

    /* Initialize attribute lookup tables */
    for (i = 0; i < 256; i++) {
      if ((i & 0x77) == 0x07)
        attr = A_NORMAL | A_DIM;
      else
      	attr = COLOR_PAIR(colormap[i & 7][(i >> 4) & 7]);
      if (i & 0x80) attr |= A_BLINK;
      if (i & 0x08) attr |= A_BOLD;
      attrlookup[i] = attr;
    }
  }
  else {
    for (i = 0; i < 256; i++) {
      attr = 0;
      if (!(i & 0xF8)) attr |= A_NORMAL;
      if (!(i & 0x07)) attr |= A_INVIS;
      if (i & 0x70) attr |= A_REVERSE;
      if (i & 0x08) attr |= A_BOLD;
      if (i & 0x80) attr |= A_BLINK;
      attrlookup[i] = attr;
    }
  }  
  return 0;
}

void 
terminal_close()
{
  v_printf("VID: terminal_close() called\n");
  if (config.term_method != METHOD_NCURSES) {
    fprintf(stdout,"\033[?25h\033[%dH",li);
  }
  else {
    mvcur(-1,-1,li,0);
    endwin();
  }
  fprintf(stdout,"\n");
  if (ibm_codes) {
    /* The following turns off the IBM character set mode of virtual console
     * The same code is echoed twice, then just in case the escape code
     * not recognized and was printed, erase it with spaces.
     */
    printf("%s","\033(B\033(B\r         \r");
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


/*
 * This is a much-improved restore_screen, programmed by Mark D. Rejhon.
 * Based on Andrew Tridgell code.  Updated to do more optimization.
 * It is now much faster than it was in pre0.51pl24 and earlier,
 * and actually works more properly. It's also better looking over
 * a 2400 baud modem.  However, there is no buffer overflow checking
 * yet :-(
 */



/* This is the screen updating routine that uses NCURSES as the display
 * engine.  Because of NCURSES' overheads, this routine is much slower than 
 * the ANSI engine which writes data directly to the display.
 */
int
ncurses_update()
{

  /* In NCURSES display mode, there are speed concerns as to efficiently pass 
   * characters to NCURSES.  NOUT outputs a character and attribute into  
   * the buffer, and NFLUSH sends them to NCURSESS.  The NCURSES function
   * "waddchnstr" is a fast unformatted 16-bit character write routine 
   * where each character is 16 bits instead of the usual 8 bits, because 
   * both the character and color are contained in the same code.
   */
  #define NOUT(c)   *(noutp++) = (c); \
                    if (noutp == &noutbuf[NBUFSIZE]) { NFLUSH }
  #define NFLUSH    *(noutp) = 0; \
                    waddchnstr(win, &noutbuf[0], noutp - noutbuf); \
                    noutp = noutbuf;

  /* position the cursor on the output fd */
  void
  poscur(int x, int y, int carefulflag)
  {
    if (carefulflag) {
      if ((!cursor_blink) || (unsigned) x > co || (unsigned) y > li) {
        x = 0; 
        y = 0;
      }
    }
    NFLUSH;
    wmove(win, y, x);
  }

  static us *schar;     /* Ptr to character being updated */
  static us scrrow[256];/* Array to hold screen memory row temporarily */
  static us *temprow;   /* Temporary buffer pointer for scrrow[] */
  static us *bufrow;    /* Ptr to start of buffer row corresp to screen row */
  static int x, y;      /* X and Y position of character being updated */
  static int scanx;     /* Index for comparing screen and buffer rows */
  static int endx;      /* Last character for line loop index */
  static int xdiff;     /* Counter for scan checking used for optimization */
  static int lines;     /* Number of lines to redraw */
  static int numscan;   /* counter for number of lines scanned */
  static int numdone;   /* counter for number of lines actually updated */
  static int a, c;          /* Attrib and character temp variables */
  static int yloop = -1;    /* Row index for loop */
  static int oldx = 0;      /* Previous x cursor position */
  static int oldy = 0;      /* Previous y cursor position */
  static int attrbits = 0;
  static int oldattr = -1;

#if 0
  v_printf("RESTORE SCREEN: prev_screen at %p\n", prev_screen);
  v_printf("SADR: %p\n", screen_adr);
  v_printf("virt_text_base: %p\n", (u_char *)virt_text_base);
  v_printf("SCREEN: %x\n", READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
#endif  

  NFLUSH;
  
  /* The following determins how many lines it should scan at once,
   * since this routine is being called by sig_alrm.  If the entire
   * screen changes, it often incurs considerable delay when this
   * routine updates the entire screen.  So the variable "lines"
   * contains the maximum number of lines to update at once in one
   * call to this routine.  This is set by the "updatelines" keyword
   * in /etc/dosemu.conf 
   */
  lines = config.term_updatelines;
  if (lines < 2) 
    lines = 2;
  else if (lines > li)
    lines = li;

  numscan = 0;         /* Number of lines that have been scanned */
  numdone = 0;         /* Number of lines that needed to be updated */

  /* The highest priority is given to the current screen row for the
   * first iteration of the loop, for maximum typing response.  
   * If y is out of bounds, then give it an invalid value so that it
   * can be given a different value during the loop.
   */
  y = get_bios_cursor_y_position(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
  if ((y < 0) || (y > li)) y = -1;
  
  /* The following loop scans lines on the screen until the maximum number
   * of lines have been updated, or the entire screen has been scanned.
   */
  while ((numdone < lines) && (numscan < li)) {

    /* The following sets the row to be scanned and updated, if it is not
     * the first iteration of the loop, or y has an invalid value from
     * loop pre-initialization.
     */
    if ((numscan > 0) || (y < 0)) {
      yloop = (yloop + 1) % li;
      if (yloop == get_bios_cursor_y_position(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)))
        yloop = (yloop + 1) % li;
      y = yloop;
    }
    numscan++;

    endx = co;
    if (!config.term_corner) {
      /* If it is the last line of the screen, then only process up to 2nd   */
      /* last character, because printing last character of last line can    */
      /* scroll the screen of some terminals.  We don't want this to happen. */
      /* You will see a blank gap in its place. */
      if (y == (li - 1)) endx = co - 1;
    }

    /* Only update if the line has changed.  Note that screen_adr & prev_screen
     * are unsigned short ptrs, so co is not multiplied by 2.
     */
    bufrow = prev_screen + y * co;  /* Position of first char in row in prev_screen */

    /* Copy screen mem line to temporary buffer row */
    memcpy(scrrow, screen_adr + y * co, co * 2);

    /* Strip all blinking bits from temp buffer row if blinking disabled */
    if (!char_blink) {
      temprow = scrrow;
      for (scanx = 0; scanx < co; scanx++, temprow++)
        *temprow &= 0x7FFF;
    }

    /* If the line matches, then no updated is needed, and skip the line. */
    if (!memcmp(bufrow, scrrow, endx * 2)) continue;

    /* Increment the number of successfully updated lines counter */
    numdone++;

    /* The following does the screen line optimally, one character at a 
     * time.  It does a little bit optimization now, thanks to Mark D. Rejhon,
     * to avoid redundant updates.
     */
    schar = scrrow;         /* Pointer to start of row in screen memory */
    xdiff = 0;              /* scan check counter */
    for (x = 0; x < endx; x++) {

      /* The following checks if it needs to scan the line for the first
       * character to be updated.  xdiff is a scan check counter.  If it
       * is nonzero, it means the number of characters until a nonmatching 
       * character between the screen and buffer, and needs to be rescanned
       * right after that character.
       */
      if (xdiff == 0) {

        /* Scan for first character that needs to be updated */
        for (scanx = x; scanx < endx; scanx++)
          if (bufrow[scanx]!=scrrow[scanx])
            break;
      
        /* Do next row if there are no more chars needing to be updated */
        if (scanx >= endx) break;

        /* If the character to be updated is at least 7 cursor positions */
        /* to the right, then do a cursor reposition to save time.  Most */
        /* cursor positioning codes are less than 7 characters. */
        xdiff = scanx - x; 
        if ((xdiff >= 7) || (x == 0)) {
          x += xdiff;
          schar += xdiff;
          xdiff = 0;
          NFLUSH;
          poscur(x,y,0);
        }
      }

      /* The following outputs attributes if necessary, then outputs the */
      /* character onscreen. */
      c = chartrans[ ((unsigned char *) schar)[0] ];
 
      /* Strip blinking bit if blinking characters are disabled */
      if (char_blink)
        a = attrtrans[ ((unsigned char *) schar)[1] ];
      else
        a = attrtrans[ ((unsigned char *) schar)[1] ] & 0x7F;
 
      if (a != oldattr) {
        attrbits = attrlookup[a];
        /* This is a workaround for some of NCURSES bugs messing up screen */
        /* Unfortunately there are still other NCURSES bugs in 1.8.5 :( */
        if (config.term_color)
          if (((oldattr & 0x77) == (a & 0x77)) && (a & 0x70) && (oldattr & 0x8))
            attrbits |= A_BOLD;
        oldattr = a;			/* save old attr as current */
      }
      /* Output character to buffer */
      NOUT(c | attrbits);                    /* Output the actual char */
      schar++;                              /* Increment screen pointer */
      if (xdiff) xdiff--;                   /* Decrement scan check counter */
    }
    memcpy(bufrow, scrrow, co * 2);  /* Copy screen mem line to buffer */
  }

  /* If any part of the screen was updated, then reset the attributes */
  /* and reposition the cursor */
  if (numdone || (cursor_col != oldx) || (cursor_row != oldy)) {
    oldx = cursor_col;
    oldy = cursor_row;
    NFLUSH;
    poscur(oldx, oldy,1);
    wrefresh(win);
  }

  if (numdone) {
     if (numscan==li)
        return 1;     /* changed, entire screen updated */
     else
        return 2;     /* changed, part of screen updated */
  }
  else {
     /* The updates look a bit cleaner when reset to top of the screen
      * if nothing had changed on the screen in this call to screen_restore
      */
     yloop = -1;
     return 0;
  }
}

int
ansi_update()
{
  /* In FAST display mode, speed concerns are very important to efficiently
   * display data.  Displaying characters one at a time is a bad idea, it is
   * much faster to print many characters and terminal codes at once, in the
   * same string.  The following macros addresses these concerns.
   */
  #define CHOUT(c)  if ((outp - outbuf) >= CHBUFSIZE) { CHFLUSH } \
                    *(outp++) = (c);
  #define STROUT(s) if ((outp - outbuf + strlen(s)) >= CHBUFSIZE) { CHFLUSH } \
                    memcpy(outp, s, strlen(s)); \
                    outp += strlen(s);
  #define CHFLUSH   fast_buffer_flush();

  void
  fast_buffer_flush(void) 
  {
    if (outp - outbuf <= 0) return;
    while ( ! fwrite(outbuf, outp - outbuf, 1, stdout) )
      ; /* empty loop */
    outp = outbuf;
  }

  void 
  fast_buffer_poscur(int *oldx, int *oldy, int x, int y, int carefulflag)
  {
    static char cursor[20];
    
    if (carefulflag) {
      if ((!cursor_blink) || (unsigned) x > co || (unsigned) y > li) {
        x = 0; 
        y = 0;
      }
    }

    if ((*oldx == x) && (*oldy == y)) return;
    strcpy(cursor,"\033[");
    strcat(cursor,num_string[y+1]);
    strcat(cursor,";");
    strcat(cursor,num_string[x+1]);
    strcat(cursor,"H");
    STROUT(cursor);
    *oldx = x; 
    *oldy = y;
  }

  void
  fast_buffer_setcolor(int *oldatt, int newatt)
  {
    static obold, oblink, nbold, nblink;
    static char color[20];
    
    if (config.term_color) {
      if (*oldatt == newatt) return;
      obold = *oldatt & 0x08;
      nbold = newatt & 0x08;
      oblink = *oldatt & 0x80;
      nblink = newatt & 0x80;
      if ((obold > nbold) || (oblink > nblink)) {
        strcpy(color,"\033[0;");
        if (nbold)  strcat(color,"1;"); 
        if (nblink) strcat(color,"5;");
        if (newatt & 0x70) {
          strcat(color,bg_color_string[(newatt & 0x70) >> 4]);
          strcat(color,";");
        }
        strcat(color,fg_color_string[newatt & 0x7]);
        strcat(color,"m");
      }
      else {
        strcpy(color,"\033[");
        if (obold  < nbold)  strcat(color,"1;");
        if (oblink < nblink) strcat(color,"5;");
        if ((newatt & 0x70) != (*oldatt & 0x70)) {
          strcat(color,bg_color_string[(newatt & 0x70) >> 4]);
          strcat(color,";");
        }
        if ((newatt & 0x07) != (*oldatt & 0x07)) { 
          strcat(color,fg_color_string[newatt & 0x7]);
          strcat(color,";");
        }
        color[strlen(color)-1] = 'm';
      }    
    }
    else {
      if ((*oldatt & 0xF8) == (newatt & 0xF8)) return;
      strcpy(color,"\033[0;");
      if (newatt & 0x08) strcat(color,"1;"); 
      if (newatt & 0x80) strcat(color,"5;");
      if (newatt & 0x70) strcat(color,"7;");
      color[strlen(color)-1] = 'm';
    }
    STROUT(color);
    *oldatt = newatt;
  }

  static us *schar;	/* Ptr to character being updated */
  static us scrrow[256];/* Array to hold screen memory row temporarily */
  static us *temprow;	/* Temporary buffer pointer for scrrow[] */
  static us *bufrow;	/* Ptr to start of buffer row corresp to screen row */
  static int x, y;	/* X and Y position of character being updated */
  static int scanx;	/* Index for comparing screen and buffer rows */
  static int endx;	/* Last character for line loop index */
  static int xdiff;    	/* Counter for scan checking used for optimization */
  static int lines;    	/* Number of lines to redraw */
  static int numscan;	/* counter for number of lines scanned */
  static int numdone;  	/* counter for number of lines actually updated */
  static int a, c;          /* Attrib and character temp variables */
  static int yloop = -1;    /* Row index for loop */
  static int oldattr = 0;   /* Previous attributes */
  static int oldx = 0;      /* Previous x cursor position */
  static int oldy = 0;      /* Previous y cursor position */

#if 0
  v_printf("RESTORE SCREEN: prev_screen at %p\n", prev_screen);
  v_printf("SADR: %p\n", screen_adr);
  v_printf("virt_text_base: %p\n", (u_char *)virt_text_base);
  v_printf("SCREEN: %x\n", READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
#endif  

  /* The following determines how many lines it should scan at once,
   * since this routine is being called by sig_alrm.  If the entire
   * screen changes, it often incurs considerable delay when this
   * routine updates the entire screen.  So the variable "lines"
   * contains the maximum number of lines to update at once in one
   * call to this routine.  This is set by the "updatelines" keyword
   * in /etc/dosemu.conf 
   */
  lines = config.term_updatelines;
  if (lines < 2) 
    lines = 2;
  else if (lines > li)
    lines = li;

  numscan = 0;         /* Number of lines that have been scanned */
  numdone = 0;         /* Number of lines that needed to be updated */

  /* The highest priority is given to the current screen row for the
   * first iteration of the loop, for maximum typing response.  
   * If y is out of bounds, then give it an invalid value so that it
   * can be given a different value during the loop.
   */
  y = get_bios_cursor_y_position(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
  if ((y < 0) || (y > li)) y = -1; 
  
  /* The following loop scans lines on the screen until the maximum number
   * of lines have been updated, or the entire screen has been scanned.
   */
  while ((numdone < lines) && (numscan < li)) {

    /* The following sets the row to be scanned and updated, if it is not
     * the first iteration of the loop, or y has an invalid value from
     * loop pre-initialization.
     */
    if ((numscan > 0) || (y < 0)) {
      yloop = (yloop + 1) % li;
      if (yloop == get_bios_cursor_y_position(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)))
        yloop = (yloop + 1) % li;
      y = yloop;
    }
    numscan++;

    endx = co;
    if (!config.term_corner) {
      /* If it is the last line of the screen, then only process up to 2nd   */
      /* last character, because printing last character of last line can    */
      /* scroll the screen of some terminals.  We don't want this to happen. */
      /* You will see a blank gap in its place. */
      if (y == (li - 1)) endx = co - 1;
    }

    /* Only update if the line has changed.  Note that screen_adr & prev_screen
     * are unsigned short ptrs, so co is not multiplied by 2.
     */
    bufrow = prev_screen + y * co;  /* Position of first char in row in prev_screen */

    /* Copy screen mem line to temporary buffer row */
    memcpy(scrrow, screen_adr + y * co, co * 2);  
    
    /* Strip all blinking bits from temp buffer row if blinking disabled */
    if (!char_blink) {
      temprow = scrrow;
      for (scanx = 0; scanx < co; scanx++, temprow++)
        *temprow &= 0x7FFF;
    }
    
    /* If the line matches, then no updated is needed, and skip the line. */
    if (!memcmp(bufrow, scrrow, endx * 2)) continue;

    /* Increment the number of successfully updated lines counter */
    numdone++;

    /* The following does the screen line optimally, one character at a 
     * time.  It does a little bit optimization now, thanks to Mark D. Rejhon,
     * to avoid redundant updates.
     */
    schar = scrrow;         /* Pointer to start of row in screen memory */
    xdiff = 0;              /* scan check counter */
    for (x = 0; x < endx; x++) {

      /* The following checks if it needs to scan the line for the first
       * character to be updated.  xdiff is a scan check counter.  If it
       * is nonzero, it means the number of characters until a nonmatching 
       * character between the screen and buffer, and needs to be rescanned
       * right after that character.
       */
      if (xdiff == 0) {

        /* Scan for first character that needs to be updated */
        for (scanx = x; scanx < endx; scanx++)
          if (bufrow[scanx]!=scrrow[scanx])
            break;
      
        /* Do next row if there are no more chars needing to be updated */
        if (scanx >= endx) break;

        /* If the character to be updated is at least 7 cursor positions */
        /* to the right, then do a cursor reposition to save time.  Most */
        /* cursor positioning codes are less than 7 characters. */
        xdiff = scanx - x; 
        if ((xdiff >= 7) || (x == 0)) {
          x += xdiff;
          schar += xdiff;
          xdiff = 0;
          fast_buffer_poscur(&oldx,&oldy,x,y,0);
        }
      }

      /* The following outputs attributes if necessary, then outputs the */
      /* character onscreen. */
      c = chartrans[ ((unsigned char *) schar)[0] ];

      /* Strip blinking bit if blinking characters are disabled */
      if (char_blink)
        a = attrtrans[ ((unsigned char *) schar)[1] ];
      else
        a = attrtrans[ ((unsigned char *) schar)[1] ] & 0x7F;

      if (oldattr != a) 
        fast_buffer_setcolor(&oldattr, a);

      /* Output character to buffer */
      CHOUT(c);                      /* Output the actual char */
      oldx++;
      schar++;                       /* Increment screen pointer */
      if (xdiff) xdiff--;            /* Decrement scan check counter */
    }
    memcpy(bufrow, scrrow, co * 2);  /* Copy screen mem line to buffer */
  }

  fast_buffer_poscur(&oldx,&oldy,cursor_col,cursor_row,1);
  CHFLUSH;

  if (numdone) {
     if (numscan==li)
        return 1;     /* changed, entire screen updated */
     else
        return 2;     /* changed, part of screen updated */
  }
  else {
     /* The updates look a bit cleaner when reset to top of the screen
      * if nothing had changed on the screen in this call to screen_restore
      */
     yloop = -1;
     return 0;
  }
}

#define term_setmode NULL
#define term_update_cursor NULL

struct video_system Video_term = {
   0,                /* is_mapped */
   terminal_initialize, 
   terminal_close,      
   term_setmode,      
   ansi_update,     /* might be changed by terminal_init */
   term_update_cursor
};
