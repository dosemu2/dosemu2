/* 
 * $Date: 1995/02/26 00:54:47 $
 * $Source: /home/src/dosemu0.60/dosemu/RCS/sigsegv.c,v $
 * $Revision: 2.19 $
 * $State: Exp $
 *
 * $Log: sigsegv.c,v $
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


static char rcsid[]="$Id: sigsegv.c,v 2.19 1995/02/26 00:54:47 root Exp root $";

#include <stdio.h>
#include <stdlib.h>


#include <unistd.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/times.h>

#include "emu.h"
#include "bios.h"
#include "mouse.h"
#include "serial.h"
#include "xms.h"
#include "timers.h"
#include "cmos.h"
#include "memory.h"
#include "cpu.h"
#include "termio.h"
#include "config.h"
#include "port.h"
#include "int.h"
#include "hgc.h"
#include "dosio.h"

#include "video.h"

#ifdef NEW_PIC
#include "../timer/pic.h"
#endif

#ifdef DPMI
#include "../dpmi/dpmi.h"
#endif

#ifdef USING_NET
#include "ipx.h"
#endif

/* Needed for DIAMOND define */
#include "vc.h"


/* PORT_DEBUG is to specify whether to record port writes to debug output.
 * 0 means disabled.
 * 1 means record all port accesses to 0x00 to 0xFF
 * 2 means record ANY port accesses!  (big fat debugfile!)
 */ 
#define PORT_DEBUG 0

/* int port61 = 0xd0;           the pseudo-8255 device on AT's */
static int port61 = 0x0e;		/* the pseudo-8255 device on AT's */
extern void set_leds(void);
/* FIXME -- move to common header */
extern int s3_8514_base;
static u_short microsoft_port_check = 0;

/* 
 * Copied adverbatim from Mach code, please be aware of their 
 * copyright in mfs.c
 */
static int timer_interrupt_rate = 55;
static int min_timer_milli_pause = 16;
static int timer_milli_pause = 55;

static int timer0_read_latch_value = 0;
static int timer0_read_latch_value_orig = 0;
static int timer0_latched_value = 0;
static int timer0_new_value = 0;
static boolean_t latched_counter_msb = FALSE;

void update_timers(void);
/* ??? */
static u_long milliseconds_since_boot;

#if 1
static int do_40(in_out, val)
	boolean_t in_out;
	int val;
{
	int ret;

	if (in_out) {  /* true => in, false => out */

		update_timers();
		if (timer0_latched_value == 0) {
			timer0_latched_value = timer_interrupt_rate*850;
		}
		switch (timer0_read_latch_value) {
			case 0x30:
				ret = timer0_latched_value&0xf;
				timer0_read_latch_value = 0x20;
				break;
			case 0x10:
				ret = timer0_latched_value&0xf;
				timer0_read_latch_value = 0x0;
				break;
			case 0x20:
				ret = (timer0_latched_value&0xf0)>>8;
				timer0_read_latch_value = 0x0;
				timer0_latched_value = 0;
				break;
			case 0x0:
				if (latched_counter_msb) {
					ret = 
					(milliseconds_since_boot >> 8) & 0xff;
				} else {
					ret = milliseconds_since_boot & 0xff;
				}
				latched_counter_msb = !latched_counter_msb;
				break;

		}
		i_printf("PORT: do_40, in = 0x%x\n",ret);
	} else {
		switch (timer0_read_latch_value) {
			case 0x0:
			case 0x30:
				timer0_new_value = val;
				timer0_read_latch_value = 0x20;
				break;
			case 0x10:
				timer0_new_value = val;
				timer0_read_latch_value = 0x0;
				break;
			case 0x20:
				timer0_new_value |= (val<<8);
				timer0_read_latch_value = 0x0;
				break;
		}
		i_printf("PORT: do_40, out = 0x%x\n",val);
		if (timer0_read_latch_value == 0) {
			timer0_read_latch_value=timer0_read_latch_value_orig;
			if (timer0_new_value == 0) timer0_new_value = 64*1024;
			timer_interrupt_rate = ((timer0_new_value)/1193);
			if (timer_interrupt_rate < min_timer_milli_pause) {
				int i;
				if (timer_interrupt_rate == 0) 
					timer_interrupt_rate = 1;
				for (i = 1; i<=min_timer_milli_pause;i++) {
				    if (i*timer_interrupt_rate >= 
						min_timer_milli_pause){
				    	timer_milli_pause = i*
						timer_interrupt_rate;
					break;
				    }
				}
			} else {
				timer_milli_pause = timer_interrupt_rate;
			}
			i_printf("timer_interrupt_rate = %d\n",
					  timer_interrupt_rate);
			i_printf("timer_milli_pause = %d\n",
					  timer_milli_pause);
		}
	}
	return(ret);
}

