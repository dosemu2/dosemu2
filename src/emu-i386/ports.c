#include "config.h"

/* Define if we want graphics in X (of course we want :-) (root@zaphod) */
/* WARNING: This may not work in BSD, because it was written for Linux! */

#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/times.h>
#ifdef __NetBSD__
#include <errno.h>
#endif
#if X_GRAPHICS
#include <sys/mman.h>           /* root@sjoerd*/
#endif

#include "emu.h"
#include "bios.h"
#include "mouse.h"
#include "serial.h"
#include "xms.h"
#include "keyboard.h"
#include "timers.h"
#include "cmos.h"
#include "memory.h"
#include "termio.h"
#include "config.h"
#include "port.h"
#include "int.h"
#include "hgc.h"
#include "dosio.h"
#include "timers.h"

#include "video.h"
#if X_GRAPHICS
#include "vgaemu.h" /* root@zaphod */
#endif

#include "pic.h"
#include "dpmi.h"

#ifdef USING_NET
#include "ipx.h"
#endif

/* Needed for DIAMOND define */
#include "vc.h"

#include "sound.h"

#include "dma.h"

/*  */
/* inb,inw,ind,outb,outw,outd @@@  32768 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/arch/linux/async/sigsegv.c --> src/emu-i386/ports.c  */
/* PORT_DEBUG is to specify whether to record port writes to debug output.
 * 0 means disabled.
 * 1 means record all port accesses to 0x00 to 0xFF
 * 2 means record ANY port accesses!  (big fat debugfile!)
 */ 
#define PORT_DEBUG 0

extern void set_leds(void);
/* FIXME -- move to common header */
extern int s3_8514_base;
static u_short microsoft_port_check = 0;

/*
 * DANG_BEGIN_FUNCTION inb
 *
 * description:
 *  INB is used to do controlled emulation of input from ports.
 *
 * arguments:
 *  port - port to input from.
 *
 * DANG_END_FUNCTION
 */
