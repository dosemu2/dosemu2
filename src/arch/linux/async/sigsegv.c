/*
 * $Date: 1995/04/08 22:30:40 $
 * $Source: /usr/src/dosemu0.60/dosemu/RCS/sigsegv.c,v $
 * $Revision: 2.20 $
 * $State: Exp $
 *
 * $Log: sigsegv.c,v $
 * Revision 2.20  1995/04/08  22:30:40  root
 * Release dosemu0.60.0
 *
 * Revision 2.20  1995/04/08  22:30:40  root
 * Release dosemu0.60.0
 *
 * Revision 2.19  1995/02/26  00:54:47  root
 * *** empty log message ***
 *
 * Revision 2.18  1995/02/25  22:37:52  root
 * *** empty log message ***
 *
 * Revision 2.17  1995/02/25  21:52:39  root
 * *** empty log message ***
 *
 * Revision 2.16  1995/02/05  16:52:03  root
 * Prep for Scotts patches.
 *
 * Revision 2.15  1995/01/14  15:29:17  root
 * New Year checkin.
 *
 * Revision 2.14  1994/11/03  11:43:26  root
 * Checkin Prior to Jochen's Latest.
 *
 * Revision 2.13  1994/10/14  17:58:38  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.12  1994/10/03  00:24:25  root
 * Checkin prior to pre53_25.tgz
 *
 * Revision 2.11  1994/09/28  00:55:59  root
 * Prep for pre53_23.tgz
 *
 * Revision 2.10  1994/09/26  23:10:13  root
 * Prep for pre53_22.
 *
 * Revision 2.9  1994/09/23  01:29:36  root
 * Prep for pre53_21.
 *
 * Revision 2.8  1994/09/11  01:01:23  root
 * Prep for pre53_19.
 *
 * Revision 2.7  1994/08/14  02:52:04  root
 * Rain's latest CLEANUP and MOUSE for X additions.
 *
 * Revision 2.6  1994/08/05  22:29:31  root
 * Prep dir pre53_10.
 *
 * Revision 2.5  1994/08/01  14:26:23  root
 * Prep for pre53_7  with Markks latest, EMS patch, and Makefile changes.
 *
 * Revision 2.4  1994/07/09  14:29:43  root
 * prep for pre53_3.
 *
 * Revision 2.3  1994/06/27  02:15:58  root
 * Prep for pre53
 *
 * Revision 2.2  1994/06/24  14:51:06  root
 * Markks's patches plus.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.20  1994/06/03  00:58:55  root
 * pre51_23 prep, Daniel's fix for scrbuf malloc().
 *
 * Revision 1.19  1994/05/26  23:15:01  root
 * Prep. for pre51_21.
 *
 * Revision 1.18  1994/05/24  01:23:00  root
 * Lutz's latest, int_queue_run() update.
 *
 * Revision 1.17  1994/05/21  23:39:19  root
 * PRE51_19.TGZ with Lutz's latest updates.
 *
 * Revision 1.16  1994/05/16  23:13:23  root
 * Prep for pre51_16.
 *
 * Revision 1.15  1994/05/13  17:21:00  root
 * pre51_15.
 *
 * Revision 1.14  1994/05/10  23:08:10  root
 * pre51_14.
 *
 * Revision 1.13  1994/05/04  22:16:00  root
 * Patches by Alan to mouse subsystem.
 *
 * Revision 1.12  1994/05/04  21:56:55  root
 * Prior to Alan's mouse patches.
 *
 * Revision 1.11  1994/04/30  01:05:16  root
 * Clean Up.
 *
 * Revision 1.10  1994/04/27  23:39:57  root
 * Lutz's patches to get dosemu up under 1.1.9.
 *
 * Revision 1.9  1994/04/23  20:51:40  root
 * Get new stack over/underflow working in VM86 mode.
 *
 * Revision 1.8  1994/04/20  23:43:35  root
 * pre51_8 out the door.
 *
 * Revision 1.7  1994/04/18  20:57:34  root
 * Checkin prior to Jochen's latest patches.
 *
 * Revision 1.6  1994/04/16  01:28:47  root
 * Prep for pre51_6.
 *
 * Revision 1.5  1994/04/13  00:07:09  root
 * Lutz's patches
 *
 * Revision 1.4  1994/04/09  18:41:52  root
 * Prior to Lutz's kernel enhancements.
 *
 * Revision 1.3  1994/04/07  00:18:41  root
 * Pack up for pre52_4.
 *
 * Revision 1.2  1994/03/30  22:12:30  root
 * Prep for 0.51 pre 2.
 *
 * Revision 1.1  1994/03/24  22:40:31  root
 * Initial revision
 *
 */

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

