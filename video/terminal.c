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

#ifndef USE_NCURSES
/* Put a string on the output fd */
void
dostputs(char *a, int b, int (*c) (int) /* outfuntype c */ )
{
  /* discard c right now */
  /* CHFLUSH; */
  /* was "CHFLUSH; tputs(a,b,outcbuf);" */
  tputs(a, b, c);
}

#endif

/* position the cursor on the output fd */
inline void
poscur(int x, int y)
{
  /* were "co" and "li" */
  if ((unsigned) x >= CO || (unsigned) y >= LI)
    return;
#ifdef USE_NCURSES
  move(y, x);
  refresh();			/* may not be necessary */
#else
  tputs(tgoto(cm, x, y), 1, outch);
#endif
}

#ifndef OLD_SCROLL
/*
This is a better scroll routine, mostly for aesthetic reasons. It was
just too horrible to contemplate a scroll that worked 1 character at a
time :-)

It may give some performance improvement on some systems (it does
on mine)
(Andrew Tridgell)
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

  update_screen = 0;

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
      memcpy(&sadr[y0 * CO],
	     &sadr[(y0 + l) * CO],
	     (dy - l) * dx * sizeof(us));
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

  update_screen = 0;
  memcpy(scrbuf, sadr, CO * LI * 2);
  free(tbuf);
}

#else

void
scrollup(int x0, int y0, int x1, int y1, int l, int att)
{
  int dx, dy, x, y, ofs;
  us *sadr, *p, *q, blank = ' ' | (att << 8);

  if (l == 0) {			/* Wipe mode */
    sadr = SCREEN_ADR(bios_current_screen_page);
    for (dy = y0; dy <= y1; dy++)
      for (dx = x0; dx <= x1; dx++)
	sadr[dy * CO + dx] = blank;
    return;
  }

  sadr = SCREEN_ADR(bios_current_screen_page);
  sadr += x0 + CO * (y0 + l);

  dx = x1 - x0 + 1;
  dy = y1 - y0 - l + 1;
  ofs = -CO * l;
  for (y = 0; y < dy; y++) {
    p = sadr;
    if (l != 0)
      for (x = 0; x < dx; x++, p++)
	p[ofs] = p[0];
    else
      for (x = 0; x < dx; x++, p++)
	p[0] = blank;
    sadr += CO;
  }
  for (y = 0; y < l; y++) {
    sadr -= CO;
    p = sadr;
    for (x = 0; x < dx; x++, p++)
      *p = blank;
  }

  update_screen = 1;
}

void
scrolldn(int x0, int y0, int x1, int y1, int l, int att)
{
  int dx, dy, x, y, ofs;
  us *sadr, *p, blank = ' ' | (att << 8);

  if (l == 0) {
    l = LI - 1;			/* Clear whole if l=0 */
  }

  sadr = SCREEN_ADR(bios_current_screen_page);
  sadr += x0 + CO * (y1 - l);

  dx = x1 - x0 + 1;
  dy = y1 - y0 - l + 1;
  ofs = CO * l;
  for (y = 0; y < dy; y++) {
    p = sadr;
    if (l != 0)
      for (x = 0; x < dx; x++, p++)
	p[ofs] = p[0];
    else
      for (x = 0; x < dx; x++, p++)
	p[0] = blank;
    sadr -= CO;
  }
  for (y = 0; y < l; y++) {
    sadr += CO;
    p = sadr;
    for (x = 0; x < dx; x++, p++)
      *p = blank;
  }

  update_screen = 1;
}

#endif /* OLD_SCROLL */
#ifdef USE_NCURSES
void 
char_out(unsigned char ch, int s, int advflag)
{
  addch(ch);
  refresh();
}

