#define INT_H 1

#include "ipx.h"

/* 
 * $Date: 1994/06/13 20:52:43 $
 * $Source: /home/src/dosemu0.52/RCS/int.h,v $
 * $Revision: 2.2 $
 * $State: Exp $
 *
 * $Log: int.h,v $
 * Revision 2.2  1994/06/13  20:52:43  root
 * Markkk's fix for termcap.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.26  1994/05/30  00:08:20  root
 * Prep for pre51_22 and temp kludge fix for dir a: error.
 *
 * Revision 1.25  1994/05/26  23:15:01  root
 * Prep. for pre51_21.
 *
 * Revision 1.24  1994/05/21  23:39:19  root
 * PRE51_19.TGZ with Lutz's latest updates.
 *
 * Revision 1.23  1994/05/18  00:15:51  root
 * pre15_17.
 *
 * Revision 1.22  1994/05/16  23:13:23  root
 * Prep for pre51_16.
 *
 * Revision 1.21  1994/05/13  17:21:00  root
 * pre51_15.
 *
 * Revision 1.20  1994/05/10  23:08:10  root
 * pre51_14.
 *
 * Revision 1.19  1994/05/09  23:35:11  root
 * pre51_13.
 *
 * Revision 1.18  1994/05/05  00:16:26  root
 * Prep for pre51_12.
 *
 * Revision 1.17  1994/05/04  22:16:00  root
 * Patches by Alan to mouse subsystem.
 *
 * Revision 1.16  1994/05/04  21:56:55  root
 * Prior to Alan's mouse patches.
 *
 * Revision 1.15  1994/04/30  22:12:30  root
 * Prep for pre51_11.
 *
 * Revision 1.14  1994/04/30  01:05:16  root
 * Clean Up
 *
 * Revision 1.13  1994/04/27  23:52:28  root
 * Drop locking.
 *
 * Revision 1.12  1994/04/27  23:42:18  root
 * Jochen's + Lutz's.
 *
 * Revision 1.10  1994/04/23  20:51:40  root
 * Get new stack over/underflow working in VM86 mode.
 *
 * Revision 1.9  1994/04/20  23:43:35  root
 * pre51_8 out the door.
 *
 * Revision 1.8  1994/04/20  21:05:01  root
 * Prep for Rob's patches to linpkt...
 *
 * Revision 1.7  1994/04/16  01:28:47  root
 * Prep for pre51_6.
 *
 * Revision 1.6  1994/04/13  00:07:09  root
 * Lutz's patches
 *
 * Revision 1.5  1994/04/09  18:41:52  root
 * Prior to Lutz's kernel enhancements.
 *
 * Revision 1.4  1994/04/07  00:18:41  root
 * Pack up for pre52_4.
 *
 * Revision 1.3  1994/04/04  22:51:55  root
 * Patches for PS/2 mouse.
 *
 * Revision 1.2  1994/03/30  22:12:30  root
 * Prep for 0.51 pre 2.
 *
 * Revision 1.1  1994/03/24  00:47:25  root
 * Initial revision
 *
 * Revision 1.1  1994/03/23  23:23:27  root
 * Initial revision
 *
 */

extern int pkt_int(void);
extern void pkt_init(int);
extern int pkt_check_receive(void);
extern void restore_screen(void);

extern int fatalerr;

#define IQUEUE_LEN 1000
struct int_queue_struct {
  int interrupt;
  int (*callstart) ();
  int (*callend) ();
} int_queue[IQUEUE_LEN];

#define NUM_INT_QUEUE 64
struct int_queue_list_struct {
  struct int_queue_struct int_queue_ptr;
  int int_queue_return_addr;
  u_char in_use;
  struct vm86_regs saved_regs;
} int_queue_head[NUM_INT_QUEUE];

/*
   This is here to allow multiple hard_int's to be running concurrently.
   Needed for programs that steal INT9 away from DOSEMU.
*/
#define NUM_INT_QUEUE 64
extern struct int_queue_list_struct int_queue_head[NUM_INT_QUEUE]; 
extern int int_queue_running;

extern inline void int_queue_run(void);

/* this holds all the configuration information, set in config_init() */
extern unsigned int configuration;

