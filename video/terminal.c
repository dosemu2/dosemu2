/* 
 * video/terminal.c - contains the video-functions
 *                    for terminals (TERMCAP/CURSES)
 */

#include <unistd.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

#include "bios.h"
#include "emu.h"
#include "memory.h"
#include "video.h"
#include "terminal.h" 

/* The default character set is to use the latin character set */
unsigned char *trans = charset_latin;

/* In normal keybaord mode (XLATE), some concerns are delt with to
 * efficiently write characters to the STDOUT output. These macros
 * attempt to speed up this screen updating.
 *
 * Thanks to Andrew Haylett (ajh@gec-mrc.co.uk) for catching character loss
 * in CHOUT 
 */
#define OUTBUFSIZE	8192
#define CHOUT(c)   *(outp++) = (c); \
		   if (outp == &outbuf[OUTBUFSIZE-2]) { CHFLUSH }

#define CHFLUSH    if (outp - outbuf) { *(outp) = 0; \
					waddchnstr(win,&outbuf[0],outp-outbuf); \
					outp = outbuf; }

chtype outbuf[OUTBUFSIZE];
chtype *outp = outbuf;
int cursor_row;
int cursor_col;
int colormap[8][8];
WINDOW *win;

/* The following initializes the terminal.  This should be called at the
 * startup of DOSEMU if it's running in terminal mode.
 */ 
void
terminal_initialize()
{
  int i,j,fore,back,pairnum;
  switch (config.term_charset) {
  case CHARSET_LATIN:   trans = charset_latin;   break; 
  case CHARSET_IBM:     trans = charset_ibm;     break;
  case CHARSET_FULLIBM: trans = charset_fullibm; break;
  }
  initscr();
  if (!has_colors()) config.term_color = 0;
  if (config.term_color) start_color();
  win = newwin(0,0,0,0);
  keypad(win,TRUE);
  /*nodelay(win,TRUE);*/
  raw();

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
        if (i || j) {
          init_pair(pairnum,fore,back);
          colormap[j][i] = pairnum;
          pairnum++;
        }
      }
    } 
    colormap[0][0] = colormap[7][0];
  }
  if ((config.term_charset == CHARSET_IBM) || 
      (config.term_charset == CHARSET_FULLIBM)) 
  {
    /* The following turns on the IBM character set mode of virtual console
     * The same code is echoed twice, then just in case the escape code
     * not recognized and was printed, erase it with spaces.
     */
    printf("%s","\033(U\033(U\r        \r");
  }
}