#else
void
char_out_att(unsigned char ch, unsigned char att, int s, int advflag)
{
  u_short *sadr;
  int xpos = bios_cursor_x_position(s), ypos = bios_cursor_y_position(s);

  /*	if (s > max_page) return; */

  if (config.console_video) {
    if (ch >= ' ') {
      sadr = SCREEN_ADR(s);
      sadr[bios_cursor_y_position(s) * CO + bios_cursor_x_position(s)++] = ch | (att << 8);
    }
    else if (ch == '\r') {
      bios_cursor_x_position(s) = 0;
    }
    else if (ch == '\n') {
      bios_cursor_y_position(s)++;
      bios_cursor_x_position(s) = 0;		/* EDLIN needs this behavior */
    }
    else if (ch == '\010' && bios_cursor_x_position(s) > 0) {
      bios_cursor_x_position(s)--;
    }
    else if (ch == '\t') {
      v_printf("tab\n");
      do {
	char_out(' ', s, advflag);
      } while (bios_cursor_x_position(s) % 8 != 0);
    }

    if (!advflag) {
      bios_cursor_x_position(s) = xpos;
      bios_cursor_y_position(s) = ypos;
    }
    else
      poscur(bios_cursor_x_position(s), bios_cursor_y_position(s));
  }

  else {
    unsigned short *wscrbuf = (unsigned short *) scrbuf;

    /* update_screen not set to 1 because we do the outputting
	   * ourselves...scrollup() and scrolldn() should also work
	   * this way, when I get around to it.
	   */
    update_screen = 0;

    /* this will need some fixing to work with advflag, so that extra
	   * characters won't cause wraparound...
	   */

    if (ch >= ' ') {
      sadr = SCREEN_ADR(s);
      sadr[bios_cursor_y_position(s) * CO 
	   + bios_cursor_x_position(s)] = ch | (att << 8);
      wscrbuf[bios_cursor_y_position(s) * CO 
	      + bios_cursor_x_position(s)] = ch | (att << 8);
      bios_cursor_x_position(s)++;
      if (s == bios_current_screen_page)
	outch(trans[ch]);
    }
    else if (ch == '\r') {
      bios_cursor_x_position(s) = 0;
      if (s == bios_current_screen_page)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\n') {
      bios_cursor_y_position(s)++;
      bios_cursor_x_position(s) = 0;		/* EDLIN needs this behavior */
      if (s == bios_current_screen_page)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\010' && bios_cursor_x_position(s) > 0) {
      bios_cursor_x_position(s)--;
      if ((s == bios_current_screen_page) && advflag)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\t') {
      do {
	char_out(' ', s, advflag);
      } while (bios_cursor_x_position(s) % 8 != 0);
    }
    else if (ch == '\010' && bios_cursor_x_position(s) > 0) {
      bios_cursor_x_position(s)--;
      if ((s == bios_current_screen_page) && advflag)
	write(STDOUT_FILENO, &ch, 1);
    }
  }

  if (advflag) {
    if (bios_cursor_x_position(s) == CO) {
      bios_cursor_x_position(s) = 0;
      bios_cursor_y_position(s)++;
    }
    if (bios_cursor_y_position(s) == LI) {
      bios_cursor_y_position(s)--;
      scrollup(0, 0, CO - 1, LI - 1, 1, 7);
      update_screen = 0 /* 1 */ ;
    }
  }
}

/* temporary hack... we'll merge this back into char_out_att later.
 * right now, I need to play with it.
 */