int dos_helper(void);           /* handle int's created especially for DOSEMU */

/* Function to set up all memory area for DOS, as well as load boot block */
extern void boot();

extern long start_time;                /* Keep track of times for DOS calls */
extern unsigned long last_ticks;
 
/* Used to keep track of when xms calls are made */
extern int xms_grab_int15;

/* Time structures for translating UNIX <-> DOS times */
extern struct timeval scr_tv;
extern struct itimerval itv;
 
/* Once every 18 let's update leds */
u_char leds = 0;

/* This is not used at this time */
inline void
int08(void)
{
  run_int(0x1c);
  REG(eflags) |= VIF;
  return;
}

inline void
int15(void)
{
  struct timeval wait_time;
  int num;

  if (HI(ax) != 0x4f)
    NOCARRY;

  switch (HI(ax)) {
  case 0x10:			/* TopView/DESQview */
    switch (LO(ax))
    {
    case 0x00:			/* giveup timeslice */
      usleep(INT15_IDLE_USECS);
      return;
    }
    CARRY;
    break;
  case 0x41:			/* wait on external event */
    break;
  case 0x4f:			/* Keyboard intercept */
    HI(ax) = 0x86;
    k_printf("INT15 0x4f CARRY=%x AX=%x\n", (LWORD(eflags) & CF),LWORD(eax));
    CARRY;
/*
    if (LO(ax) & 0x80 )
      if (1 || !(LO(ax)&0x80) ){
	fprintf(stderr, "Carrying it out\n");
        CARRY;
      }
      else
	NOCARRY;
*/
    break;
  case 0x80:			/* default BIOS device open event */
    LWORD(eax) &= 0x00FF;
    return;
  case 0x81:
    LWORD(eax) &= 0x00FF;
    return;
  case 0x82:
    LWORD(eax) &= 0x00FF;
    return;
  case 0x83:
    h_printf("int 15h event wait:\n");
    show_regs();
    CARRY;
    return;			/* no event wait */
  case 0x84:
    CARRY;
    return;			/* no joystick */
  case 0x85:
    num = LWORD(eax) & 0xFF;	/* default bios handler for sysreq key */
    if (num == 0 || num == 1) {
      LWORD(eax) &= 0x00FF;
      return;
    }
    LWORD(eax) &= 0xFF00;
    LWORD(eax) |= 1;
    CARRY;
    return;
  case 0x86:
    /* wait...cx:dx=time in usecs */
    g_printf("doing int15 wait...ah=0x86\n");
    show_regs();
    wait_time.tv_sec = 0;
    wait_time.tv_usec = (LWORD(ecx) << 16) | LWORD(edx);
    RPT_SYSCALL(select(STDIN_FILENO, NULL, NULL, NULL, &scr_tv));
    NOCARRY;
    return;

  case 0x87:
    if (config.xms_size)
      xms_int15();
    else {
      LWORD(eax) &= 0xFF;
      LWORD(eax) |= 0x0300;	/* say A20 gate failed - a lie but enough */
      CARRY;
    }
    return;

  case 0x88:
    if (config.xms_size) {
      xms_int15();
    }
    else {
      LWORD(eax) &= ~0xffff;	/* we don't have extended ram if it's not XMS */
      NOCARRY;
    }
    return;

  case 0x89:			/* enter protected mode : kind of tricky! */
    LWORD(eax) |= 0xFF00;		/* failed */
    CARRY;
    return;
  case 0x90:			/* no device post/wait stuff */
    CARRY;
    return;
  case 0x91:
    CARRY;
    return;
  case 0xc0:
    LWORD(es) = ROM_CONFIG_SEG;
    LWORD(ebx) = ROM_CONFIG_OFF;
    LO(ax) = 0;
    return;
  case 0xc1:
    CARRY;
    return;			/* no ebios area */
  case 0xc2:
	if (!mice->intdrv) 
        if (mice->type != MOUSE_PS2) {
                REG(eax) = 0500;        /* No ps2 mouse device handler */
                CARRY;
                return;
        }
                
        switch (REG(eax) &= 0x00FF)
        {
                case 0x0000:                    
                        if (LO(bx)) mouse.cursor_on = 1;
                                else mouse.cursor_on = 0;
                        HI(ax) = 0;             
                        break;
                case 0x0001:
                        HI(ax) = 0;
                        LWORD(ebx) = 0xAAAA;    /* we have a ps2 mouse */
                        break;
		case 0x0002:			/* set sampling rate */
			HI(ax) = 1;		/* invalid function */
			CARRY;
			break;
		case 0x0003:
			switch (LO(bx))
			{
			case 0x00:	
				mouse.ratio = 1;
				break;
			case 0x01:
				mouse.ratio = 2;
				break;
			case 0x02:
				mouse.ratio = 4;
				break;
			case 0x03:
				mouse.ratio = 8;
			} 
			HI(ax) = 0;
			break;
		case 0x0004:
			HI(bx) = 0xAA;
			break;
		case 0x0005:
			HI(ax) = 1;
			CARRY;
			break;
                default:
                        g_printf("PS2MOUSE: Unknown call ax=0x%04x\n", LWORD(eax));
                        CARRY;
        }
    return;
  case 0xc3:
    /* no watchdog */
    CARRY;
    return;
  case 0xc4:
    /* no post */
    CARRY;
    return;
  default:
    g_printf("int 15h error: ax=0x%04x\n", LWORD(eax));
    CARRY;
    return;
  }
}