#endif
#if 0
/* return value of port -- what happens when output?  Stack trash? */
static int do_42(in_out, val)
	boolean_t in_out;
	int val;
{
	int ret;
	if (in_out) {  /* true => in, false => out */
		ioperm(0x42,1,1);
		ret = port_in(0x42);
		ioperm(0x42,1,0);
		i_printf( "PORT: do_42, in = 0x%x\n",ret);
	} else {
		ioperm(0x42,1,1);
		port_out(val, 0x42);
		ioperm(0x42,1,0);
		i_printf( "PORT: do_42, out = 0x%x\n",val);
	}
	return(ret);
}

#endif

static int do_43(boolean_t in_out, int val)
{
	int ret;
	
	if (in_out) {  /* true => in, false => out */
		i_printf( "PORT: do_43, in = 0x%x\n",ret);
	} else {
		i_printf( "PORT: do_43, out = 0x%x\n",val);
		switch(val>>6) {
			case 0:
				timer0_read_latch_value = (val&0x30);
				timer0_read_latch_value_orig = (val&0x30);
				if (timer0_read_latch_value == 0) {
					latched_counter_msb = FALSE;
				}
				break;
			case 2:
#if 0
				ioperm(0x43,1,1);
				port_out(val, 0x43);
				ioperm(0x43,1,0);
#else
				safe_port_out_byte(0x43, val);
#endif
				i_printf( "PORT: Really do_43\n");
				break;
		}
	}
	return(ret);
}

/*
 * DANG_BEGIN_FUNCTION inb(int port)
 *
 * description:
 *  INB is used to do controlled emulation of input from ports.
 *
 * DANG_END_FUNCTION
 */
int
inb(int port)
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
#ifdef NEW_PIC
  case 0x20:
  case 0x21:
    r = read_pic0(port-0x20);
    break;
  case 0xa0:
  case 0xa1:
    r = read_pic1(port-0xa0);
    break; 
#endif
  case 0x60:
    if (keys_ready) microsoft_port_check = 0;
    k_printf("direct 8042 read1: 0x%02x microsoft=%d\n", *LASTSCAN_ADD, microsoft_port_check);
    if (microsoft_port_check)
      r = microsoft_port_check;
    else
      r = *LASTSCAN_ADD;
      keys_ready = 0;
    break;

  case 0x61:
    k_printf("inb [0x61] = 0x%02x (8255 chip)\n", port61);
    r = port61;
    break;

  case 0x64:
    r = 0x1c | (keys_ready || microsoft_port_check ? 1 : 0);	/* low bit set = sc ready */
    k_printf("direct 8042 0x64 status check: 0x%02x keys_ready=%d, microsoft=%d\n", tmp, keys_ready, microsoft_port_check);
    break;

  case 0x70:
  case 0x71:
    r = cmos_read(port);
    break;

#define COUNTER 2
#if 0 /*Nov-21-94*/
  case 0x40:
    r = do_40(1, 0);
    break;
  case 0x41:
    r = do_40(1, 0);
    break;
#else
  case 0x40:
    pit.CNTR0 -= COUNTER;
    i_printf("inb [0x40] = 0x%02x  1st timer inb\n", pit.CNTR0);
    r = pit.CNTR0;
    break;
  case 0x41:
    pit.CNTR1 -= COUNTER;
    i_printf("inb [0x41] = 0x%02x  2nd timer inb\n", pit.CNTR1);
    r = pit.CNTR1;
    break;
#endif
  case 0x42:
#if 0
    r = do_42(1, 0);
    break;
#else
    pit.CNTR2 -= COUNTER;
    i_printf("inb [0x42] = 0x%02x  3rd timer inb\n", pit.CNTR2);
    r = pit.CNTR2;
    break;
#endif
  case 0x43:
    r = safe_port_in_byte(0x43);
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
    r=0;
    /* SERIAL PORT I/O.  The base serial port must be a multiple of 8. */
    for (tmp = 0; tmp < config.num_ser; tmp++)
      if ((port & ~7) == com[tmp].base_port) {
	r = do_serial_in(tmp, port);
	break;
      }

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
    fprintf(stderr,"PORT: Rd 0x%04x -> 0x%02x\n",port,r);
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

void
outb(int port, int byte)
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
    fprintf(stderr,"PORT: Wr 0x%04x <- 0x%02x\n",port,byte);
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
#ifdef NEW_PIC
    write_pic0(port-0x20,byte);
#else /* NEW_PIC */
    k_printf("OUTB 0x%x to byte=%x\n", port, byte);
