#define PORTS_H 1

/*
 * $Date: 1994/10/14 17:58:38 $
 * $Source: /home/src/dosemu0.60/RCS/ports.h,v $
 * $Revision: 2.14 $
 * $State: Exp $
 *
 * $Log: ports.h,v $
 * Revision 2.14  1994/10/14  17:58:38  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.13  1994/09/26  23:10:13  root
 * Prep for pre53_22.
 *
 * Revision 2.12  1994/09/23  01:29:36  root
 * Prep for pre53_21.
 *
 * Revision 2.11  1994/09/11  01:01:23  root
 * Prep for pre53_19.
 *
 * Revision 2.10  1994/08/14  02:52:04  root
 * Rain's latest CLEANUP and MOUSE for X additions.
 *
 * Revision 2.9  1994/08/02  00:08:51  root
 * Markk's latest.
 *
 * Revision 2.8  1994/08/01  14:26:23  root
 * Prep for pre53_7  with Markks latest, EMS patch, and Makefile changes.
 *
 * Revision 2.7  1994/07/09  14:29:43  root
 * prep for pre53_3.
 *
 * Revision 2.6  1994/07/04  23:59:23  root
 * Prep for Markkk's NCURSES patches.
 *
 * Revision 2.5  1994/06/27  02:15:58  root
 * Prep for pre53
 *
 * Revision 2.4  1994/06/24  14:51:06  root
 * Markks's patches plus.
 *
 * Revision 2.3  1994/06/14  21:34:25  root
 * Second series of termcap patches.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.15  1994/06/05  21:17:35  root
 * Prep for pre51_24.
 *
 * Revision 1.14  1994/05/30  00:08:20  root
 * Prep for pre51_22 and temp kludge fix for dir a: error.
 *
 * Revision 1.13  1994/05/26  23:15:01  root
 * Prep. for pre51_21.
 *
 * Revision 1.12  1994/05/24  01:23:00  root
 * Lutz's latest, int_queue_run() update.
 *
 * Revision 1.11  1994/05/21  23:39:19  root
 * PRE51_19.TGZ with Lutz's latest updates.
 *
 * Revision 1.10  1994/05/18  00:15:51  root
 * pre15_17.
 *
 * Revision 1.9  1994/05/13  17:21:00  root
 * pre51_15.
 *
 * Revision 1.8  1994/05/10  23:08:10  root
 * pre51_14.
 *
 * Revision 1.7  1994/04/30  22:12:30  root
 * Prep for pre51_11.
 *
 * Revision 1.6  1994/04/27  23:39:57  root
 * Lutz's patches to get dosemu up under 1.1.9.
 *
 * Revision 1.5  1994/04/13  00:07:09  root
 * Lutz's patches
 *
 * Revision 1.4  1994/04/07  20:50:59  root
 * More updates.
 *
 * Revision 2.9  1994/04/06  00:56:56  root
 * Made serial config more flexible, and up to 4 ports.
 *
 * Revision 1.2  1994/04/04  22:51:55  root
 * Patches for PS/2 mouse.
 *
 * Revision 1.1  1994/03/24  00:47:10  root
 * Initial revision
 *
 * Revision 1.1  1994/03/23  23:45:12  root
 * Initial revision
 *
 */

/*
   Allow checks via inport 0x64 for available scan codes
*/
extern u_char keys_ready;

/* PORT_DEBUG is to specify whether to record port writes to debug output.
 * 0 means disabled.
 * 1 means record all port accesses to 0x00 to 0xFF
 * 2 means record ANY port accesses!  (big fat debugfile!)
 */ 
#define PORT_DEBUG 0

/* int port61 = 0xd0;           the pseudo-8255 device on AT's */
int port61 = 0x0e;		/* the pseudo-8255 device on AT's */
extern int fatalerr;
struct pit pit;
extern void set_leds(void);
extern int s3_8514_base;
extern int cursor_row;
extern int cursor_col;
extern int char_blink;
u_short microsoft_port_check = 0;