/* the famous interrupt 0x2f
 *     returns 1 if handled by dosemu, else 0
 *
 * note that it switches upon both AH and AX
 */
inline int
int2f(void)
{
  /* TRB - catch interrupt 2F to detect IPX in int2f() */
#ifdef IPX
  if (config.ipxsup) {
    if (LWORD(eax) == INT2F_DETECT_IPX) {
      return (IPXInt2FHandler());
    }
  }
#endif

  /* this is the DOS give up time slice call...*/
  if (LWORD(eax) == INT2F_IDLE_MAGIC) {
    usleep(INT2F_IDLE_USECS);
    REG(eax) = 0;
    return 1;
  }

  /* is it a redirector call ? */
  else if (HI(ax) == 0x11 && mfs_redirector())
    return 1;

  else if (HI(ax) == INT2F_XMS_MAGIC) {
    if (!config.xms_size)
      return 0;
    switch (LO(ax)) {
    case 0:			/* check for XMS */
      x_printf("Check for XMS\n");
      xms_grab_int15 = 0;
      LO(ax) = 0x80;
      break;
    case 0x10:
      x_printf("Get XMSControl address\n");
      REG(es) = XMSControl_SEG;
      LWORD(ebx) = XMSControl_OFF;
      break;
    default:
      x_printf("BAD int 0x2f XMS function:0x%02x\n", LO(ax));
    }
    return 1;
  }

#ifdef DPMI
  /* Call for getting DPMI entry point */
  else if (LWORD(eax) == 0x1687) {

    dpmi_get_entry_point();
    return 1;

  }
  /* Are we in protected mode ? */
  else if (LWORD(eax) == 0x1686) {

    if (in_dpmi)
      REG(eax) = 0;
    D_printf("DPMI 1686 returns %x\n", (int)REG(eax));
    return 1;

  }
#endif

  return 0;
}

