
/* ====================================================================== */

#include "config.h"

/* Define if we want graphics in X (of course we want :-) (root@zaphod) */
/* WARNING: This may not work in BSD, because it was written for Linux! */

/* GUS PnP support: I added code here and in cpu.c which allows you to
   initialize the GUS under dosemu, while the Linux support is still in
   the works. It is clearly a better solution than the loadlin/warmboot
   method.
   You have to use -DGUSPNP in the local Makefile to activate it.
   WARNING: the HW addresses are hardcoded; I obtained them by running
   isapnptools. On your system they can change depending on the other
   PnP hardware you have; so check them before - AV
*/

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
#include "bitops.h"
#if X_GRAPHICS
#include <sys/mman.h>           /* root@sjoerd*/
#endif

#include "emu.h"
#include "bios.h"
#include "mouse.h"
#include "serial.h"
#include "xms.h"
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
#include "priv.h"

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

#ifdef USE_SBEMU
#include "sound.h"
#endif

#include "keyb_server.h"
#include "keyboard.h"

#include "dma.h"
#include "cpu-emu.h"

extern Bit8u spkr_io_read(Bit32u port);
extern void spkr_io_write(Bit32u port, Bit8u value);

/* ====================================================================== */

static long nyb2bin[16] = {
	0x30303030, 0x31303030, 0x30313030, 0x31313030,
	0x30303130, 0x31303130, 0x30313130, 0x31313130,
	0x30303031, 0x31303031, 0x30313031, 0x31313031,
	0x30303131, 0x31303131, 0x30313131, 0x31313131
};

static char *
p2bin (unsigned char c)
{
  static char s[16] = "   [00000000]";

  ((long *)s)[1]=nyb2bin[(c>>4)&15];
  ((long *)s)[2]=nyb2bin[c&15];
  
  return s+3;
}

/* ====================================================================== */
/*  */
/* inb,inw,ind,outb,outw,outd @@@  32768 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/arch/linux/async/sigsegv.c --> src/emu-i386/ports.c  */
/* PORT_DEBUG is to specify whether to record port writes to debug output.
* 0 means disabled.
* 1 means record all port accesses to 0x00 to 0xFF
* 2 means record ANY port accesses!  (big fat debugfile!)
* 3 means record all port accesses from 0x100 to 0x3FF
*/ 
#define PORT_DEBUG 3


/*                              3b0         3b4         3b8         3bc                     */
const unsigned char ATIports[]={ 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, /* 3b0-3bf */
                                 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, /* 3c0-3cf */
                                 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0  /* 3d0-3df */
                               };

