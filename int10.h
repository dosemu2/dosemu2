/*
 * Handler for int10-requests, moved from video.c and made it inline
 */

extern inline void poscur(int, int);
extern void Scroll(int, int, int, int, int, int);
extern void hide_cursor(void);

__inline__ void int10(void)
{
  /* some code here is copied from Alan Cox ***************/
  int x, y, i, tmp;
  unsigned int screen;
  static int gfx_flag = 0;
  u_char c;
  us *sm;

  if (d.video >= 3)
    {
      if (d.video >= 4)
	dbug_printf("int10 near %04x:%08lx\n", LWORD(cs), REG(eip));
      if ( (LO(ax) >= ' ') && (LO(ax) < 0x7f) )
	dbug_printf("int10 AH=%02x AL=%02x '%c'\n",
		    HI(ax), LO(ax), LO(ax));
      else
	dbug_printf("int10 AH=%02x AL=%02x\n", HI(ax), LO(ax));
    }

  NOCARRY;
  switch (HI(ax)) {
  case 0x0:			/* define mode */
    v_printf("define mode: 0x%x\n", LO(ax));
    switch (LO(ax)) {
    case 2:
    case 3:
      v_printf("text mode requested from %d to %d, ", screen_mode, LO(ax));
      max_page = 7;
      if ( (configuration & MDA_CONF_SCREEN_MODE) == MDA_CONF_SCREEN_MODE )
	screen_mode = 7;
      else
	screen_mode = LO(ax);
      v_printf("set to %d\n", screen_mode);
      gfx_flag = 0;		/* we're in a text mode now */
      gfx_mode = TEXT;
      /* mode change clears screen unless bit7 of AL set */
      if (!(LO(ax) & 0x80))
	clear_screen(bios_current_screen_page, 7);
      NOCARRY;
      break;
    default:
      v_printf("undefined video mode 0x%x\n", LO(ax));
      CARRY;
      return;
    }

    /* put the mode in the BIOS Data Area */
    bios_video_mode = screen_mode;
    break;

  case 0x1:			/* define cursor shape */
    v_printf("define cursor: 0x%04x\n", LWORD(ecx));
    /* 0x20 is the cursor no blink/off bit */
    /*	if (HI(cx) & 0x20) */
    if (REG(ecx) == 0x2000) {
      hide_cursor();
    }
    else
      show_cursor();
    CARRY;
    break;

  case 0x2:			/* set cursor pos */
    screen = HI(bx);
    x = LO(dx);
    y = HI(dx);
    if (screen > 7) {
      error("ERROR: video error (setcur/screen>7: %d)\n", screen);
      CARRY;
      return;
    }
    if (x >= CO || y >= LI)
      break;

    bios_cursor_x_position(screen) = x;
    bios_cursor_y_position(screen) = y;

    if (screen == bios_current_screen_page)
      poscur(x, y);
    break;

  case 0x3:			/* get cursor pos */
    screen = HI(bx);
    if (screen > 7) {
      error("ERROR: video error(0x3 screen>7: %d)\n", screen);
      CARRY;
      return;
    }
    REG(edx) = (bios_cursor_y_position(screen) << 8) 
              | bios_cursor_x_position(screen);
    break;

  case 0x5:
    {				/* change page */
      screen = LO(ax);
      error("VID: change page from %d to %d!\n", bios_current_screen_page, screen);
      if (screen <= max_page) {
	if (!config.console_video) {
	  scrtest_bitmap = 1 << (24 + screen);
	  vm86s.screen_bitmap = -1;
	  update_screen = 1;
	}
	else
	  set_vc_screen_page(screen);
      }
      else {
	error("ERROR: video error: set bad screen %d\n", screen);
	CARRY;
      }

      bios_current_screen_page = screen;
      bios_video_memory_address = TEXT_SIZE * screen;

      poscur(bios_cursor_x_position(bios_current_screen_page), 
	     bios_cursor_y_position(bios_current_screen_page));
      break;
    }

  case 0x6:			/* scroll up */
    v_printf("scroll up %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    scrollup(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    if (!config.console_video)
      vm86s.screen_bitmap = -1;
    break;

  case 0x7:			/* scroll down */
    v_printf("scroll dn %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    scrolldn(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    if (!config.console_video)
      vm86s.screen_bitmap = -1;
    break;

  case 0x8:			/* read character at x,y + attr */
    screen = HI(bx);
    if (screen > max_page) {
      error("ERROR: read char from bad page %d\n", screen);
      CARRY;
      break;
    }
    sm = SCREEN_ADR(screen);
    REG(eax) = sm[CO * bios_cursor_y_position(screen) 
		     + bios_cursor_x_position(screen)];
    break;

    /* these two put literal character codes into memory, and do
       * not scroll or move the cursor...
       * the difference is that 0xA ignores color for text modes
       */
  case 0x9:
  case 0xA:
    {
      u_short *sadr;
      u_char attr = LO(bx);

      screen = HI(bx);
      sadr = (u_short *) SCREEN_ADR(screen) 
	     + bios_cursor_y_position(screen) * CO 
	     + bios_cursor_x_position(screen);
      x = (us) REG(ecx);
      c = LO(ax);

      /* XXX - need to make sure this doesn't overrun video memory!
       * we don't really know how large video memory is yet (I haven't
       * abstracted things yet) so we'll just leave it for the
       * Great Video Housecleaning foretold in the TODO file of the
       * ancients
       */
      if (HI(ax) == 9)
	for (i = 0; i < x; i++)
	  *(sadr++) = c | (attr << 8);
      else
	for (i = 0; i < x; i++) {	/* leave attribute as it is */
	  *sadr &= 0xff00;
	  *(sadr++) |= c;
	}

      if (!config.console_video)
	update_screen = 1;

      break;
    }

  case 0xe:			/* print char */
    char_out(*(char *) &REG(eax), bios_current_screen_page, ADVANCE);	/* char in AL */
    break;

  case 0x0f:			/* get screen mode */
    REG(eax) = (CO << 8) | screen_mode;
    v_printf("get screen mode: 0x%04x s=%d\n", LWORD(eax), bios_current_screen_page);
    HI(bx) = bios_current_screen_page;
    break;

#if 0
  case 0xb:			/* palette */
  case 0xc:			/* set dot */
  case 0xd:			/* get dot */
    break;
#endif

  case 0x4:			/* get light pen */
    error("ERROR: video error(no light pen)\n");
    CARRY;
    return;

  case 0x1a:			/* get display combo */
    if (LO(ax) == 0) {
      v_printf("get display combo!\n");
      LO(ax) = 0x1a;		/* valid function=0x1a */
      LO(bx) = video_combo;	/* active display */
      HI(bx) = 0;		/* no inactive display */
    }
    else {
      v_printf("set display combo not supported\n");
    }
    break;

  case 0x12:			/* video subsystem config */
    v_printf("get video subsystem config ax=0x%04x bx=0x%04x\n",
	     LWORD(eax), LWORD(ebx));
    switch (LO(bx)) {
    case 0x10:
      HI(bx) = video_subsys;
      /* this breaks qedit! (any but 0x10) */
      /* LO(bx)=3;  */
      v_printf("video subsystem 0x10 BX=0x%04x\n", LWORD(ebx));
      REG(ecx) = 0x0809;
      break;
    case 0x20:
      v_printf("select alternate printscreen\n");
      break;
    case 0x30:
      if (LO(ax) == 0)
	tmp = 200;
      else if (LO(ax) == 1)
	tmp = 350;
      else
	tmp = 400;
      v_printf("select textmode scanlines: %d", tmp);
      LO(ax) = 12;		/* ok */
      break;
    case 0x32:			/* enable/disable cpu access to cpu */
      if (LO(ax) == 0)
	v_printf("disable cpu access to video!\n");
      else
	v_printf("enable cpu access to video!\n");
      break;
    default:
      error("ERROR: unrecognized video subsys config!!\n");
      show_regs();
    }
    break;

  case 0xfe:			/* get shadow buffer..return unchanged */
  case 0xff:			/* update shadow buffer...do nothing */
    break;

  case 0x11:			/* ega character generator */
  case 0x10:			/* ega palette */
  case 0x4f:			/* vesa interrupt */

  default:
    v_printf("new unknown video int 0x%x\n", LWORD(eax));
    CARRY;
    break;
  }
}