inline void
int1a(void)
{
  unsigned long ticks;
  long akt_time, elapsed;
  struct timeval tp;
  struct timezone tzp;
  struct tm *tm;

  switch (HI(ax)) {

    /* A timer read should reset the overflow flag */
  case 0:			/* read time counter */
    time(&akt_time);
    elapsed = akt_time - start_time;
    ticks = (elapsed * 182) / 10 + last_ticks;
    LO(ax) = *(u_char *) (TICK_OVERFLOW_ADDR);
    *(u_char *) (TICK_OVERFLOW_ADDR) = 0;
    LWORD(ecx) = (ticks >> 16) & 0xffff;
    LWORD(edx) = ticks & 0xffff;
    /* dbug_printf("read timer st: %ud %ud t=%d\n",
				    start_time, ticks, akt_time); */
    break;
  case 1:			/* write time counter */
    last_ticks = (LWORD(ecx) << 16) | (LWORD(edx) & 0xffff);
    set_ticks(last_ticks);
    time(&start_time);
    g_printf("set timer to %lu \n", last_ticks);
    break;
  case 2:			/* get time */
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
    tm = localtime((time_t *) & ticks);
    /* g_printf("get time %d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec); */
    HI(cx) = tm->tm_hour % 10;
    tm->tm_hour /= 10;
    HI(cx) |= tm->tm_hour << 4;
    LO(cx) = tm->tm_min % 10;
    tm->tm_min /= 10;
    LO(cx) |= tm->tm_min << 4;
    HI(dx) = tm->tm_sec % 10;
    tm->tm_sec /= 10;
    HI(dx) |= tm->tm_sec << 4;
    /* LO(dx) = tm->tm_isdst; */
    REG(eflags) &= ~CF;
    break;
  case 4:			/* get date */
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
    tm = localtime((time_t *) & ticks);
    tm->tm_year += 1900;
    tm->tm_mon++;
    /* g_printf("get date %d.%d.%d\n", tm->tm_mday, tm->tm_mon, tm->tm_year); */
    LWORD(ecx) = tm->tm_year % 10;
    tm->tm_year /= 10;
    LWORD(ecx) |= (tm->tm_year % 10) << 4;
    tm->tm_year /= 10;
    LWORD(ecx) |= (tm->tm_year % 10) << 8;
    tm->tm_year /= 10;
    LWORD(ecx) |= (tm->tm_year) << 12;
    LO(dx) = tm->tm_mday % 10;
    tm->tm_mday /= 10;
    LO(dx) |= tm->tm_mday << 4;
    HI(dx) = tm->tm_mon % 10;
    tm->tm_mon /= 10;
    HI(dx) |= tm->tm_mon << 4;
    REG(eflags) &= ~CF;
    break;
  case 3:			/* set time */
  case 5:			/* set date */
    error("ERROR: timer: can't set time/date\n");
    break;
  default:
    error("ERROR: timer error AX=0x%04x\n", LWORD(eax));
    /* show_regs(); */
    /* fatalerr = 9; */
    return;
  }
}

/* note that the emulation herein may cause problems with programs
 * that like to take control of certain int 21h functions, or that
 * change functions that the true int 21h functions use.  An example
 * of the latter is ANSI.SYS, which changes int 10h, and int 21h
 * uses int 10h.  for the moment, ANSI.SYS won't work anyway, so it's
 * no problem.
 */
