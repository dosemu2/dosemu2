/* 
 * video/terminal.c - contains the video-functions
 *                    for terminals (TERMCAP/CURSES)
 */

#include <unistd.h>
#include <termcap.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_NCURSES
#include <ncurses.h>
#endif

#include "bios.h"
#include "emu.h"
#include "memory.h"
#include "video.h"
#include "../disks.h"

#if 1 
unsigned char trans[] =		/* NEW IMPROVED LATIN CHAR SET */
{
  " \326@hdcs.#o0/+;M*"
  "><|H\266\247+|^v><--^v"
  " !\"#$%&'()*+,-./"
  "0123456789:;<=>?"
  "@ABCDEFGHIJKLMNO"
  "PQRSTUVWXYZ[\\]^_"
  "`abcdefghijklmno"
  "pqrstuvwxyz{|}~^"
  "\307\374\351\342\344\340\345\347\352\353\350\357\356\354\304\305"
  "\311\346\306\364\366\362\373\371\377\326\334\242\243\245\120\146"
  "\341\355\363\372\361\321\252\272\277\055\254\275\274\241\253\273"
  ":%&|{{{..{I.'''."
  "``+}-+}}`.**}=**"
  "+*+``..**'.#_][~"
  "a\337\254\266{\363\265t\330\364\326\363o\370En"
  "=\261><()\367=\260\267\267%\140\262= "
};
#else
unsigned char trans[] =		/* LATIN CHAR SET */
{
  "\0\0\0\0\0\0\0\0\00\00\00\00\00\00\00\00"
  "\0\0\0\0\266\247\0\0\0\0\0\0\0\0\0\0"
  " !\"#$%&'()*+,-./"
  "0123456789:;<=>?"
  "@ABCDEFGHIJKLMNO"
  "PQRSTUVWXYZ[\\]^_"
  "`abcdefghijklmno"
  "pqrstuvwxyz{|}~ "
  "\307\374\351\342\344\340\345\347\352\353\350\357\356\354\304\305"
  "\311\346\306\364\366\362\373\371\377\326\334\242\243\245\0\0"
  "\341\355\363\372\361\321\252\272\277\0\254\275\274\241\253\273"
  "\0\0\0|++++++|+++++"
  "++++-++++++++-++"
  "+++++++++++\0\0\0\0\0"
  "\0\337\0\0\0\0\265\0\0\0\0\0\0\0\0\0"
  "\0\261\0\0\0\0\367\0\260\267\0\0\0\262\244\0"
};
#endif

/*
   In normal keybaord mode (XLATE), some concerns are delt with to
   efficiently write characters to the STDOUT output. These macros
   attempt to speed up this screen updating
*/
/* thanks to Andrew Haylett (ajh@gec-mrc.co.uk) for catching character loss
 * in CHOUT */
#define OUTBUFSIZE	80
#define CHOUT(c)   *(outp++) = (c);  \
		   if (outp == &outbuf[OUTBUFSIZE-2]) { CHFLUSH } 

#define CHFLUSH    if (outp - outbuf) { v_write(1, outbuf, outp - outbuf); \
						outp = outbuf; }
unsigned char outbuf[OUTBUFSIZE], *outp = outbuf;

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

/* Put a string on the output fd */
void
dostputs(char *a, int b, int (*c) (int) /* outfuntype c */ )
{
  /* discard c right now */
  /* was "CHFLUSH; tputs(a,b,outcbuf);" */
  tputs(a, b, c);
}