#if 0				/* 94/04/30 */
    REG(eflags) |= VIF;
#endif
#if 1				/* 94/05/11 */
    *OUTB_ADD = 1;
#endif
    if (port == 0x20 && byte != 0x20)
      set_leds();
#endif  /* NEW_PIC */
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
    port61 = byte & 0x0f;
    k_printf("8255 0x61 outb = 0x%x\n", byte);
    if (((byte & 3) == 3) 
#if 1
    && (timer_beep == 1) 
#endif
    && (config.speaker == SPKR_EMULATED)) {
      i_printf("beep!\n");
      putchar('\007');
      timer_beep = 0;
    }
    else {
      timer_beep = 1;
    }
    break;
  case 0x70:
  case 0x71:
    cmos_write(port, byte);
    break;
#ifdef NEW_PIC
  case 0xa0:
  case 0xa1:
    write_pic1(port-0xa0,byte);
    break;
#endif
  case 0x40:
	do_40(0, byte);
	break;
  case 0x41:
	do_40(0, byte);
	break;
  case 0x43:
#if 1
        do_43(0, byte);
        break;
#else
        safe_port_out_byte(port,  byte);
	break;
#endif
  case 0x42:
	safe_port_out_byte(port, byte);
    if ((port == 0x42) && (lastport == 0x42)) {
      if ((timer_beep == 1) &&
	  (config.speaker == SPKR_EMULATED)) {
	putchar('\007');
	timer_beep = 0;
      }
      else {
	timer_beep = 1;
      }
    }
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
    i_printf("outb [0x%x] 0x%02x\n", port, byte);
  }
  lastport = port;
}

void
outw(int port, int value)
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



/* time of boot */
static struct timeval initial_time;

void initialize_timers(void)
{

	gettimeofday(&initial_time, NULL);
}

	
/* not sure what adder does -- get time in relation to inital time and
 * convert to milliseocnds
 */