inline int
ms_dos(int nr)
{				/* returns 1 if emulated, 0 if internal handling */

  /* int 21, ah=1,7,or 8:  return 0 for an extended keystroke, but the
   next call returns the scancode */
  /* if I press and hold a key like page down, it causes the emulator to
   exit! */

Restart:
  /* dbug_printf("DOSINT 0x%02x\n", nr); */
  /* emulate keyboard io to avoid DOS' busy wait */

  switch (nr) {

  case 12:			/* clear key buffer, do int AL */
    keybuf_clear();
    nr = LO(ax);
    if (nr == 0)
      break;			/* thanx to R Michael McMahon for the hint */
    HI(ax) = LO(ax);
    NOCARRY;
    goto Restart;

#define DOS_HANDLE_OPEN		0x3d
#define DOS_HANDLE_CLOSE	0x3e
#define DOS_IOCTL		0x44
#define IOCTL_GET_DEV_INFO	0
#define IOCTL_CHECK_OUTPUT_STS	7

    /* XXX - MAJOR HACK!!! this is bad bad wrong.  But it'll probably work, unless
 * someone puts "files=200" in his/her config.sys
 */
#define EMM_FILE_HANDLE 200

#ifndef USE_NCURSES
#define FALSE 0
#define TRUE 1
#endif

    /* lowercase and truncate to 3 letters the replacement extension */
#define ext_fix(s) { char *r=(s); \
		     while (*r) { *r=toupper(*r); r++; } \
		     if ((r - s) > 3) s[3]=0; }

    /* we trap this for two functions: simulating the EMMXXXX0
	 * device, and fudging the CONFIG.XXX and AUTOEXEC.XXX
	 * bootup files
	 */
  case DOS_HANDLE_OPEN:{
      char *ptr = SEG_ADR((char *), ds, dx);

#ifdef INTERNAL_EMS
      if (!strncmp(ptr, "EMMXXXX0", 8) && config.ems_size) {
	E_printf("EMS: opened EMM file!\n");
	LWORD(eax) = EMM_FILE_HANDLE;
	NOCARRY;
	show_regs();
	return (1);
      }
      else
#endif
      if (!strncmp(ptr, "\\CONFIG.SYS", 11) && config.emusys) {
	ext_fix(config.emusys);
	sprintf(ptr, "\\CONFIG.%-3s", config.emusys);
	d_printf("DISK: Substituted %s for CONFIG.SYS\n", ptr);
      }
      /* ignore explicitly selected drive by incrementing ptr by 1 */
      else if (!strncmp(ptr + 1, ":\\AUTOEXEC.BAT", 14) && config.emubat) {
	ext_fix(config.emubat);
	sprintf(ptr + 1, ":\\AUTOEXEC.%-3s", config.emubat);
	d_printf("DISK: Substituted %s for AUTOEXEC.BAT\n", ptr + 1);
      }

      return (0);
    }

#ifdef INTERNAL_EMS
  case DOS_HANDLE_CLOSE:
    if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
      return (0);
    else {
      E_printf("EMS: closed EMM file!\n");
      NOCARRY;
      show_regs();
      return (1);
    }

  case DOS_IOCTL:
    {

      if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
	return (FALSE);

      switch (LO(ax)) {
      case IOCTL_GET_DEV_INFO:
	E_printf("EMS: dos_ioctl getdevinfo emm.\n");
	LWORD(edx) = 0x80;
	NOCARRY;
	show_regs();
	return (TRUE);
	break;
      case IOCTL_CHECK_OUTPUT_STS:
	E_printf("EMS: dos_ioctl chkoutsts emm.\n");
	LO(ax) = 0xff;
	NOCARRY;
	show_regs();
	return (TRUE);
	break;
      }
      error("ERROR: dos_ioctl shouldn't get here. XXX\n");
      return (FALSE);
    }
#endif

#ifdef DPMI
    /* DPMI must leave protected mode with an int21 4c call */
  case 0x4c:
    if (in_dpmi) {
      D_printf("Leaving DPMI with err of 0x%02x\n", LO(ax));
      /* in_dpmi_dos_int = 0; */
      in_dpmi = 0;
    }
    return 0;
#endif

  default:
#if 0
    /* taking this if/printf out will speed things up a bit...*/
    if (d.dos)
      dbug_printf(" dos interrupt 0x%02x, ax=0x%04x, bx=0x%04x\n",
		  nr, LWORD(eax), LWORD(ebx));
#endif
    return 0;
  }
  return 1;
}

inline void
run_int(int i)
{
  unsigned char *ssp;
  unsigned long sp;

#if 0
  if (i == 0x16 && config.hogthreshold && KBD_Head == KBD_Tail ) {
    static struct timeval tp1;
    static struct timeval tp2;
    static int time_count = 0;
    if (time_count == 0)
      gettimeofday(&tp1, NULL);
    else {
      gettimeofday(&tp2, NULL);
      if ((tp2.tv_sec - tp1.tv_sec) * 1000000 +
	  ((int) tp2.tv_usec - (int) tp1.tv_usec) < config.hogthreshold){
	usleep(100);
      }
    }
    time_count = (time_count + 1) % 10;
  }

  /* XXX bootstrap cs is now 0 */
  if (!(REG(eip) && REG(cs))) {
    error("run_int: not running NULL interrupt 0x%04x handler\n", i);
    show_regs();
    return 0;
  }
#endif

  ssp = (unsigned char *)(REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, vflags);
  pushw(ssp, sp, LWORD(cs));
  pushw(ssp, sp, LWORD(eip));
  LWORD(esp) -= 6;
  LWORD(cs) = ((us *) 0)[(i << 1) + 1];
  LWORD(eip) = ((us *) 0)[i << 1];

  /* clear TF (trap flag, singlestep), IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   */
  REG(eflags) &= ~(VIF | TF | IF | NT);
}