int isATIport(int port)
{
  return (    (port & 0x3ff) == 0x102
          ||  (port & 0x3fe) == 0x1ce
          ||  (port & 0x3fc) == 0x2ec
          || ((port & 0x3ff) >= 0x3b0 && (port & 0x3ff) < 0x3e0 && ATIports[(port & 0x3ff)-0x3b0])
          ||  port == 0x46e8
         );
}

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
unsigned char
inb(unsigned int port)
{
  PRIV_SAVE_AREA

  static unsigned int cga_r = 0;
  static unsigned char r;
  static unsigned int tmp = 0;

/* it is a fact of (hardware) life that unused locations return all
   (or almost all) the bits at 1; some software can try to detect a
   card basing on this fact and fail if it reads 0x00 - AV */
  r = 0xff;

  port &= 0xffff;
/* On my system MS-DOS 7 startup makes a lot of reads to port 0x80, which
   is undefined in the AT architecture; I don't know why. Also, linux
   uses successfully since 1991 this port as a 'nop' hardware delay, and
   so did I under djgpp. Let's assume that accessing port 0x80 is only
   a delay operation, and don't do anything with it - AV */
  if (port == 0x80) return 0xff;

  if (port_readable((u_int)port)) {
    r = read_port((u_int)port);
    return r;
  }

#if X_GRAPHICS
  if ( (config.X) &&
            ( ((port>=0x3c0) && (port<=0x3c1)) || /* root@zaphod attr ctrl */
              ((port>=0x3c4) && (port<=0x3c5)) || /* erik@zaphod sequencer */
              ((port>=0x3c6) && (port<=0x3c9)) || /* root@zaphod */
#ifdef NEW_X_CODE
              ((port>=0x3d4) && (port<=0x3d5)) || /* sw: crt controller */
#endif
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
  if (config.chipset && (port > 0x3b3) && (port < 0x3df) && config.mapped_bios)
    r = (video_port_in((u_int)port));
  else if (v_8514_base && ((port & 0x03fe) == v_8514_base) && (port & 0xfc00)) {
    enter_priv_on();
    iopl(3);
    r = port_in((u_int)port) & 0xff;
    iopl(0);
    leave_priv_setting();
    v_printf("8514 inb [0x%04x] = 0x%02x\n", port, r);
  }
  else if ((config.chipset == ATI) && isATIport(port) && (port & 0xfc00)) {
    enter_priv_on();
    iopl(3);
    r = port_in(port) & 0xff;
    iopl(0);
    leave_priv_setting();
    v_printf("ATI inb [0x%04x] = 0x%02x\n", port, r);
  }
#endif
  else switch ((u_int)port) {
  case 0x20:
  case 0x21:
    r = read_pic0((u_int)port);
    return r;		/* use 'break' if you want to trace it */
  case 0xa0:
  case 0xa1:
    r = read_pic1((u_int)port);
    return r;
  case 0x60:
  case 0x64:
    r = keyb_io_read((u_int)port);
    return r;
  case 0x61:
    r = spkr_io_read((u_int)port);
    return r;
  case 0x70:
  case 0x71:
    r = cmos_read((u_int)port);
    break;		/* always trace this one */
  case 0x40:
  case 0x41:
  case 0x42:
    r = pit_inp((u_int)port);
    return r;
  case 0x43:
    r = inb((u_int)port);
    return r;
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
    
#ifdef GUSPNP
  case 0x203:
  case 0x207:
  case 0x20b:
  case 0x220 ... 0x22f:
  case 0x320 ... 0x32f:
/*  case 0x279: */
    r = safe_port_in_byte (port);
    return r;

  case 0xa79:
    enter_priv_on();
    iopl (3);
    r = port_in (port);
    iopl (0);
    leave_priv_setting();
    break;
#endif
  default:
    /* SERIAL PORT I/O.  The base serial port must be a multiple of 8. */
    for (tmp = 0; tmp < config.num_ser; tmp++)
      if ((port & ~7) == com[tmp].base_port) {
        r = do_serial_in(tmp, port);
        return r;
      }

#ifdef USE_SBEMU
    /* Sound I/O */
    if ((port & SOUND_IO_MASK) == config.sb_base) {
      r=sb_io_read(port);
      return r;
    }
    /* It seems that we might need 388, but this is write-only, at least in the
       older chip... */
    if ((port & ~3) == 0x388) {
      r=adlib_io_read(port);
      return r;
    }
    /* MPU401 */
    if ((port & ~1) == config.mpu401_base) {
	r=mpu401_io_read(port);
	return r;
    }
#endif /* USE_SBEMU */

    /* DMA I/O */
    if ( ((port & ~15) == 0) || ((port & ~15) == 0x80) || ((port & ~31) == 0xC0) ) {
       r=dma_io_read(port);
    }

    /* The diamond bug */
    else if (config.chipset == DIAMOND && (port >= 0x23c0) && (port <= 0x23cf)) {
      enter_priv_on();
      iopl(3);
      r = port_in(port);
      iopl(0);
      leave_priv_setting();
      i_printf(" Diamond inb [0x%x] = 0x%x\n", port, r);
    }
    else {
    i_printf("default inb [0x%x] = 0x%02x\n", port, r);
      h_printf("read port 0x%x dummy return 0xff", port);
    h_printf(" because not in access list\n");
  }
  }

/* Now record the port and the read value to debugfile if needed */
#if PORT_DEBUG > 0
#if PORT_DEBUG == 1
  if (port < 0x100)
#elif PORT_DEBUG == 3
  if (port >= 0x100)
#endif
    i_printf ("PORT: Rd 0x%04x -> 0x%02x\n", port, r);
#endif

  return r;    /* Return with port read value */
}

unsigned int
inw(int port)
{
  PRIV_SAVE_AREA
  if (v_8514_base && ((port & 0x03fd) == v_8514_base) && (port & 0xfc00)) {
    int value;

    enter_priv_on();
    iopl(3);
    value = port_in_w(port) & 0xffff;
    iopl(0);
    leave_priv_setting();
    v_printf("8514 inw [0x%04x] = 0x%04x\n", port, value);
    return value;
  }
  else if ((config.chipset == ATI) && isATIport(port) && (port & 0xfc00)) {
    int value;

    enter_priv_on();
    iopl(3);
    value = port_in_w(port) & 0xffff;
    iopl(0);
    leave_priv_setting();
    v_printf("ATI inw [0x%04x] = 0x%04x\n", port, value);
    return value;
  }
  return( read_port_w(port) );
}

unsigned int
ind(int port)
{
  PRIV_SAVE_AREA
  int v;
  if (config.pci && (port >= 0xcf8) && (port < 0xd00)) {
    enter_priv_on();
    iopl(3);
    v=port_real_ind(port);
    iopl(0);
    leave_priv_setting();
    return v;
  }
  v=read_port_w(port) & 0xffff;
  return (read_port_w(port+2)<< 16) | v;
}

void
outb(unsigned int port, unsigned int byte)
{
  PRIV_SAVE_AREA
  static int lastport = 0;
  static unsigned int tmp = 0;

  port &= 0xffff;
  if (port == 0x80) return;   /* used by linux, djgpp.. see above */
  byte &= 0xff;

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
#ifndef NEW_X_CODE
    return;
  }
  else {
#else
  }
  /* the above stuff should someday be moved to crtcemu.c -- sw */
  {
#endif
#if X_GRAPHICS
    if ( (config.X) &&
         ( ((port>=0x3c0) && (port<=0x3c1)) || /* root@zaphod attr ctrl */
           ((port>=0x3c4) && (port<=0x3c5)) || /* erik@zaphod sequencer */
           ((port>=0x3c6) && (port<=0x3c9)) || /* root@zaphod */
#ifdef NEW_X_CODE
           ((port>=0x3d4) && (port<=0x3d5)) || /* sw: crt controller */
#endif
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

  if (port > 0x3b3 && port < 0x3df && config.chipset && config.mapped_bios) {
    video_port_out(byte, port);
    return;
  }
  if (v_8514_base && ((port & 0x03fe) == v_8514_base) && (port & 0xfc00)) {
    enter_priv_on();
    iopl(3);
    port_out(byte, port);
    iopl(0);
    leave_priv_setting();
    v_printf("8514 outb [0x%04x] = 0x%02x\n", port, byte);
    return;
  }
  if ((config.chipset == ATI) && isATIport(port) && (port & 0xfc00)) {
    enter_priv_on();
    iopl(3);
    port_out(byte, port);
    iopl(0);
    leave_priv_setting();
    v_printf("ATI outb [0x%04x] = 0x%02x\n", port, byte);
    return;
  }

  /* The diamond bug */
  if (config.chipset == DIAMOND && (port >= 0x23c0) && (port <= 0x23cf)) {
    enter_priv_on();
    iopl(3);
    port_out(byte, port);
    iopl(0);
    leave_priv_setting();
    i_printf(" Diamond outb [0x%x] = 0x%x\n", port, byte);
    return;
  }

#if PORT_DEBUG > 0
#if PORT_DEBUG == 1
  if (port < 0x100)
#elif PORT_DEBUG == 3
  if (port >= 0x100)
#endif
    i_printf ("PORT: Wr 0x%04x <- 0x%02x\n", port, byte);
#endif

  switch (port) {
  case 0x20:
  case 0x21:
    write_pic0(port, byte);
    break;
  case 0x60:
  case 0x64:
    keyb_io_write((u_int)port, byte);
    break;
  case 0x61:
    spkr_io_write((u_int)port, byte);
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

#ifdef GUSPNP
  case 0x203:
  case 0x207:
  case 0x20b:
  case 0x220 ... 0x22f:
  case 0x320 ... 0x32f:
  case 0x279:
    set_ioperm (port, 1, 1);
    port_out (byte, port);
    set_ioperm (port, 1, 0);
    break;

  case 0xa79:
    enter_priv_on();
    iopl (3);
    port_out (byte, port);
    iopl (0);
    leave_priv_setting();
  break;
#endif

  default:
    /* DMA I/O */
    if (((port & ~15) == 0) || ((port & ~15) == 0x80) || ((port & ~31) == 0xC0)) { 
      dma_io_write(port, byte);
    }
    /* SERIAL PORT I/O.  Avoids port==0 for safety.  */
    /* The base serial port must be a multiple of 8. */
    for (tmp = 0; tmp < config.num_ser; tmp++) {
      if ((port & ~7) == com[tmp].base_port) {
      do_serial_out(tmp, port, byte);
      lastport = port;
      return;
      }
    }
#ifdef USE_SBEMU
    /* Sound I/O */
    if ((port & SOUND_IO_MASK) == config.sb_base) {
      sb_io_write(port, byte);
      break;
    }
    else if ((port & ~3) == 0x388) {
      adlib_io_write(port, byte);
      break;
    }
    /* MPU401 */
    if ((port & ~1) == config.mpu401_base) {
      mpu401_io_write(port, byte);
      break;
    }
#endif /* USE_SBEMU */

    i_printf("default outb [0x%x] 0x%02x\n", port, byte);
    h_printf("write port 0x%x denied value %02x", port, (byte & 0xff));
    h_printf(" because not in access list\n");
  }
  lastport = port;
}

void
outw(unsigned int port, unsigned int value)
{
  PRIV_SAVE_AREA
  if (v_8514_base && ((port & 0x03fd) == v_8514_base) && (port & 0xfc00)) {
    enter_priv_on();
    iopl(3);
    port_out_w(value, port);
    iopl(0);
    leave_priv_setting();
    v_printf("8514 outw [0x%04x] = 0x%04x\n", port, value);
    return;
  }
  if ((config.chipset == ATI) && isATIport(port) && (port & 0xfc00)) {
    enter_priv_on();
    iopl(3);
    port_out_w(value, port);
    iopl(0);
    leave_priv_setting();
    v_printf("ATI outw [0x%04x] = 0x%04x\n", port, value);
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
  PRIV_SAVE_AREA
  port &= 0xffff;
  if (config.pci && (port >= 0xcf8) && (port < 0xd00)) {
    enter_priv_on();
    iopl(3);
    port_real_outd(port, value);
    iopl(0);
    leave_priv_setting();
    return;
  }
  outw(port,value & 0xffff);
  outw(port+2,(unsigned int)value >> 16);
}
/* @@@ MOVE_END @@@ 32768 */


/* The following functions were previously in portss.c */

Bit8u port_safe_inb(Bit32u port)
{
  PRIV_SAVE_AREA
  Bit8u res = 0;

  i_printf("PORT: safe_inb ");
  if (i_am_root) {
    enter_priv_on();
    iopl(3);
    res = port_in(port);
    iopl(0);

    leave_priv_setting();
  }
  else
    i_printf("want to ");
  i_printf("in(%lx)", port);
  if (i_am_root)
    i_printf(" = %hx", res);
  i_printf("\n");
  return res;
}

void port_safe_outb(Bit32u port, Bit8u byte)
{
  PRIV_SAVE_AREA
  i_printf("PORT: safe_outb ");
  if (i_am_root) {
    enter_priv_on();
    iopl(3);
    port_out(byte, port);
    iopl(0);
    leave_priv_setting();
  }
  else
    i_printf("want to ");
  i_printf("outb(%lx, %hx)\n", port, byte);
}

Bit16u port_safe_inw(Bit32u port)
{
  PRIV_SAVE_AREA
  Bit16u res = 0;

  i_printf("PORT: safe_inw ");
  if (i_am_root) {
    enter_priv_on();
    iopl(3);
    res = port_in_w(port);
    iopl(0);
    leave_priv_setting();
  }
  else
    i_printf("want to ");
  i_printf("inw(%lx)", port);
  if (i_am_root)
    i_printf(" = %x", res);
  i_printf("\n");
  return res;
}

void port_safe_outw(Bit32u port, Bit16u word)
{
  PRIV_SAVE_AREA
  i_printf("PORT: safe_outw ");
  if (i_am_root) {
    enter_priv_on();
    iopl(3);
    port_out_w(word, port);
    iopl(0);
    leave_priv_setting();
  }
  else
    i_printf("want to ");
  i_printf("outw(%lx, %x)\n", port, word);
}


int port_init(void)
{
  /* this function intentionally left blank */
  return 0;
}


/*  */
/* set_ioperm @@@  40960 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/emu.c --> src/emu-i386/ports.c  */
/* return status of io_perm call */
int
set_ioperm(int start, int size, int flag)
{
    PRIV_SAVE_AREA
    int             tmp;

    if (!i_am_root)
	return -1;		/* don't bother */

#ifdef X86_EMULATOR
    if (config.cpuemu) {
	int i;
	for (i=start; i<start+size; i++)
		(flag? set_bit(i,io_bitmap):clear_bit(i,io_bitmap));
	tmp = 0;
    }
#endif
    enter_priv_on();
    tmp = ioperm(start, size, flag);
    leave_priv_setting();

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