unsigned int
inb(unsigned int port)
{

  static unsigned int cga_r = 0;
  static unsigned int r;
  static unsigned int tmp = 0;

/* it is a fact of (hardware) life that unused locations return all
   (or almost all) the bits at 1; some software can try to detect a
   card basing on this fact and fail if it reads 0x00 - AV */
  r = 0xff;

  port &= 0xffff;
  if (port_readable((u_int)port))
    r = (read_port((u_int)port) & 0xFF);
#if X_GRAPHICS
  else if ( (config.X) &&
            ( ((port>=0x3c0) && (port<=0x3c1)) || /* root@zaphod attr ctrl */
              ((port>=0x3c6) && (port<=0x3c9)) || /* root@zaphod */
              ((port>=0x3D0) && (port<=0x3DD)) ) )
    {
        r=VGA_emulate_inb((u_int)port);
    }
#endif
  else if (config.usesX) {
    v_printf("HGC Portread: %d\n", (int) port);
    switch ((u_int)port) {
    case 0x03b8:		/* mode-reg */
	r = safe_port_in_byte((u_int)port);
      r = (r & 0x7f) | (hgc_Mode & 0x80);
      break;
    case 0x03ba:		/* status-reg */
	r = safe_port_in_byte((u_int)port);
      break;
    case 0x03bf:		/* conf-reg */
      set_ioperm(port, 1, 1);
      r = port_in((u_int)port);
      set_ioperm(port, 1, 0);
      r = (r & 0xfd) | (hgc_Konv & 0x02);
      break;
    case 0x03b4:		/* adr-reg */
    case 0x03b5:		/* data-reg */
	r = safe_port_in_byte((u_int)port);
      break;
    }
  }
#if 1
  else if (config.chipset && port > 0x3b3 && port < 0x3df && config.mapped_bios)
    r = (video_port_in((u_int)port));
  else if ((config.chipset == S3) && ((port & 0x03fe) == s3_8514_base) && (port & 0xfc00)) {
    iopl(3);
    r = port_in((u_int)port) & 0xff;
    iopl(0);
    v_printf("S3 inb [0x%04x] = 0x%02x\n", port, r);
  }
#endif
  else switch ((u_int)port) {
  case 0x20:
  case 0x21:
    r = read_pic0((u_int)port);
    break;
  case 0xa0:
  case 0xa1:
    r = read_pic1((u_int)port);
    break;
  case 0x60:
  case 0x61:
  case 0x64:
    r = keyb_io_read((u_int)port);
    break;
  case 0x70:
  case 0x71:
    r = cmos_read((u_int)port);
    break;
  case 0x40:
  case 0x41:
  case 0x42:
    r = pit_inp((u_int)port);
    break;
  case 0x43:
    r = port_inb((u_int)port);
    break;
  case 0x3ba:
  case 0x3da:
    /* graphic status - many programs will use this port to sync with
     * the vert & horz retrace so as not to cause CGA snow */
    i_printf("3ba/3da port inb\n");
    r = (cga_r ^= 1) ? 0xcf : 0xc6;
    break;

  case 0x3bc:
    i_printf("printer port inb [0x3bc] = 0\n");    /* 0 by default */
    break;

  case 0x3db:			/* light pen strobe reset, 0 by default */
    break;
    
  default:
    /* SERIAL PORT I/O.  The base serial port must be a multiple of 8. */
    for (tmp = 0; tmp < config.num_ser; tmp++)
      if ((port & ~7) == com[tmp].base_port) {
        r = do_serial_in(tmp, port);
        break;
      }

    /* Sound I/O */
    if ((port & SOUND_IO_MASK) == SOUND_BASE) {r=sb_read(port & SOUND_IO_MASK2);};
    /* It seems that we might need 388, but this is write-only, at least in the
       older chip... */

    /* DMA I/O */
    if ((port & ~15) == 0) {r=dma_read(port);};
    if ((port & ~15) == 0x80) {r=dma_read(port);};
    if ((port & ~31) == 0xC0) {r=dma_read(port);};

    /* The diamond bug */
    if (config.chipset == DIAMOND && (port >= 0x23c0) && (port <= 0x23cf)) {
      iopl(3);
      r = port_in(port);
      iopl(0);
      i_printf(" Diamond inb [0x%x] = 0x%x\n", port, r);
      break;
    }
    i_printf("default inb [0x%x] = 0x%02x\n", port, r);
  }

/* Now record the port and the read value to debugfile if needed */
#if PORT_DEBUG > 0
#if PORT_DEBUG == 1
  if (port < 0x100)
#endif
    i_printf("PORT: Rd 0x%04x -> 0x%02x\n",port,r);
#endif

  return r;    /* Return with port read value */
}

int
inw(int port)
{
  if ((config.chipset == S3) && ((port & 0x03ff) == s3_8514_base) && (port & 0xfc00)) {
    int value;

    iopl(3);
    value = port_in_w(port) & 0xffff;
    iopl(0);
    v_printf("S3 inw [0x%04x] = 0x%04x\n", port, value);
    return value;
  }
  return( read_port_w(port) );
}

int
ind(int port)
{
  int v=read_port_w(port) & 0xffff;
  return (read_port_w(port+2)<< 16) | v;
}