static void update_timers(void) {
  static unsigned long adder=0;
  struct timeval tp;

  gettimeofday(&tp, NULL);

  milliseconds_since_boot = (tp.tv_sec - initial_time.tv_sec)*1000;
  milliseconds_since_boot += (tp.tv_usec - initial_time.tv_usec)/1000;
  milliseconds_since_boot += adder;

  adder = (adder + 1) % 20;
  i_printf("Timers updated: %x\n", milliseconds_since_boot);

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

  unsigned char *csp, *lina;

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

  switch (*csp) {

  case 0x6c:                    /* insb */
    *(SEG_ADR((unsigned char *),es,di)) = inb((int) LWORD(edx));
    i_printf("insb(0x%04x) value %02x\n",
            LWORD(edx),*(SEG_ADR((unsigned char *),es,di)));   
    if(LWORD(eflags) & DF) LWORD(edi)--;
    else LWORD(edi)++;
    LWORD(eip)++;
    break;

  case 0x6d:			/* insw */
    *(SEG_ADR((unsigned short *),es,di)) =inw((int) LWORD(edx));
    i_printf("insw(0x%04x) value %04x \n",
            LWORD(edx),*(SEG_ADR((unsigned short *),es,di)));
    if(LWORD(eflags) & DF) LWORD(edi) -= 2;
    else LWORD(edi) +=2;
    LWORD(eip)++;
    break;
      

  case 0x6e:			/* outsb */
    fprintf(stderr,"untested: outsb port 0x%04x value %02x\n",
            LWORD(edx),*(SEG_ADR((unsigned char *),ds,si)));
    outb(LWORD(edx), *(SEG_ADR((unsigned char *),ds,si)));
    if(LWORD(eflags) & DF) LWORD(esi)--;
    else LWORD(esi)++;
    LWORD(eip)++;
    break;


  case 0x6f:			/* outsw */
    fprintf(stderr,"untested: outsw port 0x%04x value %04x\n",
            LWORD(edx), *(SEG_ADR((unsigned short *),ds,si)));
    outw(LWORD(edx), *(SEG_ADR((unsigned short *),ds,si)));
    if(LWORD(eflags) & DF ) LWORD(esi) -= 2;
    else LWORD(esi) +=2; 
    LWORD(eip)++;
    break;

  case 0xe5:			/* inw xx */
    LWORD(eax) = inw((int) csp[1]);
    LWORD(eip) += 2;
    break;
  case 0xe4:			/* inb xx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb((int) csp[1]);
    LWORD(eip) += 2;
    break;

  case 0xed:			/* inw dx */
    LWORD(eax) = inw(LWORD(edx));
    LWORD(eip) += 1;
    break;
  case 0xec:			/* inb dx */
    LWORD(eax) &= ~0xff;
    LWORD(eax) |= inb(LWORD(edx));
    LWORD(eip) += 1;
    break;

  case 0xe7:			/* outw xx */
    outb((int) csp[1], LO(ax));
    outb((int) csp[1] + 1, HI(ax));
    LWORD(eip) += 2;
    break;
  case 0xe6:			/* outb xx */
    outb((int) csp[1], LO(ax));
    LWORD(eip) += 2;
    break;

  case 0xef:			/* outw dx */
    outb(REG(edx), LO(ax));
    outb(REG(edx) + 1, HI(ax));
    LWORD(eip) += 1;
    break;
  case 0xee:			/* outb dx */
    outb(LWORD(edx), LO(ax));
    LWORD(eip) += 1;
    break;

  case 0xf3:                    /* rep */
    switch(csp[1] & 0xff) {                
      case 0x6c: {             /* rep insb */
        int delta = 1;
        if(LWORD(eflags) &DF ) delta = -1;
        i_printf("Doing REP F3 6C (rep insb) %04x bytes, DELTA %d\n",
                LWORD(ecx),delta);
        while (LWORD(ecx))  {
          *(SEG_ADR((unsigned char *),es,di)) = inb((int) LWORD(edx));
          LWORD(edi) += delta;
          LWORD(ecx)--;
        }
        LWORD(eip)+=2;
        break;
      }
      case  0x6d: {           /* rep insw */
        int delta =2;
        if(LWORD(eflags) & DF) delta = -2;
        i_printf("REP F3 6D (rep insw) %04x words, DELTA %d\n",
                LWORD(ecx),delta);
        while(LWORD(ecx)) {
          *(SEG_ADR((unsigned short *),es,di))=inw(LWORD(edx));
          LWORD(edi) += delta;
          LWORD(ecx)--;
         }
         LWORD(eip) +=2;
         break;
      }
      case 0x6e: {           /* rep outsb */
        int delta = 1;
        fprintf(stderr,"untested: rep outsb\n");
        if(LWORD(eflags) & DF) delta = -1;
        while(LWORD(ecx)) {
          outb(LWORD(edx), *(SEG_ADR((unsigned char *),ds,si)));
          LWORD(esi) += delta;
          LWORD(ecx)--;
        }
        LWORD(eip)+=2;
        break;
      }
      case 0x6f: { 
        int delta = 2;
        fprintf(stderr,"untested: rep outsw\n");
        if(LWORD(eflags) & DF) delta = -2;
        while(LWORD(ecx)) {
          outw(LWORD(edx), *(SEG_ADR((unsigned short *),ds,si)));
          LWORD(esi) += delta;
          LWORD(ecx)--;
        }
        LWORD(eip)+=2;
        break;
      }
      default:
      fprintf(stderr, "Nope REP F3,CSP[1] = 0x%04x\n", csp[1]);
    }
    break;        
  case 0xf4:			/* hlt...I use it for various things,
		  like trapping direct jumps into the XMS function */
    if (lina == (unsigned char *) XMSTrap_ADD) {
      LWORD(eip) += 2;		/* skip halt and info byte to point to FAR RET */
      xms_control();
    }
#ifdef DPMI
    else if ((lina >=(unsigned char *)DPMI_ADD) &&
	(lina <(unsigned char *)(DPMI_ADD+(unsigned long)DPMI_dummy_end-(unsigned long)DPMI_dummy_start)))
    {
      dpmi_realmode_hlt(lina);
      break;
    }
#endif /* DPMI */
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

  case 0xf0:			/* lock */
  default:
    /* er, why don't we advance eip here, and
	 why does it work??  */
    error("general protection at %p: %x\n", csp,*csp);
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

/*
 * DANG_BEGIN_FUNCTION dosemu_fault(int, struct sigcontext_struct);
 *
 * All CPU exceptions (except 13=general_protection from V86 mode,
 * which is directly scaned by the kernel) are handled here.
 *
 * DANG_END_FUNCTION
 */

void 
dosemu_fault(int signal, struct sigcontext_struct context)
{
  struct sigcontext_struct *scp = &context;
  unsigned char *csp;
  int i;

  if (in_vm86) {
    in_vm86 = 0;
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
		 show_regs(__FILE__, __LINE__);
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
	  	"cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x\n", _trapno,scp->err,
	  	_eip, _esp, _eflags, _cs, _ds, _es, _ss);
		perror("YUCK");
 		 show_regs(__FILE__, __LINE__);
 		 leavedos(4);
    }
  }

#ifdef DPMI
  if (in_dpmi)
    return dpmi_fault(&context);
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
	  _trapno, scp->err, scp->cr2,
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