void
char_out(unsigned char ch, int s, int advflag)
{
  us *sadr;
  int xpos = bios_cursor_x_position(s);
  int ypos = bios_cursor_y_position(s);
  int newline_att = 7;

  if (config.console_video) {
    if (ch >= ' ') {
      sadr = SCREEN_ADR(s);
      sadr[bios_cursor_y_position(s) * CO 
	   + bios_cursor_x_position(s)] &= 0xff00;
      sadr[bios_cursor_y_position(s) * CO 
	   + bios_cursor_x_position(s)++] |= ch;
    }
    else if (ch == '\r') {
      bios_cursor_x_position(s) = 0;
    }
    else if (ch == '\n') {
      bios_cursor_y_position(s)++;
      bios_cursor_x_position(s) = 0;		/* EDLIN needs this behavior */

      /* color new line */
      sadr = SCREEN_ADR(s);
      newline_att = sadr[bios_cursor_y_position(s) * CO 
			 + bios_cursor_x_position(s) - 1] >> 8;

    }
    else if (ch == '\010' && bios_cursor_x_position(s) > 0) {
      bios_cursor_x_position(s)--;
    }
    else if (ch == '\t') {
      v_printf("tab\n");
      do {
	char_out(' ', s, advflag);
      } while (bios_cursor_x_position(s) % 8 != 0);
    }

    if (!advflag) {
      bios_cursor_x_position(s) = xpos;
      bios_cursor_y_position(s) = ypos;
    }
    else
      poscur(bios_cursor_x_position(s), bios_cursor_y_position(s));
  }

  else {
    unsigned short *wscrbuf = (unsigned short *) scrbuf;

    /* this will need some fixing to work with advflag, so that extra
	   * characters won't cause wraparound...
	   */

    update_screen = 0;

    if (ch >= ' ') {
      sadr = SCREEN_ADR(s);
      sadr[bios_cursor_y_position(s) * CO 
	   + bios_cursor_x_position(s)] &= 0xff00;
      sadr[bios_cursor_y_position(s) * CO 
	   + bios_cursor_x_position(s)] |= ch;
      wscrbuf[bios_cursor_y_position(s) * CO 
	      + bios_cursor_x_position(s)] &= 0xff00;
      wscrbuf[bios_cursor_y_position(s) * CO 
	      + bios_cursor_x_position(s)] |= ch;
      bios_cursor_x_position(s)++;
      if (s == bios_current_screen_page)
	outch(trans[ch]);
    }
    else if (ch == '\r') {
      bios_cursor_x_position(s) = 0;
      if (s == bios_current_screen_page)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\n') {
      bios_cursor_y_position(s)++;
      bios_cursor_x_position(s) = 0;		/* EDLIN needs this behavior */
      if (s == bios_current_screen_page)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\010' && bios_cursor_x_position(s) > 0) {
      bios_cursor_x_position(s)--;
      if ((s == bios_current_screen_page) && advflag)
	write(STDOUT_FILENO, &ch, 1);
    }
    else if (ch == '\t') {
      do {
	char_out(' ', s, advflag);
      } while (bios_cursor_x_position(s) % 8 != 0);
    }
    else if (ch == '\010' && bios_cursor_x_position(s) > 0) {
      bios_cursor_x_position(s)--;
      if ((s == bios_current_screen_page) && advflag)
	write(STDOUT_FILENO, &ch, 1);
    }
  }

  if (advflag) {
    if (bios_cursor_x_position(s) == CO) {
      bios_cursor_x_position(s) = 0;
      bios_cursor_y_position(s)++;
    }
    if (bios_cursor_y_position(s) == LI) {
      bios_cursor_y_position(s)--;
      scrollup(0, 0, CO - 1, LI - 1, 1, newline_att);
      update_screen = 0 /* 1 */ ;
    }
  }
}

#endif /* USE_NCURSES */

void
clear_screen(int s, int att)
{
  us *sadr, *p, blank = ' ' | (att << 8);
  int lx;

  update_screen = 0;
  v_printf("VID: cleared screen\n");
  if (s > max_page)
    return;
  sadr = SCREEN_ADR(s);
  cli();

  for (p = sadr, lx = 0; lx < (CO * LI); *(p++) = blank, lx++) ;
  if (!config.console_video) {
    memcpy(scrbuf, sadr, CO * LI * 2);
    if (s == bios_current_screen_page) {
#ifdef USE_NCURSES
      attrset(A_NORMAL);		/* should set attribute according to 'att' */
      clear();
      move(0, 0);
      refresh();
#else
      tputs(cl, 1, outch);
#endif
    }
  }

  bios_cursor_x_position(s) = bios_cursor_y_position(s) = 0;
  poscur(0, 0);
  sti();
  update_screen = 0;
}