void
outb(unsigned int port, unsigned int byte)
{
  static int lastport = 0;
  static unsigned int tmp = 0;

  port &= 0xffff;
  byte &= 0xff;

#if PORT_DEBUG > 0
#if PORT_DEBUG == 1
  if (port < 0x100)
#endif
    i_printf("PORT: Wr 0x%04x <- 0x%02x\n",port,byte);
#endif

  if (port_writeable(port)) {
    write_port(byte, port);
    return;
  }

  /* Port writes for enable/disable blinking character mode */
  if (port == 0x03C0) {
    static int last_byte = -1;
    static int last_index = -1;
    static int flip_flop = 1;

    flip_flop = !flip_flop;
    if (flip_flop) {
/* JES This was last_index = 0x10..... WRONG? */
      if (last_index == 0x10)
	char_blink = (byte & 8) ? 1 : 0;
      last_byte = byte;
    }
    else {
      last_index = byte;
    }
    return;
  }

  /* Port writes for cursor position */
  if ((port & 0xfffe) == READ_WORD(BIOS_VIDEO_PORT)) {
    /* Writing to the 6845 */
    static int last_port;
    static int last_byte;
    static int hi = 0, lo = 0;
    int pos;

    v_printf("Video Port outb [0x%04x]\n", port);
    if (!config.usesX) {
      if ((port == READ_WORD(BIOS_VIDEO_PORT) + 1) && (last_port == READ_WORD(BIOS_VIDEO_PORT))) {
	/* We only take care of cursor positioning for now. */
	/* This code should work most of the time, but can
	     be defeated if i/o permissions change (e.g. by a vt
	     switch) while a new cursor location is being written
	     to the 6845. */
	if (last_byte == 14) {
	  hi = (unsigned char) byte;
	  pos = (hi << 8) | lo;
	  cursor_col = pos % 80;
	  cursor_row = pos / 80;
	  if (config.usesX)
	    poshgacur(cursor_col,
		      cursor_row);
	}
	else if (last_byte == 15) {
	  lo = (unsigned char) byte;
	  pos = (hi << 8) | lo;
	  cursor_col = pos % 80;
	  cursor_row = pos / 80;
	  if (config.usesX)
	    poshgacur(cursor_col,
		      cursor_row);
	}
      }
      last_port = port;
      last_byte = byte;
    }
    else {
      set_ioperm(port, 1, 1);
      port_out(byte, port);
      set_ioperm(port, 1, 0);
    }
    return;
  }
  else {
#if X_GRAPHICS
    if ( (config.X) &&
         ( ((port>=0x3c0) && (port<=0x3c1)) || /* root@zaphod attr ctrl */
           ((port>=0x3c6) && (port<=0x3c9)) || /* root@zaphod */
           ((port>=0x3D0) && (port<=0x3DD)) ) )
      {
        VGA_emulate_outb(port, (unsigned char)byte);
        return;
      }
#endif
    if (config.usesX) {
      v_printf("HGC Portwrite: %d %d\n", (int) port, (int) byte);
      switch (port) {
      case 0x03b8:		/* mode-reg */
	if (byte & 0x80)
	  set_hgc_page(1);
	else
	  set_hgc_page(0);
	set_ioperm(port, 1, 1);
	port_out(byte & 0x7f, port);
	set_ioperm(port, 1, 0);
	break;
      case 0x03ba:		/* status-reg */
	set_ioperm(port, 1, 1);
	port_out(byte, port);
	set_ioperm(port, 1, 0);
	break;
      case 0x03bf:		/* conf-reg */
	if (byte & 0x02)
	  map_hgc_page(1);
	else
	  map_hgc_page(0);
	set_ioperm(port, 1, 1);
	port_out(byte & 0xFD, port);
	set_ioperm(port, 1, 0);
	break;
      }
      return;
    }
  }

#if 1
  if (port > 0x3b3 && port < 0x3df && config.chipset && config.mapped_bios)
    video_port_out(byte, port);
  if ((config.chipset == S3) && ((port & 0x03fe) == s3_8514_base) && (port & 0xfc00)) {
    iopl(3);
    port_out(byte, port);
    iopl(0);
    v_printf("S3 outb [0x%04x] = 0x%02x\n", port, byte);
    return;
  }
#endif

  /* The diamond bug */
  if (config.chipset == DIAMOND && (port >= 0x23c0) && (port <= 0x23cf)) {
    iopl(3);
    port_out(byte, port);
    iopl(0);
    i_printf(" Diamond outb [0x%x] = 0x%x\n", port, byte);
    return;
  }

  switch (port) {
  case 0x20:
  case 0x21:
    write_pic0(port, byte);
    break;
  case 0x60:
  case 0x64:
  case 0x61:
    keyb_io_write((u_int)port, byte);
    break;
  case 0x70:
  case 0x71:
    cmos_write(port, byte);
    break;
  case 0xa0:
  case 0xa1:
    write_pic1(port,byte);
    break;
  case 0x40:
  case 0x41:
  case 0x42:
    pit_outp(port, byte);
    break;
  case 0x43:
    pit_control_outp(port, byte);
    break;
  default:
    /* SERIAL PORT I/O.  Avoids port==0 for safety.  */
    /* The base serial port must be a multiple of 8. */
    for (tmp = 0; tmp < config.num_ser; tmp++) {
      if ((port & ~7) == com[tmp].base_port) {
      do_serial_out(tmp, port, byte);
      lastport = port;
      return;
      }
    }
    /* Sound I/O */
    if ((port & SOUND_IO_MASK) == SOUND_BASE) {sb_write(port & SOUND_IO_MASK2, byte);};
    if ((port & ~3) == 388) {fm_write(port & 3, byte);};

    /* DMA I/O */
    if ((port & ~15) == 0) {dma_write(port, byte);};
    if ((port & ~15) == 0x80) {dma_write(port, byte);};
    if ((port & ~31) == 0xC0) {dma_write(port, byte);};

    i_printf("outb [0x%x] 0x%02x\n", port, byte);
  }
  lastport = port;
}