/* position the cursor on the output fd */
inline void
poscur(int x, int y)
{
  /* were "co" and "li" */
  if ((unsigned) x >= CO || (unsigned) y >= LI) return;
  tputs(tgoto(cm, x, y), 1, outch);
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

  if (dx <= 0 || dy <= 0 || x0 < 0 || x1 >= CO || y0 < 0 || y1 >= LI)
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
      memcpy(&sadr[y * CO + x0], tbuf, dx * sizeof(us));
    free(tbuf);
    return;
  }

  if (l > 0) {
    if (dx == CO)
      memcpy(&sadr[y0 * CO], &sadr[(y0 + l) * CO], (dy - l) * dx * sizeof(us));
    else
      for (y = y0; y <= (y1 - l); y++)
	memcpy(&sadr[y * CO + x0], &sadr[(y + l) * CO + x0], dx * sizeof(us));

    for (y = y1 - l + 1; y <= y1; y++)
      memcpy(&sadr[y * CO + x0], tbuf, dx * sizeof(us));
  }
  else {
    for (y = y1; y >= (y0 - l); y--)
      memcpy(&sadr[y * CO + x0], &sadr[(y + l) * CO + x0], dx * sizeof(us));

    for (y = y0 - l - 1; y >= y0; y--)
      memcpy(&sadr[y * CO + x0], tbuf, dx * sizeof(us));
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

  if (ch == '\r') {         /* Carriage return */
    xpos = 0;
  }
  else if (ch == '\n') {    /* Newline */
    ypos++;
    xpos = 0;                  /* EDLIN needs this behavior */
    sadr = SCREEN_ADR(s);      /* Color newline */
    newline_att = sadr[ypos * CO + xpos - 1] >> 8;
  }
  else if (ch == '\010') {
    if (xpos > 0) xpos--;
  }
  else if (ch == '\t') {    /* Tab */
    v_printf("tab\n");
    do char_out(' ', s); while (xpos % 8 != 0);
  }
  else if (ch == 7) {       /* Bell */
    /* Bell should be sounded here, but it's more trouble than its */
    /* worth for now, because printf, addch or addstr or out_char  */
    /* would all interfere by possibly interrupting terminal codes */
    /* Ignore this for now, since this is a hack til NCURSES.      */
  }
  else {                    /* Printable character */
    sadr = SCREEN_ADR(s);
    sadr[ypos * CO + xpos] &= 0xff00;
    sadr[ypos * CO + xpos++] |= ch;
  }

  if (xpos == CO) {
    xpos = 0;
    ypos++;
  }
  if (ypos == LI) {
    ypos--;
    scrollup(0, 0, CO - 1, LI - 1, 1, newline_att);
  }

  bios_cursor_x_position(s) = xpos;
  bios_cursor_y_position(s) = ypos;
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
       lx = 0; lx < (CO * LI); 
       *(schar++) = blank, lx++);

  bios_cursor_x_position(s) = bios_cursor_y_position(s) = 0;
  if (config.console_video) {
    cli();
    poscur(0,0); 
    sti();
  }
}

/*
 * This version of restore_screen works in chunks across the screen,
 * instead of only with whole lines. This can be MUCH faster in some
 * cases, like when running across a 2400 baud modem :-)
 * Andrew Tridgell
 */