inline int
can_revector(int i)
{
  /* here's sort of a guideline:
 * if we emulate it completely, but there is a good reason to stick
 * something in front of it, and it seems to work, by all means revector it.
 * if we emulate none of it, say yes, as that is a little bit faster.
 * if we emulate it, but things don't seem to work when it's revectored,
 * then don't let it be revectored.
 *
 */

  if (i < 0x21 || i > 0xe8 ) return 1;

  switch (i) {

    /* some things, like 0x29, need to be unrevectored if the vectors
       * are the DOS vectors...but revectored otherwise
       */
  case 0x21:			/* we want it first...then we'll pass it on */
  case 0x2f:			/* needed for XMS, redirector, and idle interrupt */
if ((mice->type == MOUSE_PS2) || (mice->intdrv)) {
  case 0x74:			/* needed for PS/2 Mouse */
}
  case 0xe6:			/* for redirector and helper (was 0xfe) */
  case 0xe7:			/* for mfs FCB helper */
  case 0xe8:			/* for int_queue_run return */
    return 0;

  case 0x28:
   if (!config.keybint && config.console_keyb)
      return 0;
    else
      return 1;
    break;

  case 0x33:			/* mouse...we're testing */
    if (mice->intdrv)
      return 0;
    else
      return 1;
    break;
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
  case 0x15:
  case 0x25:			/* absolute disk read, calls int 13h */
  case 0x26:			/* absolute disk write, calls int 13h */
  case 0x1b:			/* ctrl-break handler */
  case 0x1c:			/* user timer tick */
  case 0x16:			/* BIOS keyboard */
  case 0x17:			/* BIOS printer */
  case 0x10:			/* BIOS video */
  case 0x13:			/* BIOS disk */
  case 0x27:			/* TSR */
  case 0x20:			/* exit program */
  case 0x2a:
  case 0x60:
  case 0x61:
  case 0x62:
  case 0x67:			/* EMS */
  case 0xfe:			/* Turbo Debugger and others... */
    return 1;
  default:
    g_printf("revectoring 0x%02x\n", i);
    return 1;
  }
}