void
outw(unsigned int port, unsigned int value)
{
  if ((config.chipset == S3) && ((port & 0x03ff) == s3_8514_base) && (port & 0xfc00)) {
    iopl(3);
    port_out_w(value, port);
    iopl(0);
    v_printf("S3 outw [0x%04x] = 0x%04x\n", port, value);
    return;
  }
  if(!write_port_w(value,port) ) {
    outb(port, value & 0xff);
    outb(port + 1, value >> 8);
  }

}

void
outd(unsigned int port, unsigned int value)
{
  outw(port,value & 0xffff);
  outw(port+2,(unsigned int)value >> 16);
}
/* @@@ MOVE_END @@@ 32768 */



/*  */
/* set_ioperm @@@  40960 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/emu.c --> src/emu-i386/ports.c  */
/* return status of io_perm call */
int
set_ioperm(int start, int size, int flag)
{
    int             tmp;

    if (!i_am_root)
	return -1;		/* don't bother */

    tmp = ioperm(start, size, flag);
    return tmp;
}
/* @@@ MOVE_END @@@ 40960 */



#ifdef __NetBSD__
/*  */
/* NetBSD:set_bitmap,ioperm @@@  49152 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/emu.c --> src/emu-i386/ports.c  */
/* lifted from linux kernel, ioport.c */

/* Set EXTENT bits starting at BASE in BITMAP to value TURN_ON. */
static void
set_bitmap(unsigned long *bitmap, short base, short extent, int new_value)
{
	int mask;
	unsigned long *bitmap_base = bitmap + (base >> 5);
	unsigned short low_index = base & 0x1f;
	int length = low_index + extent;

	if (low_index != 0) {
		mask = (~0 << low_index);
		if (length < 32)
				mask &= ~(~0 << length);
		if (new_value)
			*bitmap_base++ |= mask;
		else
			*bitmap_base++ &= ~mask;
		length -= 32;
	}

	mask = (new_value ? ~0 : 0);
	while (length >= 32) {
		*bitmap_base++ = mask;
		length -= 32;
	}

	if (length > 0) {
		mask = ~(~0 << length);
		if (new_value)
			*bitmap_base++ |= mask;
		else
			*bitmap_base++ &= ~mask;
	}
}

#include <machine/sysarch.h>
#include <machine/pcb.h>

int
ioperm(unsigned int startport, unsigned int howmany, int onoff)
{
    unsigned long bitmap[NIOPORTS/32];
    int err;

    if (startport + howmany > NIOPORTS)
	return ERANGE;

    if ((err = i386_get_ioperm(bitmap)) != 0)
	return err;
    i_printf("%sabling %x->%x\n", onoff ? "en" : "dis", startport, startport+howmany);
    /* now diddle the current bitmap with the request */
    set_bitmap(bitmap, startport, howmany, !onoff);

    return i386_set_ioperm(bitmap);
}
/* @@@ MOVE_END @@@ 49152 */


#endif