/* 
 * Copied adverbatim from Mach code, please be aware of their 
 * copyright in mfs.c
 */
int timer_interrupt_rate = 55;
int min_timer_milli_pause = 16;
int timer_milli_pause = 55;

int timer0_read_latch_value = 0;
int timer0_read_latch_value_orig = 0;
int timer0_latched_value = 0;
int timer0_new_value = 0;
boolean_t latched_counter_msb = FALSE;

extern void update_timers(void);
u_long milliseconds_since_boot;

int do_40(in_out, val)
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

int do_42(in_out, val)
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

int do_43(in_out, val)
	boolean_t in_out;
	int val;
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
				ioperm(0x43,1,1);
				port_out(val, 0x43);
				ioperm(0x43,1,0);
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
inline int
inb(int port)
{

  static unsigned int cga_r = 0;
  static unsigned int tmp = 0;
  static unsigned int r = 0;

#ifdef PORT_DEBUG
#if PORT_DEBUG == 1
  if (port < 0x100)
#endif
    fprintf(stderr,"PORT: Rd 0x%04x\n",port);
#endif

  port &= 0xffff;
  if (port_readable(port))
    return (read_port(port) & 0xFF);
 
  if (config.usesX) {
    v_printf("HGC Portread: %d\n", (int) port);
    switch (port) {
    case 0x03b8:		/* mode-reg */
      set_ioperm(port, 1, 1);
      r = port_in(port);
      set_ioperm(port, 1, 0);
      r = (r & 0x7f) | (hgc_Mode & 0x80);
      break;
    case 0x03ba:		/* status-reg */
      set_ioperm(port, 1, 1);
      r = port_in(port);
      set_ioperm(port, 1, 0);
      break;
    case 0x03bf:		/* conf-reg */
      set_ioperm(port, 1, 1);
      r = port_in(port);
      set_ioperm(port, 1, 0);
      r = (r & 0xfd) | (hgc_Konv & 0x02);
      break;
    case 0x03b4:		/* adr-reg */
      set_ioperm(port, 1, 1);
      r = port_in(port);
      set_ioperm(port, 1, 0);
      break;
    case 0x03b5:		/* data-reg */
      set_ioperm(port, 1, 1);
      r = port_in(port);
      set_ioperm(port, 1, 0);
      break;
    }
    return r;
  }

#if 1
  if (config.chipset && port > 0x3b3 && port < 0x3df && config.mapped_bios)
    return (video_port_in(port));
  if ((config.chipset == S3) && ((port & 0x03fe) == s3_8514_base) && (port & 0xfc00)) {
    int _v;

    iopl(3);
    _v = port_in(port) & 0xff;
    iopl(0);
    v_printf("S3 inb [0x%04x] = 0x%02x\n", port, _v);
    return _v;
  }
#endif

  switch (port) {
  case 0x60:
    if (keys_ready)
      microsoft_port_check = 0;
    k_printf("direct 8042 read1: 0x%02x microsoft=%d\n", *LASTSCAN_ADD, microsoft_port_check);
    if (microsoft_port_check)
      return microsoft_port_check;
    else
      return *LASTSCAN_ADD;

  case 0x61:
    k_printf("inb [0x61] =  0x%02x (8255 chip)\n", port61);
    return port61;

  case 0x64:
    tmp = 0x1c | (keys_ready || microsoft_port_check ? 1 : 0);	/* low bit set = sc ready */
    k_printf("direct 8042 0x64 status check: 0x%02x keys_ready=%d, microsoft=%d\n", tmp, keys_ready, microsoft_port_check);
    return tmp;

  case 0x70:
  case 0x71:
    return cmos_read(port);

#define COUNTER 2
  case 0x40:
	return(do_40(1, 0));
    pit.CNTR0 -= COUNTER;
    i_printf("inb [0x40] = 0x%02x  1st timer inb\n",
	     pit.CNTR0);
    return pit.CNTR0;
  case 0x41:
	return(do_40(1, 0));
    pit.CNTR1 -= COUNTER;
    i_printf("inb [0x41] = 0x%02x  2nd timer inb\n",
	     pit.CNTR1);
    return pit.CNTR1;
  case 0x42:
	return(do_42(1, 0));
    pit.CNTR2 -= COUNTER;
    i_printf("inb [0x42] = 0x%02x  3rd timer inb\n",
	     pit.CNTR2);
    return pit.CNTR2;
  case 0x43:
	return(do_43(1, 0));

  case 0x3ba:
  case 0x3da:
    /* graphic status - many programs will use this port to sync with
     * the vert & horz retrace so as not to cause CGA snow */
    i_printf("3ba/3da port inb\n");
    return (cga_r ^= 1) ? 0xcf : 0xc6;
  case 0x3bc:
    i_printf("printer port inb [0x3bc] = 0\n");
    return 0;
  case 0x3db:			/* light pen strobe reset */
    return 0;

  default:
    /* SERIAL PORT I/O.  The base serial port must be a multiple of 8. */
    for (tmp = 0; tmp < config.num_ser; tmp++)
      if ((port & ~7) == com[tmp].base_port)
	return (do_serial_in(tmp, port));

    /* The diamond bug */
    if (config.chipset == DIAMOND && (port >= 0x23c0) && (port <= 0x23cf)) {
      iopl(3);
      tmp = port_in(port);
      iopl(0);
      i_printf(" Diamond inb [0x%x] = 0x%x\n", port, tmp);
      return (tmp);
    }
    i_printf("default inb [0x%x] = 0x%02x\n", port,0x00);
    return 0x00;
  }
  return 0;
}