void
restore_screen(void)
{
  us *sadr;	/* Pointer to start of screen memory of curr page */
  us *srow;	/* Pointer to start of screen row updated */
  us *schar;	/* Pointer to character being updated */
  int bufrow;	/* Pointer to start of buffer row corresp to screen row */
  u_char c, a;	/* Temporary character and attributes holding vars */
  int oa;	/* Old attribute */
  int x, y;	/* X and Y position of character being updated */
  int scanx;	/* Index for comparing screen and buffer rows */
  int xdiff;    /* Counter for scan checking used for optimization */
  int lines;    /* Number of lines to redraw */
  int numdone;  /* counter for number of lines actually updated */
  int numscan;  /* counter for number of lines scanned */
  static ucounter = 0; /* Infrequent update counter */
  static yloop = -1;   /* Row index for loop */
  static oldx = 0;     /* Previous x cursor position */
  static oldy = 0;     /* Previous y cursor position */

  v_printf("RESTORE SCREEN: scrbuf at %p\n", scrbuf);

  CHFLUSH;
  sadr = SCREEN_ADR(bios_current_screen_page);
  v_printf("SADR: %p\n", sadr);
  v_printf("virt_text_base: %p\n", (u_char *)virt_text_base);
  v_printf("SCREEN: %x\n", bios_current_screen_page);
  oa = 7;
  
  /* The following determins how many lines it should scan at once,
   * since this routine is being called by sig_alrm.  If the entire
   * screen changes, it often incurs considerable delay when this
   * routine updates the entire screen.  So the variable "lines"
   * contains the maximum number of lines to update at once in one
   * call to this routine.  If chunks in config is 1, then the full
   * screen is updated.  If chunks is 2, then half the screen is updated.
   * The amount of screen update per call is 1 divided by chunks.
   * If chunks is greater than 25, then the updates become less frequent,
   * with this function simply being exited in some calls.
   */
  if (config.redraw_chunks == 0)
    lines = LI;
  else if (config.redraw_chunks <= LI) {
    lines = LI / config.redraw_chunks;
    if (lines < 1) lines = 1;
  }
  else {
    ucounter = (ucounter + 1) % ((config.redraw_chunks / LI) + 1);
    if (ucounter) return; 
    lines = 1;
  }
  
  numdone = 0;         /* Number of lines that needed to be updated */
  numscan = 0;         /* Number of lines that have been scanned */

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
  while ((numdone < lines) && (numscan < LI)) {

    /* The following sets the row to be scanned and updated, if it is not
     * the first iteration of the loop, or y has an invalid value from
     * loop pre-initialization.
     */
    if ((numscan > 0) || (y < 0)) {
      yloop = (yloop + 1) % LI;
      if (yloop == bios_cursor_y_position(bios_current_screen_page))
        yloop = (yloop + 1) % LI;
      y = yloop;
    }
    numscan++;

    /* Only update if the line has changed.  Note that sadr is an unsigned
     * short ptr, so CO is not multiplied by 2...I'll clean this up later.
     */
    bufrow = y * CO * 2;	/* Position of first char in row in sadr */
    srow = sadr + y * CO;	/* p is unsigned short ptr to char in sadr */
    
    /* If the line matches, then no updated is needed, and skip the line. */
    if (!memcmp(scrbuf + bufrow, srow, CO * 2)) continue;

    /* Increment the number of successfully updated lines counter */
    if (numscan > 1) numdone++;

    /* The following does the screen line optimally, one character at a 
     * time.  It does a little bit optimization now, thanks to Mark D. Rejhon,
     * to avoid redundant updates.
     */
    schar = srow;         /* Pointer to start of row in screen memory */
    xdiff = 0;            /* scan check counter */
    for (x = 0; x < CO; x++) {

      /* The following checks if it needs to scan the line for the first
       * character to be updated.  xdiff is a scan check counter.  If it
       * is nonzero, it means the number of characters until a nonmatching 
       * character between the screen and buffer, and needs to be rescanned
       * right after that character.
       */
      if (xdiff == 0) {

        /* Scan for first character that needs to be updated */
        for (scanx = x; scanx < CO; scanx++)
          if (memcmp(scrbuf + bufrow + scanx * 2, sadr + y * CO + scanx, 2))
            break;
      
        /* Do Next row if there are no more chars needing to be updated */
        if (scanx >= CO) break;

        /* If the character to be updated is at least 7 cursor positions */
        /* to the right, then do a cursor reposition to save time.  Most */
        /* cursor positioning codes are less than 7 characters. */
        xdiff = scanx - x; 
        if ((xdiff >= 7) || (x == 0)) {
          x += xdiff;
          schar += xdiff;
          xdiff = 0;
          dostputs(tgoto(cm, x, y), 1, outcbuf);
        }
      }

      /* The following outputs attributes if necessary, then outputs the */
      /* character onscreen. */
      c = *(unsigned char *) schar;
      if ((a = ((unsigned char *) schar)[1]) != oa) {
        /* do fore/back-ground colors */
        if (!(a & 7) || (a & 0x70))
          dostputs(mr, 1, outcbuf);
        else
          dostputs(me, 1, outcbuf);

        /* do high intensity */
        if (a & 0x8)
          dostputs(md, 1, outcbuf);
        else if (oa & 0x8) {
          dostputs(me, 1, outcbuf);
        if (!(a & 7) || (a & 0x70))
          dostputs(mr, 1, outcbuf);
        }

        /* do underline/blink */
        if (a & 0x80)
          dostputs(so, 1, outcbuf);
        else if (oa & 0x80)
          dostputs(se, 1, outcbuf);

        oa = a;		/* save old attr as current */
      }
      /* Output the actual character */
      CHOUT(trans[c] ? trans[c] : '_');     /* Output the actual char */
      schar++;                              /* Increment screen pointer */
      if (xdiff) xdiff--;                   /* Decrement scan check counter */
    }
    memcpy(scrbuf + bufrow, srow, CO * 2);  /* Copy screen mem line to buffer */
  }

  /* If any part of the screen was updated, then reset the attributes */
  /* and reposition the cursor */
  if (numdone || 
     (bios_cursor_x_position(bios_current_screen_page) != oldx) ||
     (bios_cursor_y_position(bios_current_screen_page) != oldy)) 
  {
    oldx = bios_cursor_x_position(bios_current_screen_page);
    oldy = bios_cursor_y_position(bios_current_screen_page);
    dostputs(me, 1, outcbuf);
    dostputs(tgoto(cm, oldx, oldy), 1, outcbuf);
    CHFLUSH;
  }
  /* The updates look a bit cleaner when reset to top of the screen
   * if nothing had changed on the screen in this call to screen_restore
   */
  if (!numdone) yloop = -1;
}