#ifdef USE_NCURSES
void 
restore_screen()
{
  refresh();
}

#else

/* there's a bug in here somewhere */
#define CHUNKY_RESTORE 1
#ifdef CHUNKY_RESTORE
/*
This version of restore_screen works in chunks across the screen,
instead of only with whole lines. This can be MUCH faster in some
cases, like when running across a 2400 baud modem :-)
Andrew Tridgell
*/
void
restore_screen(void)
{
  us *sadr, *p;
  unsigned char c, a;
  int x, y, oa;
  int Xchunk = CO / config.redraw_chunks;
  int Xstart;

  v_printf("RESTORE SCREEN: scrbuf at %p\n", scrbuf);

  if (config.console_video) {
    v_printf("restore cancelled for console_video\n");
    return;
  }

  update_screen = 0;
  CHFLUSH;

  sadr = SCREEN_ADR(bios_current_screen_page);
  v_printf("SADR: %p\n", sadr);
  v_printf("virt_text_base: %p\n", (u_char *)virt_text_base);
  v_printf("SCREEN: %x\n", bios_current_screen_page);
  oa = 7;
  p = sadr;
  for (y = 0; y < LI; y++)
    for (Xstart = 0; Xstart < CO; Xstart += Xchunk) {
      int chunk = ((Xstart + Xchunk) > CO) ? (CO - Xstart) : Xchunk;

      /* only update if this chunk of line has changed
	 * ..note that sadr is an unsigned
	 * short ptr, so CO is not multiplied by 2...I'll clean this up
	 * later.
	 */
      if (!memcmp(scrbuf + y * CO * 2 + Xstart * 2, sadr + y * CO + Xstart,
		  chunk * 2)) {
	p += chunk;		/* p is an unsigned short pointer */
	continue;		/* go to the next chunk */
      }
      else
	memcpy(scrbuf + y * CO * 2 + Xstart * 2, p, chunk * 2);

      /* This chunk must have changed - we'll have to redraw it */

      /* go to the start of the chunk */
      dostputs(tgoto(cm, Xstart, y), 1, outcbuf);

      /* do chars one at a time */
      for (x = Xstart; x < Xstart + chunk; x++) {
	c = *(unsigned char *) p;
	if ((a = ((unsigned char *) p)[1]) != oa) {
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
	CHOUT(trans[c] ? trans[c] : '_');
	p++;
      }
    }

  dostputs(me, 1, outcbuf);
  CHFLUSH;
  poscur(bios_cursor_x_position(bios_current_screen_page), 
	 bios_cursor_y_position(bios_current_screen_page));
}

#else

void
restore_screen(void)
{
  us *sadr, *p;
  unsigned char c, a;
  int x, y, oa;

  update_screen = 0;

  v_printf("RESTORE SCREEN: scrbuf at 0x%08x\n", scrbuf);

  if (config.console_video) {
    v_printf("restore cancelled for console_video\n");
    return;
  }

  sadr = SCREEN_ADR(bios_current_screen_page);
  oa = 7;
  p = sadr;
  for (y = 0; y < LI; y++) {
    dostputs(tgoto(cm, 0, y), 1, outcbuf);
    for (x = 0; x < CO; x++) {
      c = *(unsigned char *) p;
      if ((a = ((unsigned char *) p)[1]) != oa) {
#ifndef OLD_RESTORE
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

	oa = a;			/* save old attr as current */
#else
	if ((a & 7) == 0)
	  dostputs(mr, 1, outcbuf);
	else
	  dostputs(me, 1, outcbuf);
	if ((a & 0x88))
	  dostputs(md, 1, outcbuf);
	oa = a;
#endif
      }
      CHOUT(trans[c] ? trans[c] : '_');
      p++;
    }
  }

  dostputs(me, 1, outcbuf);
  CHFLUSH;
  poscur(bios_cursor_x_position(bios_current_screen_page), 
	 bios_cursor_y_position(bios_current_screen_page));
}

#endif /* CHUNKY_RESTORE */
#endif /* USE_NCURSES */