inline int
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
  return ((inb(port + 1) << 8) + inb(port));
}

inline void
outb(int port, int byte)
{
  static int timer_beep = 0;
  static int lastport = 0;
  static unsigned int tmp = 0;

  port &= 0xffff;
  byte &= 0xff;

#ifdef PORT_DEBUG
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
  if ((port & 0xfffe) == bios_video_port) {
    /* Writing to the 6845 */
    static int last_port;
    static int last_byte;
    static int hi = 0, lo = 0;
    int pos;

    v_printf("Video Port outb [0x%04x]\n", port);
    if (!config.usesX) {
      if ((port == bios_video_port + 1) && (last_port == bios_video_port)) {
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
    k_printf("OUTB 0x%x to byte=%x\n", port, byte);
#if 0				/* 94/04/30 */
    REG(eflags) |= VIF;
#endif
#if 1				/* 94/05/11 */
    *OUTB_ADD = 1;
#endif
    if (port == 0x20 && byte != 0x20)
      set_leds();
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
    if (((byte & 3) == 3) && (timer_beep == 1) &&
	(config.speaker == SPKR_EMULATED)) {
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
  case 0x40:
	do_40(0, byte);
	break;
  case 0x41:
	do_40(0, byte);
	break;
  case 0x42:
	do_42(0, byte);
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
  case 0x43:
#if 0
    i_printf("timer outb 0x%02x\n", byte);
#endif
	do_43(0, byte);
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

inline void
outw(int port, int value)
{
  if ((config.chipset == S3) && ((port & 0x03ff) == s3_8514_base) && (port & 0xfc00)) {
    iopl(3);
    port_out_w(value, port);
    iopl(0);
    v_printf("S3 outw [0x%04x] = 0x%04x\n", port, value);
    return;
  }
  outb(port, value & 0xff);
  outb(port + 1, value >> 8);
}

void update_timers(void) {
  static long initial_sec=0L;
  static long initial_usec;
  static struct timeval tp;
  static unsigned long adder=0;

  gettimeofday(&tp, NULL);

  if (initial_sec == 0L) {
    initial_sec = tp.tv_sec;
    initial_usec = tp.tv_usec;
  }

  milliseconds_since_boot = ((tp.tv_sec - initial_sec)*1000) +
                            ((tp.tv_usec - initial_usec)/1000) + adder;
  adder = (adder + 1) % 20;
  i_printf("Timers updated: %x\n", milliseconds_since_boot);

}
 
#undef PORTS_H