static char rcsid[]="$Id: sigsegv.c,v 2.20 1995/04/08 22:30:40 root Exp $";

#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/times.h>
#ifdef __NetBSD__
#include <setjmp.h>
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

#include "video.h"

#include "pic.h"

#include "dpmi.h"

#ifdef USING_NET
#include "ipx.h"
#endif

/* Needed for DIAMOND define */
#include "vc.h"

#include "sound.h"

#include "dma.h"

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
 * DANG_BEGIN_FUNCTION inb(int port)
 *
 * description:
 *  INB is used to do controlled emulation of input from ports.
 *
 * DANG_END_FUNCTION
 */
unsigned int
inb(unsigned int port)
{

  static unsigned int cga_r = 0;
  static unsigned int tmp = 0;
  static unsigned int r;

  r = 0;
  port &= 0xffff;
  if (port_readable(port))
    r = (read_port(port) & 0xFF);
  else if (config.usesX) {
    v_printf("HGC Portread: %d\n", (int) port);
    switch (port) {
    case 0x03b8:		/* mode-reg */
#ifdef OLD_PORT
      set_ioperm(port, 1, 1);
      r = port_in(port);
      set_ioperm(port, 1, 0);
#else
	r = safe_port_in_byte(port);
#endif
      r = (r & 0x7f) | (hgc_Mode & 0x80);
      break;
    case 0x03ba:		/* status-reg */
#ifdef OLD_PORT
      set_ioperm(port, 1, 1);
      r = port_in(port);
      set_ioperm(port, 1, 0);
#else
	r = safe_port_in_byte(port);
#endif
      break;
    case 0x03bf:		/* conf-reg */
      set_ioperm(port, 1, 1);
      r = port_in(port);
      set_ioperm(port, 1, 0);
      r = (r & 0xfd) | (hgc_Konv & 0x02);
      break;
    case 0x03b4:		/* adr-reg */
    case 0x03b5:		/* data-reg */
#ifdef OLD_PORT
      set_ioperm(port, 1, 1);
      r = port_in(port);
      set_ioperm(port, 1, 0);
#else
	r = safe_port_in_byte(port);
#endif
      break;
    }
  }
#if 1
  else if (config.chipset && port > 0x3b3 && port < 0x3df && config.mapped_bios)
    r = (video_port_in(port));
  else if ((config.chipset == S3) && ((port & 0x03fe) == s3_8514_base) && (port & 0xfc00)) {
    iopl(3);
    r = port_in(port) & 0xff;
    iopl(0);
    v_printf("S3 inb [0x%04x] = 0x%02x\n", port, r);
  }
#endif
  else switch (port) {
  case 0x20:
  case 0x21:
    r = read_pic0(port-0x20);
    break;
  case 0xa0:
  case 0xa1:
    r = read_pic1(port-0xa0);
    break; 
  case 0x60:
    if (keys_ready) microsoft_port_check = 0;
    k_printf("direct 8042 0x60 read1: 0x%02x microsoft=%d\n", *LASTSCAN_ADD, microsoft_port_check);
    if (microsoft_port_check)
      r = microsoft_port_check;
    else
      r = *LASTSCAN_ADD;
      keys_ready = 0;
    break;

  case 0x61:
    r = read_port61();
    break;

  case 0x64:
    r = 0x1c | (keys_ready || microsoft_port_check ? 1 : 0);	/* low bit set = sc ready */
    k_printf("direct 8042 0x64 status check: 0x%02x keys_ready=%d, microsoft=%d\n", tmp, keys_ready, microsoft_port_check);
    break;

  case 0x70:
  case 0x71:
    r = cmos_read(port);
    break;

  case 0x40:
  case 0x41:
  case 0x42:
    r = pit_inp(port - 0x40);
    break;
  case 0x43:
    r = pit_control_inp();
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
    r=0x00;
    /* SERIAL PORT I/O.  The base serial port must be a multiple of 8. */
    for (tmp = 0; tmp < config.num_ser; tmp++)
      if ((port & ~7) == com[tmp].base_port) {
	r = do_serial_in(tmp, port);
	break;
      }

    /* Sound I/O */
    if ((port & SOUND_IO_MASK) == sound_base) {r=sb_read(port & SOUND_IO_MASK2);};
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
  static int timer_beep = 0;
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
      if (last_index = 0x10)
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
    write_pic0(port-0x20,byte);
    break;
  case 0x60:
    k_printf("keyboard 0x60 outb = 0x%x\n", byte);
    microsoft_port_check = 1;
    if (byte < 0xf0) {
      microsoft_port_check = 0xfe;
    }
    else {
      microsoft_port_check = 0xfa;
    }
    break;
  case 0x64:
    k_printf("keyboard 0x64 outb = 0x%x\n", byte);
    break;
  case 0x61:
    write_port61(byte);
    break;
  case 0x70:
  case 0x71:
    cmos_write(port, byte);
    break;
  case 0xa0:
  case 0xa1:
    write_pic1(port-0xa0,byte);
    break;
  case 0x40:
  case 0x41:
  case 0x42:
    pit_outp(port - 0x40, byte);
    break;
  case 0x43:
    pit_control_outp(byte);
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
    if ((port & SOUND_IO_MASK) == sound_base) {sb_write(port & SOUND_IO_MASK2, byte);};
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

/*
 * DANG_BEGIN_FUNCTION vm86_GP_fault();
 *
 * description:
 * All from the kernel unhandled general protection faults from V86 mode
 * are handled here. This are mainly port IO and the HLT instruction.
 *
 * DANG_END_FUNCTION
 */

void vm86_GP_fault(void)
{

  unsigned char *csp, *lina, org_eip;
  int pref_seg;
  int done,is_rep,is_32bit;

#if 0
  u_short *ssp = SEG_ADR((us *), ss, sp);
  static int haltcount = 0;
#endif

#define MAX_HALT_COUNT 3

#if 0
    csp = SEG_ADR((unsigned char *), cs, ip);
    k_printf("SIGSEGV cs=0x%04x ip=0x%04lx *csp=0x%02x->0x%02x 0x%02x 0x%02x\n", LWORD(cs), REG(eip), csp[-1], csp[0], csp[1], csp[2]);
/*
    show_regs(__FILE__, __LINE__);
*/
#endif

  if (ignore_segv) {
    error("ERROR: sigsegv ignored!\n");
    return;
  }

  if (in_sigsegv)
    error("ERROR: in_sigsegv=%d!\n", in_sigsegv);

  /* in_vm86 = 0; */
  in_sigsegv++;

  /* DANG_BEGIN_REMARK 
   * In a properly functioning emulator :-), sigsegv's will never come
   * while in a non-reentrant system call (ioctl, select, etc).  Therefore,
   * there's really no reason to worry about them, so I say that I'm NOT
   * in a signal handler (I might make this a little clearer later, to
   * show that the purpose of in_sighandler is to stop non-reentrant system
   * calls from being reentered.
   * I reiterate: sigsegv's should only happen when I'm running the vm86
   * system call, so I really shouldn't be in a non-reentrant system call
   * (except maybe vm86)
   * - Robert Sanders
   * DANG_END_REMARK
   */
  in_sighandler = 0;

  if (LWORD(eflags) & TF) {
    g_printf("SIGSEGV received while TF is set\n");
    show_regs(__FILE__, __LINE__);
  }

  csp = lina = SEG_ADR((unsigned char *), cs, ip);

  /* fprintf(stderr, "CSP in cpu is 0x%04x\n", *csp); */


  /* DANG_BEGIN_REMARK
   * Here we handle all prefixes prior switching to the appropriate routines
   * The exception CS:EIP will point to the first prefix that effects the
   * the faulting instruction, hence, 0x65 0x66 is same as 0x66 0x65.
   * So we collect all prefixes and remember them.
   * - Hans Lermen
   * DANG_END_REMARK
   */

  #define __SEG_ADR(type, seg, reg)  type((seg << 4) + LWORD(e##reg))
  done=0;
  is_rep=0;
  is_32bit=0;
  pref_seg=-1;

  do {
    switch (*(csp++)) {
       case 0x66:      /* operant prefix */  is_32bit=1; break;
       case 0x2e:      /* CS */              pref_seg=REG(cs); break;
       case 0x3e:      /* DS */              pref_seg=REG(ds); break;
       case 0x26:      /* ES */              pref_seg=REG(es); break;
       case 0x36:      /* SS */              pref_seg=REG(ss); break;
       case 0x65:      /* GS */              pref_seg=REG(gs); break;
       case 0x64:      /* FS */              pref_seg=REG(fs); break;
       case 0xf3:      /* rep */             is_rep=1; break;
#if 0
       case 0xf2:      /* repnz */
#endif
       default: done=1;
    }
  } while (!done);
  csp--;   
  org_eip = REG(eip);
  LWORD(eip) += (csp-lina);

  switch (*csp) {

  case 0x6c:                    /* insb */
    /* NOTE: ES can't be overwritten */
    if (is_rep) {
      int delta = 1;
      if(LWORD(eflags) &DF ) delta = -1;
      i_printf("Doing REP F3 6C (rep insb) %04x bytes, DELTA %d\n",
              LWORD(ecx),delta);
      while (LWORD(ecx))  {
        *(SEG_ADR((unsigned char *),es,di)) = inb((int) LWORD(edx));
        LWORD(edi) += delta;
        LWORD(ecx)--;
      }
    }
    else {
      *(SEG_ADR((unsigned char *),es,di)) = inb((int) LWORD(edx));
      i_printf("insb(0x%04x) value %02x\n",
              LWORD(edx),*(SEG_ADR((unsigned char *),es,di)));   
      if(LWORD(eflags) & DF) LWORD(edi)--;
      else LWORD(edi)++;
    }
    LWORD(eip)++;
    break;

  case 0x6d:			/* (rep) insw / insd */
    /* NOTE: ES can't be overwritten */
    if (is_rep) {
      #define ___LOCALAUX(typ,iotyp,incr,x) \
        int delta =incr; \
        if(LWORD(eflags) & DF) delta = -incr; \
        i_printf("rep ins%c %04x words, DELTA %d\n", x, \
                LWORD(ecx),delta); \
        while(LWORD(ecx)) { \
          *(SEG_ADR(typ,es,di))=iotyp(LWORD(edx)); \
          LWORD(edi) += delta; \
          LWORD(ecx)--; \
        }

      if (is_32bit) {
                                /* rep insd */
        ___LOCALAUX((unsigned long *),ind,4,'d')
      }
      else {
                                /* rep insw */
        ___LOCALAUX((unsigned short *),inw,2,'w')
      }
      #undef  ___LOCALAUX
    }
    else {
      #define ___LOCALAUX(typ,iotyp,incr,x) \
        *(SEG_ADR(typ,es,di)) =iotyp((int) LWORD(edx)); \
        i_printf("ins%c(0x%04x) value %04x \n", x, \
                LWORD(edx),*(SEG_ADR((unsigned short *),es,di))); \
        if(LWORD(eflags) & DF) LWORD(edi) -= incr; \
        else LWORD(edi) +=incr;
      
      if (is_32bit) {
                                /* insd */
        ___LOCALAUX((unsigned long *),ind,4,'d')
      }
      else {
                                /* insw */
        ___LOCALAUX((unsigned short *),inw,2,'w')
      }
      #undef  ___LOCALAUX
    }
    LWORD(eip)++;
    break;
      

  case 0x6e:			/* (rep) outsb */
    if (pref_seg < 0) pref_seg = LWORD(ds);
    if (is_rep) {
      int delta = 1;
      i_printf("untested: rep outsb\n");
      if(LWORD(eflags) & DF) delta = -1;
      while(LWORD(ecx)) {
        outb(LWORD(edx), *(__SEG_ADR((unsigned char *),pref_seg,si)));
        LWORD(esi) += delta;
        LWORD(ecx)--;
      }
    }
    else {
      i_printf("untested: outsb port 0x%04x value %02x\n",
              LWORD(edx),*(__SEG_ADR((unsigned char *),pref_seg,si)));
      outb(LWORD(edx), *(__SEG_ADR((unsigned char *),pref_seg,si)));
      if(LWORD(eflags) & DF) LWORD(esi)--;
      else LWORD(esi)++;
    }
    LWORD(eip)++;
    break;


  case 0x6f:			/* (rep) outsw / outsd */
    if (pref_seg < 0) pref_seg = LWORD(ds);
    if (is_rep) {
      #define ___LOCALAUX(typ,iotyp,incr,x) \
        int delta = incr; \
        i_printf("untested: rep outs%c\n", x); \
        if(LWORD(eflags) & DF) delta = -incr; \
        while(LWORD(ecx)) { \
          iotyp(LWORD(edx), *(__SEG_ADR(typ,pref_seg,si))); \
          LWORD(esi) += delta; \
          LWORD(ecx)--; \
        }
      
      if (is_32bit) {
                                /* rep outsd */
        ___LOCALAUX((unsigned long *),outd,4,'d')
      }
      else {
                                /* rep outsw */
        ___LOCALAUX((unsigned short *),outw,2,'w')
      }
      #undef  ___LOCALAUX
    }
    else {
      #define ___LOCALAUX(typ,iotyp,incr,x) \
        i_printf("untested: outs%c port 0x%04x value %04x\n", x, \
                LWORD(edx), *(__SEG_ADR(typ,pref_seg,si))); \
        iotyp(LWORD(edx), *(__SEG_ADR(typ,pref_seg,si))); \
        if(LWORD(eflags) & DF ) LWORD(esi) -= incr; \
        else LWORD(esi) +=incr;
      
      if (is_32bit) {
                                /* outsd */
        ___LOCALAUX((unsigned long *),outd,4,'d')
      }
      else {
                                /* outsw */
        ___LOCALAUX((unsigned short *),outw,2,'w')
      }
      #undef  ___LOCALAUX
    } 
    LWORD(eip)++;
    break;

  case 0xe5:			/* inw xx, ind xx */
    if (is_32bit) REG(eax) = ind((int) csp[1]);
    else LWORD(eax) = inw((int) csp[1]);
    LWORD(eip) += 2;
    break;
  case 0xe4:			/* inb xx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb((int) csp[1]);
    LWORD(eip) += 2;
    break;

  case 0xed:			/* inw dx, ind dx */
    if (is_32bit) REG(eax) = ind(LWORD(edx));
    else LWORD(eax) = inw(LWORD(edx));
    LWORD(eip) += 1;
    break;
  case 0xec:			/* inb dx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb(LWORD(edx));
    LWORD(eip) += 1;
    break;

  case 0xe7:			/* outw xx */
    if (is_32bit) outd((int)csp[1], REG(eax));
    else outw((int)csp[1], LWORD(eax));
    LWORD(eip) += 2;
    break;
  case 0xe6:			/* outb xx */
    outb((int) csp[1], LO(ax));
    LWORD(eip) += 2;
    break;

  case 0xef:			/* outw dx */
    if (is_32bit) outd(REG(edx), REG(eax));
    else outw(REG(edx), REG(eax));
    LWORD(eip) += 1;
    break;
  case 0xee:			/* outb dx */
    outb(LWORD(edx), LO(ax));
    LWORD(eip) += 1;
    break;

  case 0xf4:			/* hlt...I use it for various things,
		  like trapping direct jumps into the XMS function */
    if (lina == (unsigned char *) XMSTrap_ADD) {
      LWORD(eip) += 2;		/* skip halt and info byte to point to FAR RET */
      xms_control();
    }
    else if (lina == (unsigned char *) PIC_ADD) {
      pic_iret();
    }

    else if ((lina >=(unsigned char *)DPMI_ADD) &&
	(lina <(unsigned char *)(DPMI_ADD+(unsigned long)DPMI_dummy_end-(unsigned long)DPMI_dummy_start)))
    {
      dpmi_realmode_hlt(lina);
      break;
    }

    else {
#if 1
      error("HLT requested: lina=%p!\n", lina);
      show_regs(__FILE__, __LINE__);
#if 0
      haltcount++;
      if (haltcount > MAX_HALT_COUNT)
	fatalerr = 0xf4;
#endif
#endif
      LWORD(eip) += 1;
    }
    break;

  case 0x0f: /* RDE hack */
    if (csp[1] == 0x06)
      /*
	 CLTS - ignore
      */
      { LWORD(eip)+=2;
	break;
      }

  case 0xf0:			/* lock */
  default:
    if (is_rep) fprintf(stderr, "Nope REP F3,CSP = 0x%04x\n", csp[0]);
    /* er, why don't we advance eip here, and
	 why does it work??  */
    REG(eip) = org_eip;
#ifdef USE_MHPDBG
    mhp_debug(DBG_GPF, 0, 0);
#endif
    error("general protection at %p: %x\n", lina,*lina);
    show_regs(__FILE__, __LINE__);
    show_ints(0, 0x33);
    error("ERROR: SIGSEGV, protected insn...exiting!\n");
    fatalerr = 4;
    leavedos(fatalerr);		/* shouldn't return */
  }				/* end of switch() */

  if (LWORD(eflags) & TF) {
    g_printf("TF: trap done");
    show_regs(__FILE__, __LINE__);
  }

  in_sigsegv--;
  in_sighandler = 0;
}

#ifdef __NetBSD__
#include <machine/segments.h>

extern sigjmp_buf handlerbuf;

void dosemu_fault1(int, int, struct sigcontext *);

void
dosemu_fault(int signal, int code, struct sigcontext *scp)
{
    register unsigned short dsel;
    unsigned long retaddr;
    int jmp = 0;
    if (scp->sc_eflags & PSL_VM) {
	vm86s.substr.regs.vmsc = *scp;
	jmp = 1;
    }

    if (!in_dpmi) {
	asm("pushl %%ds; popl %0" : "=r" (dsel));
	if (dsel & SEL_LDT) {
	    error("ds in LDT!\n");
	    abort();
	}
	asm("pushl %%es; popl %0" : "=r" (dsel));
	if (dsel & SEL_LDT) {
	    error("es in LDT!\n");
	    abort();
	}
	asm("movl %%fs,%0" : "=r" (dsel) );
	if (dsel & SEL_LDT) {
	    error("fs in LDT!\n");
	    abort();
	}
	asm("movl %%gs,%0" : "=r" (dsel) );
	if (dsel & SEL_LDT) {
	    error("gs in LDT!\n");
	    abort();
	}
	asm("movl %%ss,%0" : "=r" (dsel) );
	if (dsel & SEL_LDT) {
	    error("ss in LDT!\n");
	    abort();
	}
	asm("movl %%cs,%0" : "=r" (dsel) );
	if (dsel & SEL_LDT) {
	    error("cs in LDT!\n");
	    abort();
	}
    }
    dosemu_fault1(signal, code, scp);
    if (jmp)
	siglongjmp(handlerbuf, VM86_SIGNAL | 0x80000000);
    return;
}
#endif

#ifdef __linux__
#define dosemu_fault1 dosemu_fault
#endif

/*
 * DANG_BEGIN_FUNCTION dosemu_fault(int, struct sigcontext_struct);
 *
 * All CPU exceptions (except 13=general_protection from V86 mode,
 * which is directly scaned by the kernel) are handled here.
 *
 * DANG_END_FUNCTION
 */

void 
dosemu_fault1(
#ifdef __linux__
int signal, struct sigcontext_struct context
#endif
#ifdef __NetBSD__
int signal, int code, struct sigcontext *scp
#endif
)
{
#ifdef __linux__
  struct sigcontext_struct *scp = &context;
#endif
  unsigned char *csp;
  unsigned char c;
  int i;

  if (in_vm86) {
    in_vm86 = 0;
#if 0
    write(2, "fault1 sig ", 11);
    c = signal + '0';
    write(2, &c, 1);
    write(2, " code ", 6);
    c = code + '0';
    write(2, &c, 1);
    write(2, " scp ", 5);
    for (i = 7; i >= 0; i--) {
	c = (((long)scp >> (i*4)) & 0xf);
	if (c > 9)
	    c = c + 'a' - 10;
	else
	    c = c + '0';
	write(2, &c, 1);
    }
    write(2, "\n", 1);
#endif
    switch (_trapno) {
      case 0x00: /* divide_error */
      case 0x01: /* debug */
      case 0x03: /* int3 */
      case 0x04: /* overflow */
      case 0x05: /* bounds */
      case 0x07: /* device_not_available */
		 return (void) do_int(_trapno);
      case 0x06: /* invalid_op */
		 dbug_printf("SIGILL while in vm86()\n");
#if 0
		 show_regs(__FILE__, __LINE__);
#endif
 		 csp = SEG_ADR((unsigned char *), cs, ip);
 		 /* Some db commands start with 2e (use cs segment) and thus is accounted
 		    for here */
 		 if (csp[0] == 0x2e) {
 		   csp++;
 		   LWORD(eip)++;
 		 }
 		 if (csp[0] == 0xf0) {
 		   dbug_printf("ERROR: LOCK prefix not permitted!\n");
 		   LWORD(eip)++;
 		   return;
 		 }
      default:	error("ERROR: unexpected CPU exception 0x%02lx errorcode: 0x%08lx while in vm86()\n"
	  	"eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%lx\n"
	  	"cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n", _trapno,
		_err,
	  	_eip, _esp, _eflags, _cs, _ds, _es, _ss);
#if 0
		perror("dosemu_fault:");
#endif
 		 show_regs(__FILE__, __LINE__);
		 if (d.network)		/* XXX */
		     abort();
 		 leavedos(4);
    }
  }

  if (in_dpmi)
#ifdef __linux__
    return dpmi_fault(scp);
#endif
#ifdef __NetBSD__
    return dpmi_fault(scp, code);
#endif

  csp = (char *) _eip;

  /* This has been added temporarily as most illegal sigsegv's are attempt
     to call Linux int routines */

#if 0
  if (!(csp[-2] == 0xcd && csp[-1] == 0x80 && csp[0] == 0x85)) {
#else
  {
#endif
    error("ERROR: cpu exception in dosemu code outside of VM86()!\n"
	  "trapno: 0x%02lx  errorcode: 0x%08lx  cr2: 0x%08lx\n"
	  "eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n"
	  "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n",
	  _trapno, _err, _cr2,
	  _eip, _esp, _eflags, _cs, _ds, _es, _ss);

    dbug_printf("  VFLAGS(b): ");
    for (i = (1 << 17); i; i >>= 1)
      dbug_printf((_eflags & i) ? "1" : "0");
    dbug_printf("\n");

    dbug_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx"
		"  VFLAGS(h): %08lx\n",
		_eax, _ebx, _ecx, _edx, _eflags);
    dbug_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n",
		_esi, _edi, _ebp);
    dbug_printf("CS: %04x  DS: %04x  ES: %04x  FS: %04x  GS: %04x\n",
		_cs, _ds, _es, _fs, _gs);

    /* display vflags symbolically...the #f "stringizes" the macro name */
#undef PFLAG
#define PFLAG(f)  if ((_eflags)&(f)) dbug_printf(" " #f)

    dbug_printf("FLAGS:");
    PFLAG(CF);
    PFLAG(PF);
    PFLAG(AF);
    PFLAG(ZF);
    PFLAG(SF);
    PFLAG(TF);
    PFLAG(IF);
    PFLAG(DF);
    PFLAG(OF);
    PFLAG(NT);
    PFLAG(RF);
    PFLAG(VM);
    PFLAG(AC);
    dbug_printf("  IOPL: %u\n", (unsigned) ((_eflags & IOPL_MASK) >> 12));

    /* display the 10 bytes before and after CS:EIP.  the -> points
     * to the byte at address CS:EIP
     */
    dbug_printf("OPS  : ");
    csp = (unsigned char *) _eip - 10;
    for (i = 0; i < 10; i++)
      dbug_printf("%02x ", *csp++);
    dbug_printf("-> ");
    for (i = 0; i < 10; i++)
      dbug_printf("%02x ", *csp++);
    dbug_printf("\n");

    show_regs(__FILE__, __LINE__);

    fatalerr = 4;
    leavedos(fatalerr);		/* shouldn't return */
  }
}