inline void
do_int(int i)
{
  u_char highint = 0;

#if 0
  saytime("do_int");
#endif

#if 0
  k_printf("Do INT0x%02x, eax=0x%04x, ebx=0x%04x ss=0x%04x esp=0x%04x cs=0x%04x ip=0x%04x CF=%d \n", i, LWORD(eax), LWORD(ebx), LWORD(ss), LWORD(esp), LWORD(cs), LWORD(eip), (int)REG(eflags) & CF);
#endif

  if ((LWORD(cs)) == BIOSSEG)
    highint = 1;
  else
    if (IS_REDIRECTED(i) && can_revector(i)){
      run_int(i);
      return;
    }

  switch (i) {
  case 0x5:
    g_printf("BOUNDS exception\n");
    goto default_handling;
    return;
  case 0x0a:
  case 0x0b:			/* com2 interrupt */
  case 0x0c:			/* com1 interrupt */
  case 0x0d:
  case 0x0e:
  case 0x0f:
    g_printf("IRQ->interrupt %x\n", i);
    show_regs();
    goto default_handling;
  case 0x09:			/* IRQ1, keyb data ready */
    fprintf(stderr, "IRQ->interrupt %x\n", i);
    run_int(0x09);
    return;
  case 0x08:
    int08();
    return;
  case 0x10:			/* VIDEO */
    int10();
    return;
  case 0x11:			/* CONFIGURATION */
    LWORD(eax) = configuration;
    return;
  case 0x12:			/* MEMORY */
    LWORD(eax) = config.mem_size;
    return;
  case 0x13:			/* DISK IO */
    int13();
    return;
  case 0x14:			/* COM IO */
    int14();
    return;
  case 0x15:			/* Cassette */
    int15();
    REG(eflags) |= VIF;
    return;
  case 0x16:			/* KEYBOARD */
    run_int(0x16);
    return;
  case 0x17:			/* PRINTER */
    int17();
    return;
  case 0x18:			/* BASIC */
    break;
  case 0x19:			/* LOAD SYSTEM */
    boot();
    return;
  case 0x1a:			/* CLOCK */
    int1a();
    return;
#if 0
  case 0x1b:			/* BREAK */
  case 0x1c:			/* TIMER */
  case 0x1d:			/* SCREEN INIT */
  case 0x1e:			/* DEF DISK PARMS */
  case 0x1f:			/* DEF GRAPHIC */
  case 0x20:			/* EXIT */
  case 0x27:			/* TSR */
#endif

  case 0x21:			/* MS-DOS */
    if (ms_dos(HI(ax)))
      return;
    /* else do default handling in vm86 mode */
    goto default_handling;

  case 0x28:			/* KEYBOARD BUSY LOOP */
    {
      static int first = 1;

      if (first && !config.keybint && config.console_keyb) {
	first = 0;
	/* revector int9, so dos doesn't play with the keybuffer */
	k_printf("revectoring int9 away from dos\n");
	SETIVEC(0x9, BIOSSEG, 16 * 0x8 + 2);	/* point to IRET */
      }
    }
    if (int28())
      return;
    goto default_handling;
    return;

  case 0x29:			/* FAST CONSOLE OUTPUT */
    /* char in AL */
    char_out(*(char *) &REG(eax), bios_current_screen_page, ADVANCE);	
    return;

  case 0x2a:			/* CRITICAL SECTION */
    goto default_handling;

  case 0x2f:			/* Multiplex */
    if (int2f())
      return;
    goto default_handling;

  case 0x33:			/* mouse */
if (mice->intdrv) {
    mouse_int();
    return;
}
else
    goto default_handling;
    break;

  case 0x67:			/* EMS */
    goto default_handling;

  case 0x60:			/* new packet driver interface */
    if (pkt_int())
      return;
    goto default_handling;

  case 0x74:			/* PS/2 Mouse */
    return;			/* Need to implement real ps/2 mouse */
    break; 			/* Should call hardware 0x12 */
				/* for the moment, internaldriver only support */

  case 0xe6:			/* dos helper and mfs startup (was 0xfe) */
    if (dos_helper())
      return;
    goto default_handling;
    return;

  case 0xe7:			/* mfs FCB call */
    SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
    run_int(0xe7);
    return;

  case 0xe8:
    {
      static unsigned short *csp;
      static int x;
      csp = SEG_ADR((us *), cs, ip) - 1;
      for (x=1; x<=int_queue_running; x++) {
        /* check if any int is finished */
        if ((int)csp == int_queue_head[x].int_queue_return_addr) {
          /* if it's finished - clean up */
          /* call user cleanup function */
          if (int_queue_head[x].int_queue_ptr.callend) {
    	    int_queue_head[x].int_queue_ptr.callend(int_queue_head[x].int_queue_ptr.interrupt);
            int_queue_head[x].int_queue_ptr.callend = NULL;
	  }

          /* restore registers */
          REGS = int_queue_head[x].saved_regs;
          REG(eflags) |= VIF;

          h_printf("e8 int_queue: finished %x\n", int_queue_head[x].int_queue_return_addr);
	  *OUTB_ADD=1;
	  if (int_queue_running == x ) 
            int_queue_running--;
	  return;
        }
      }
    }
    h_printf("e8 int_queue: shouldn't get here\n");
    show_regs();
    return;

  default:
    if (d.defint)
      dbug_printf("int 0x%02x, ax=0x%04x\n", i, LWORD(eax));
    /* fall through */

  default_handling:

    if (highint) {
      return;
    }

    if (!IS_REDIRECTED(i)) {
      g_printf("DEFIVEC: int 0x%02x  SG: 0x%04x  OF: 0x%04x\n",
	       i, ISEG(i), IOFF(i));
      return;
    }

    if (IS_IRET(i)) {
      if ((i != 0x2a) && (i != 0x28))
	g_printf("just an iret 0x%02x\n", i);
      return;
    }

    run_int(i);

    return;
  }
  error("\nERROR: int 0x%02x not implemented\n", i);
  show_regs();
  fatalerr = 1;
  return;
}

#undef INT_H