void 
terminal_close()
{
  cursor_col = 0;
  move(li,cursor_col);
  endwin();
  printf("%s","\n");
  if ((config.term_charset == CHARSET_IBM) || 
      (config.term_charset == CHARSET_FULLIBM)) 
  {
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
  if (!config.console_video)
    DOS_SYSCALL(write(fd, ch, len));
  else
    error("ERROR: (video) v_write deferred for console_video\n");
}

/* Put a character on the output fd of this DOSEMU */
int
outcbuf(int c)
{
  CHOUT(c);
  return 1;
}

/* position the cursor on the output fd */
inline void
poscur(int x, int y)
{
  /* were "co" and "li" */
  if ((unsigned) x >= co || (unsigned) y >= li) return;
  CHFLUSH;
  wmove(win, y, x);
}

/* This is a better scroll routine, mostly for aesthetic reasons. It was
 * just too horrible to contemplate a scroll that worked 1 character at a
 * time :-)
 * 
 * It may give some performance improvement on some systems (it does
 * on mine) (Andrew Tridgell)
 */
void
Scroll(int x0, int y0, int x1, int y1, int l, int att)
{
  int dx = x1 - x0 + 1;
  int dy = y1 - y0 + 1;
  int x, y;
  us *sadr, blank = ' ' | (att << 8);
  us *tbuf;

  if (dx <= 0 || dy <= 0 || x0 < 0 || x1 >= co || y0 < 0 || y1 >= li)
    return;

  /* make a blank line */
  tbuf = (us *) malloc(sizeof(us) * dx);
  if (!tbuf) {
    error("failed to malloc temp buf in scroll!");
    return;
  }
  for (x = 0; x < dx; x++)
    tbuf[x] = blank;

  sadr = SCREEN_ADR(bios_current_screen_page);

  if (l >= dy || l <= -dy)
    l = 0;

  if (l == 0) {			/* Wipe mode */
    for (y = y0; y <= y1; y++)
      memcpy(&sadr[y * co + x0], tbuf, dx * sizeof(us));
    free(tbuf);
    return;
  }

  if (l > 0) {
    if (dx == co)
      memcpy(&sadr[y0 * co], &sadr[(y0 + l) * co], (dy - l) * dx * sizeof(us));
    else
      for (y = y0; y <= (y1 - l); y++)
	memcpy(&sadr[y * co + x0], &sadr[(y + l) * co + x0], dx * sizeof(us));

    for (y = y1 - l + 1; y <= y1; y++)
      memcpy(&sadr[y * co + x0], tbuf, dx * sizeof(us));
  }
  else {
    for (y = y1; y >= (y0 - l); y--)
      memcpy(&sadr[y * co + x0], &sadr[(y + l) * co + x0], dx * sizeof(us));

    for (y = y0 - l - 1; y >= y0; y--)
      memcpy(&sadr[y * co + x0], tbuf, dx * sizeof(us));
  }
  free(tbuf);
}

/* Output a character to the screen. */ 
void
char_out(unsigned char ch, int s)
{
  us *sadr;
  int newline_att = 7;
  int xpos, ypos;
  xpos = bios_cursor_x_position(s);
  ypos = bios_cursor_y_position(s);

  switch (ch) {
  case '\r':         /* Carriage return */
    xpos = 0;
    break;

  case '\n':         /* Newline */
    ypos++;
    xpos = 0;                  /* EDLIN needs this behavior */
    sadr = SCREEN_ADR(s);      /* Color newline */
    newline_att = sadr[ypos * co + xpos - 1] >> 8;
    break;

  case 8:           /* Backspace */
    if (xpos > 0) xpos--;
    break;
  
  case '\t':        /* Tab */
    v_printf("tab\n");
    do char_out(' ', s); while (xpos % 8 != 0);
    break;

  case 7:           /* Bell */
    /* Bell should be sounded here, but it's more trouble than its */
    /* worth for now, because printf, addch or addstr or out_char  */
    /* would all interfere by possibly interrupting terminal codes */
    /* Ignore this for now, since this is a hack til NCURSES.      */
    break;

  default:          /* Printable character */
    sadr = SCREEN_ADR(s);
    sadr[ypos * co + xpos] &= 0xff00;
    sadr[ypos * co + xpos++] |= ch;
  }

  if (xpos == co) {
    xpos = 0;
    ypos++;
  }
  if (ypos == li) {
    ypos--;
    scrollup(0, 0, co - 1, li - 1, 1, newline_att);
  }

  bios_cursor_x_position(s) = xpos;
  bios_cursor_y_position(s) = ypos;
  cursor_col = xpos;
  cursor_row = ypos;
}

/* The following clears the screen buffer. It does it only to the screen 
 * buffer.  If in termcap mode, the screen will be cleared the next time
 * restore_screen() is called.
 */
void
clear_screen(int s, int att)
{
  u_short *schar, blank = ' ' | (att << 8);
  int lx;

  v_printf("VID: cleared screen\n");
  if (s > max_page) return;
  
  for (schar = SCREEN_ADR(s), 
       lx = 0; lx < (co * li); 
       *(schar++) = blank, lx++);

  bios_cursor_x_position(s) = bios_cursor_y_position(s) = 0;
  cursor_row = cursor_col = 0;
}

/*
 * This is a much-improved restore_screen, programmed by Mark D. Rejhon.
 * Based on Andrew Tridgell code.  Updated to do more optimization.
 * It is now much faster than it was in pre0.51pl24 and earlier,
 * and actually works more properly. It's also better looking over
 * a 2400 baud modem.  However, there is no buffer overflow checking
 * yet :-(
 */
int
restore_screen()
{
  us *sadr;	/* Pointer to start of screen memory of curr page */
  us *srow;	/* Pointer to start of screen row updated */
  us *schar;	/* Pointer to character being updated */
  int bufrow;	/* Pointer to start of buffer row corresp to screen row */
  int x, y;	/* X and Y position of character being updated */
  int scanx;	/* Index for comparing screen and buffer rows */
  int endx;     /* Last character for line loop index */
  int xdiff;    /* Counter for scan checking used for optimization */
  int lines;    /* Number of lines to redraw */
  int numscan;  /* counter for number of lines scanned */
  int numdone;  /* counter for number of lines actually updated */
  static int newattr = 0;
  static int oldattr;
  static int a, c;
  static int oa = 256;
  static int yloop = -1;    /* Row index for loop */
  static int oldx = 0;      /* Previous x cursor position */
  static int oldy = 0;      /* Previous y cursor position */

  v_printf("RESTORE SCREEN: scrbuf at %p\n", scrbuf);

  CHFLUSH;
  sadr = SCREEN_ADR(bios_current_screen_page);
  v_printf("SADR: %p\n", sadr);
  v_printf("virt_text_base: %p\n", (u_char *)virt_text_base);
  v_printf("SCREEN: %x\n", bios_current_screen_page);
  
  /* The following determins how many lines it should scan at once,
   * since this routine is being called by sig_alrm.  If the entire
   * screen changes, it often incurs considerable delay when this
   * routine updates the entire screen.  So the variable "lines"
   * contains the maximum number of lines to update at once in one
   * call to this routine.  This is set by the "updatelines" keyword
   * in /etc/dosemu.conf 
   */
  lines = config.term_updatelines;
  if (lines <= 0) 
    lines = 1;
  else if (lines > li)
    lines = li;

  numscan = 0;         /* Number of lines that have been scanned */
  numdone = 0;         /* Number of lines that needed to be updated */

  /* The highest priority is given to the current screen row for the
   * first iteration of the loop, for maximum typing response.  
   * If y is out of bounds, then give it an invalid value so that it
   * can be given a different value during the loop.
   */
  y = bios_cursor_y_position(bios_current_screen_page);
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
      if (yloop == bios_cursor_y_position(bios_current_screen_page))
        yloop = (yloop + 1) % li;
      y = yloop;
    }
    numscan++;

    /* Only update if the line has changed.  Note that sadr is an unsigned
     * short ptr, so co is not multiplied by 2...I'll clean this up later.
     */
    bufrow = y * co * 2;	/* Position of first char in row in sadr */
    srow = sadr + y * co;	/* p is unsigned short ptr to char in sadr */
   
    /* If it is the last line of the screen, then only process up to 2nd   */
    /* last character, because printing last character of last line can    */
    /* scroll the screen of some terminals.  We don't want this to happen. */
    /* You will see a blank gap in its place.  This should not be a big    */
    /* problem for most users.   */
    if (y == (li - 1))
      endx = co - 1;
    else
      endx = co;
    
    /* If the line matches, then no updated is needed, and skip the line. */
    if (!memcmp(scrbuf + bufrow, srow, endx * 2)) continue;

    /* Increment the number of successfully updated lines counter */
    if (numscan > 1) numdone++;

    /* The following does the screen line optimally, one character at a 
     * time.  It does a little bit optimization now, thanks to Mark D. Rejhon,
     * to avoid redundant updates.
     */
    schar = srow;         /* Pointer to start of row in screen memory */
    xdiff = 0;            /* scan check counter */
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
          if (memcmp(scrbuf + bufrow + scanx * 2, sadr + y * co + scanx, 2))
            break;
      
        /* Do Next row if there are no more chars needing to be updated */
        if (scanx >= endx) break;

        /* If the character to be updated is at least 7 cursor positions */
        /* to the right, then do a cursor reposition to save time.  Most */
        /* cursor positioning codes are less than 7 characters. */
        xdiff = scanx - x; 
        if ((xdiff >= 7) || (x == 0)) {
          x += xdiff;
          schar += xdiff;
          xdiff = 0;
          CHFLUSH;
          poscur(x,y);
        }
      }

      /* The following outputs attributes if necessary, then outputs the */
      /* character onscreen. */
      c = *(unsigned char *) schar;
      a = ((unsigned char *) schar)[1];
      if (1) {     /*(a != oa) {*/
        if (config.term_color) {
          newattr = COLOR_PAIR(colormap[a & 7][(a >> 4) & 7]);
          /* The following lines gets around a bug with NCURSES */
          if (((oa & 0x77) == (a & 0x77)) && (a & 0x70))
            if ((oldattr & A_BOLD) && (!(a & 0x8))) 
              newattr |= A_BOLD;
          if (a & 0x80) newattr |= A_BLINK;
          if (a & 0x08) newattr |= A_BOLD;
	}
	else {
	  newattr = 0;
	  if (!(a & 0xF8))
	    newattr |= A_NORMAL;
	  else {
            if (a & 0x70) newattr |= A_REVERSE;
            if (a & 0x08) newattr |= A_BOLD;
            if (a & 0x80) newattr |= A_BLINK;
          }
        }
        oa = a;			/* save old attr as current */
        oldattr = newattr;
      }
      /* Output character to buffer */
      CHOUT(trans[c] | newattr);              /* Output the actual char */
      schar++;                              /* Increment screen pointer */
      if (xdiff) xdiff--;                   /* Decrement scan check counter */
    }
    memcpy(scrbuf + bufrow, srow, co * 2);  /* Copy screen mem line to buffer */
  }

  /* If any part of the screen was updated, then reset the attributes */
  /* and reposition the cursor */
  if (numdone || (cursor_col != oldx) || (cursor_row != oldy)) {
    oldx = cursor_col;
    oldy = cursor_row;
    poscur(oldx, oldy);
    wrefresh(win);
  }
  /* The updates look a bit cleaner when reset to top of the screen
   * if nothing had changed on the screen in this call to screen_restore
   */
  if (!numdone) yloop = -1;
  return numdone;
}
