#if 0  /* set it 0 for the new INT1A AH=0 code */
  #define USE_UNIX_TIME_FOR_INT1A_AH0
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "timers.h"
#include "mouse.h"
#include "disks.h"
#include "bios.h"
#include "xms.h"
#include "int.h"
#include "dos2linux.h"
#include "video.h"
#include "vc.h"

#ifdef USING_NET
#include "ipx.h"
#endif

#include "dpmi.h"

#ifdef NEW_KBD_CODE
#include "keyb_server.h"
#else
extern void scan_to_buffer(void);
#endif

#define DJGPP_HACK	/* AV Feb 97 */

/*
   This flag will be set when doing video routines so that special
   access can be given
*/
static u_char         in_video = 0;
static u_char         save_hi_ints[128];

static int            card_init = 0;
static unsigned long  precard_eip, precard_cs;

static struct timeval scr_tv;        /* For translating UNIX <-> DOS times */

#ifdef USE_MRP_JOYSTICK
#include <linux/joystick.h>
#include <fcntl.h>
#include <string.h>
static void mrp_read_joystick(void);
#endif

void kill_time(long usecs) {
   struct timeval scr_tv, t_start, t_end;
   long t_dif;
   scr_tv.tv_sec = 0L;
   scr_tv.tv_usec = usecs;

   gettimeofday(&t_start, NULL);
   while ((int) select(STDIN_FILENO, NULL, NULL, NULL, &scr_tv) < (int) 1)
     {
        gettimeofday(&t_end, NULL);
        t_dif = ((t_end.tv_sec * 1000000 + t_end.tv_usec) -
                 (t_start.tv_sec * 1000000 + t_start.tv_usec));

        if ((t_dif >= usecs) || (errno != EINTR))
          return ;
        scr_tv.tv_sec = 0L;
        scr_tv.tv_usec = usecs - t_dif;
     }
}

/*
 * DANG_BEGIN_FUNCTION DEFAULT_INTERRUPT 
 *
 * description:
 * DEFAULT_INTERRUPT is the default interrupt service routine 
 * called when DOSEMU initializes.
 *
 * DANG_END_FUNCTION
 */

static void default_interrupt(u_char i) {
  if (d.defint)
    dbug_printf("int 0x%02x, ax=0x%04x\n", i, LWORD(eax));

  if (!IS_REDIRECTED(i) ||
      (LWORD(cs) == BIOSSEG && LWORD(eip) == (i * 16 + 2))) {
    g_printf("DEFIVEC: int 0x%02x @ 0x%04x:0x%04x\n", i, ISEG(i), IOFF(i));

    /* This is here for old SIGILL's that modify IP */
    if (i == 0x00)
      LWORD(eip)+=2;
  } else if (IS_IRET(i)) {
    if ((i != 0x2a) && (i != 0x28))
      g_printf("just an iret 0x%02x\n", i);
  } else
    run_int(i);
}

/* returns 1 if dos_helper() handles it, 0 otherwise */
static int dos_helper(void)
{
  switch (LO(ax)) {
  case 0:			/* Linux dosemu installation test */
    LWORD(eax) = 0xaa55;
    LWORD(ebx) = VERSION * 0x100 + SUBLEVEL; /* major version 0.49 -> 0049 */
    /* The patch level in the form n.n is a float...
     * ...we let GCC at compiletime translate it to 0xHHLL, HH=major, LL=minor.
     * This way we avoid usage of float instructions.
     */
    LWORD(ecx) = ((((int)(PATCHLEVEL * 100))/100) << 8) + (((int)(PATCHLEVEL * 100))%100);
    g_printf("WARNING: dosemu installation check\n");
    show_regs(__FILE__, __LINE__);
    break;

  case 1:			/* SHOW_REGS */
    show_regs(__FILE__, __LINE__);
    break;

  case 2:			/* SHOW INTS, BH-BL */
    show_ints(HI(bx), LO(bx));
    break;

  case 3:			/* SET IOPERMS: bx=start, cx=range,
				   carry set for get, clear for release */
  {
    int cflag = LWORD(eflags) & CF ? 1 : 0;

    i_printf("I/O perms: 0x%04x 0x%04x %d\n", LWORD(ebx), LWORD(ecx), cflag);
    if (set_ioperm(LWORD(ebx), LWORD(ecx), cflag)) {
      error("ERROR: SET_IOPERMS request failed!!\n");
      CARRY;			/* failure */
    } else {
      if (cflag)
	warn("WARNING! DOS can now access I/O ports 0x%04x to 0x%04x\n",
	     LWORD(ebx), LWORD(ebx) + LWORD(ecx) - 1);
      else
	warn("Access to ports 0x%04x to 0x%04x clear\n",
	     LWORD(ebx), LWORD(ebx) + LWORD(ecx) - 1);
      NOCARRY;		/* success */
    }
  }
  break;

  case 4:			/* initialize video card */
    if (LO(bx) == 0) {
      if (set_ioperm(0x3b0, 0x3db - 0x3b0, 0))
	warn("couldn't shut off ioperms\n");
      SETIVEC(0x10, BIOSSEG, 0x10 * 0x10);	/* restore our old vector */
      config.vga = 0;
    } else {
      unsigned char *ssp;
      unsigned long sp;

      if (!config.mapped_bios) {
	error("ERROR: CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
	return 1;
      }
      if (set_ioperm(0x3b0, 0x3db - 0x3b0, 1))
	warn("couldn't get range!\n");
      config.vga = 1;
      set_vc_screen_page(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
      warn("WARNING: jumping to 0[c/e]000:0003\n");

      ssp = (unsigned char *) (REG(ss) << 4);
      sp = (unsigned long) LWORD(esp);
      pushw(ssp, sp, LWORD(cs));
      pushw(ssp, sp, LWORD(eip));
      precard_eip = LWORD(eip);
      precard_cs = LWORD(cs);
      LWORD(esp) -= 4;
      LWORD(cs) = config.vbios_seg;
      LWORD(eip) = 3;
      show_regs(__FILE__, __LINE__);
      card_init = 1;
    }

  case 5:			/* show banner */
    p_dos_str("\n\nLinux DOS emulator " VERSTR " $Date: "__DATE__" "__TIME__" $\n");
    p_dos_str("Last configured at %s\n", CONFIG_TIME);
    p_dos_str("on %s\n", CONFIG_HOST);
    /* p_dos_str("Formerly maintained by Robert Sanders, gt8134b@prism.gatech.edu\n\n"); */
    p_dos_str("Bugs, Patches & New Code to linux-msdos@vger.rutgers.edu\n\n");
    if (config.dpmi)
      p_dos_str("DPMI-Server Version 0.9 installed\n\n");
    break;

#ifdef NEW_KBD_CODE
   case 6:
      k_printf("KBD: WARNING: outdated keyboard helper fn 6 was called!\n");
      break;

   case 7:                       /* INT 09 "get bios key" helper */
      _AX=get_bios_key();
      k_printf("HELPER: get_bios_key() returned %04x\n",_AX);
      break;
#else
   case 6:			/* Do inline int09 insert_into_keybuffer() */
    k_printf("KBD: Doing INT9 insert_into_keybuffer() bx=0x%04x\n", LWORD(ebx));
    scan_to_buffer();
    break;
#endif
     
  case 8:
    v_printf("Starting Video initialization\n");
    if (config.allowvideoportaccess) {
      if (config.speaker != SPKR_NATIVE) {
	v_printf("Giving access to port 0x42\n");
	set_ioperm(0x42, 1, 1);
      }
      in_video = 1;
    }
    /* Many video BIOSes use hi interrupt vector locations as
     * scratchpad area - this is because they come before DOS and feel
     * safe to do it. But we are initializing vectors before video, so
     * this will cause trouble. I assume no video BIOS will ever:
     * - change vectors < 0xe0 (0:380-0:3ff area)
     * - change anything in the vector area _after_ installation
     */
    v_printf("Save hi vector area\n");
    MEMCPY_2UNIX(save_hi_ints,0x380,128);
    break;

  case 9:
    v_printf("Finished with Video initialization\n");
    if (config.allowvideoportaccess) {
      if (config.speaker != SPKR_NATIVE) {
        v_printf("Removing access to port 0x42\n");
        set_ioperm(0x42, 1, 0);
      }
      in_video = 0;
    }
    v_printf("Restore hi vector area\n");
    MEMCPY_2DOS(0x380,save_hi_ints,128);
    if (mice->intdrv) { /* grab int10 back from video card for mouse */
        void bios_f000(), bios_f000_int10_old();
        us *ptr = (us*)((BIOSSEG << 4) +
                        ((long)bios_f000_int10_old - (long)bios_f000));
        m_printf("ptr is at %p; ptr[0] = %x, ptr[1] = %x\n",ptr,ptr[0],ptr[1]);
        ptr[0] = IOFF(0x10);
        ptr[1] = ISEG(0x10);
        m_printf("after store, ptr[0] = %x, ptr[1] = %x\n",ptr[0],ptr[1]);
    }
    break;

  case 0x10:
    /* TRB - handle dynamic debug flags in dos_helper() */
    LWORD(eax) = GetDebugFlagsHelper((char *) (((_regs.es & 0xffff) << 4) +
					       (_regs.edi & 0xffff)));
    g_printf("DBG: Get flags\n");
    break;

  case 0x11:
    g_printf("DBG: Set flags\n");
    LWORD(eax) = SetDebugFlagsHelper((char *) (((_regs.es & 0xffff) << 4) +
					       (_regs.edi & 0xffff)));
    g_printf("DBG: Flags set\n");
    break;

  case 0x12:
    g_printf("IDLE: Setting hogthreshold value to %u\n", LWORD(ebx));
    config.hogthreshold = LWORD(ebx);
    break;

  case 0x20:
    mfs_inte6();
    return 1;

  case 0x21:
    ems_helper();
    return 1;

  case 0x22:
  {
    unsigned char *ssp;
    unsigned long sp;

    ssp = (unsigned char *) (REG(ss) << 4);
    sp = (unsigned long) LWORD(esp);

    LWORD(eax) = popw(ssp, sp);
    LWORD(esp) += 2;
    E_printf("EMS: in 0xe6,0x22 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x, "
	     "cx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx), LWORD(ecx));
    if (config.ems_size)
      ems_fn(&REGS);
    else{
      error("EMS: not running ems_fn!\n");
      return 0;
    }
    break;
  }

  case 0x28:                    /* Mouse garrot helper */
    if (!LWORD(ebx))   /* Wait sub-function requested */
      usleep(INT28_IDLE_USECS);
    else {             /* Get Hogthreshold value sub-function*/
      LWORD(ebx)= config.hogthreshold;
      LWORD(eax)= config.hogthreshold;
    }
    break;

  case 0x29:			/* Serial helper */
    serial_helper();
    break;

  case 0x30:			/* set/reset use bootdisk flag */
    use_bootdisk = LO(bx) ? 1 : 0;
    break;

  case 0x33:			/* set mouse vector */
    mouse_helper();
    break;

  case 0x40:{
      E_printf("CDROM: in 0x40 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x, "
	       "cx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx), LWORD(ecx));
      cdrom_helper();
      break;
    }

  case 0x50:
    if (config.secure) {
      CARRY;
      break;
    }
    /* run the unix command in es:dx (a null terminated buffer) */
    g_printf("Running Unix Command\n");
    run_unix_command(SEG_ADR((char *), es, dx));
    break;   

  case 0x51:
    if (config.secure) {
      CARRY;
      LWORD(eax) = 0;
      break;
    }
    /* Get DOS command from UNIX in es:dx (a null terminated buffer) */
    g_printf("Locating DOS Command\n");
    LWORD(eax) = misc_e6_commandline(SEG_ADR((char *), es, dx));
    break;   

  case 0x52:
    if (config.secure) {
      CARRY;
      LWORD(eax) = 0;
      break;
    }
    /* Interrogate the UNIX environment in es:dx (a null terminated buffer) */
    g_printf("Interrogating UNIX Environment\n");
    LWORD(eax) = misc_e6_envvar(SEG_ADR((char *), es, dx));
    break;   
  case 0x53:
    if (config.secure) {
      CARRY;
      LWORD(eax) = 0;
      break;
    }
	LWORD(eax) = system(SEG_ADR((char *), es, dx));
	break;

#ifdef IPX
  case 0x7a:
    if (config.ipxsup) {
      /* TRB handle IPX far calls in dos_helper() */
      IPXFarCallHandler();
    }
    break;
  case 0x7b:
    if (config.ipxsup) {
      /* Allow notification of ESR etc... ends */
      IPXEndCall();
    }
    break;
#endif
    case 0x80:
        LWORD(eax) = (short)((int)getcwd(SEG_ADR((char *), es, dx), (size_t)LWORD(eax)));
        break;
  case 0x81:
        LWORD(eax) = chdir(SEG_ADR((char *), es, dx));
        break;
  case 0xff:
    if (LWORD(eax) == 0xffff) {
      /* terminate code is in bx */
      dbug_printf("DOS termination requested\n");
      p_dos_str("\n\rLeaving DOS...\n\r");
      leavedos(LO(bx));
    }
    break;

/*
 * DANG_BEGIN_REMARK
 * The Helper Interrupt uses the following groups:
 * 
 * 0x00      - Check for DOSEMU
 * 0x01-0x11 - Initialisation functions & Debugging
 * 0x12      - Set hogthreshold (aka garrot?)
 * 0x20      - MFS functions
 * 0x21-0x22 - EMS functions
 * 0x28      - Garrot Functions for use with the mouse
 * 0x29      - Serial functions
 * 0x30      - Whether to use the BOOTDISK predicate
 * 0x33      - Mouse Functions
 * 0x40      - CD-ROM functions
 * 0x50-0x52 - DOSEMU/Linux communications
 *      50 -- run unix command in ES:DX
 *      51,52?
 *      53 -- do system(ES:DX)
 * 0x7a      - IPX functions
 * 0x8x   -- utility functions
 *	0x80 -- getcwd(ES:DX, size AX)
 *	0x81 -- chdir(ES:DX)
 * 0xff      - Terminate DOSEMU
 *
 * There are (as yet) no guidelines on choosing areas for new functions.
 * DANG_END_REMARK
 */


  default:
    error("ERROR: bad dos helper function: AX=0x%04x\n", LWORD(eax));
    return 0;
  }

  return 1;
}

static void int08(u_char i)
{
  run_int(0x1c);
  /* REG(eflags) |= VIF; */
  WRITE_FLAGSE(READ_FLAGSE() | VIF);
  return;
}

static void int15(u_char i)
{
  int num;

  if (HI(ax) != 0x4f)
    NOCARRY;
  /* REG(eflags) |= VIF;
  WRITE_FLAGSE(READ_FLAGSE() | VIF);
  */

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
    /*k_printf("INT15 0x4f CARRY=%x AX=%x\n", (LWORD(eflags) & CF),LWORD(eax));*/
    k_printf("INT15 0x4f CARRY=%x AX=%x\n", (READ_FLAGS() & CF),LWORD(eax));
/*
    CARRY;
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
    show_regs(__FILE__, __LINE__);
    CARRY;
    return;			/* no event wait */
  case 0x84:
#ifdef USE_MRP_JOYSTICK
    mrp_read_joystick ();
    h_printf ("int 15h joystick int: %d %d\n", (int)(LWORD(eax)), (int)(LWORD(ebx)));
    return;
#else
    CARRY;
    return;			/* no joystick */
#endif
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
    show_regs(__FILE__, __LINE__);
    kill_time((long)((LWORD(ecx) << 16) | LWORD(edx)));
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
      LWORD(eax) &= ~0xffff;	/* no extended ram if it's not XMS */
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
  case 0xbf:			/* DOS/16M,DOS/4GW */
    switch (REG(eax) &= 0x00FF)
      {
        case 0: case 1: case 2:		/* installation check */
        default:
          REG(edx) = 0;
          CARRY;
          return;
      }
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
    m_printf("PS2MOUSE: Call ax=0x%04x\n", LWORD(eax));
    if (!mice->intdrv)
      if (mice->type != MOUSE_PS2) {
	REG(eax) = 0x0500;        /* No ps2 mouse device handler */
	CARRY;
	return;
      }
                
    switch (REG(eax) &= 0x00FF)
      {
      case 0x0000:                    
	mouse.ps2.state = HI(bx);
	if (mouse.ps2.state == 0)
	  mice->intdrv = FALSE;
	else
	  mice->intdrv = TRUE;
 	HI(ax) = 0;
	NOCARRY;		
 	break;
      case 0x0001:
	HI(ax) = 0;
	LWORD(ebx) = 0xAAAA;    /* we have a ps2 mouse */
	NOCARRY;
	break;
      case 0x0003:
	if (HI(bx) != 0) {
	  CARRY;
	  HI(ax) = 1;
	} else {
	  NOCARRY;
	  HI(ax) = 0;
	}
	break;
      case 0x0004:
	HI(bx) = 0xAA;
	HI(ax) = 0;
	NOCARRY;
	break;
      case 0x0005:			/* Initialize ps2 mouse */
	HI(ax) = 0;
	mouse.ps2.pkg = HI(bx);
	NOCARRY;
	break;
      case 0x0006:
	switch (HI(bx)) {
	case 0x00:
	  LO(bx)  = (mouse.rbutton ? 1 : 0);
	  LO(bx) |= (mouse.lbutton ? 4 : 0);
	  LO(bx) |= 0; 	/* scaling 1:1 */
	  LO(bx) |= 0x20;	/* device enabled */
	  LO(bx) |= 0;	/* stream mode */
	  LO(cx) = 0;		/* resolution, one count */
	  LO(dx) = 0;		/* sample rate */
	  HI(ax) = 0;
	  NOCARRY;
	  break;
	case 0x01:
	  HI(ax) = 0;
	  NOCARRY;
	  break;
	case 0x02:
	  HI(ax) = 1;
	  CARRY;
	  break;
	}
	break;
#if 0
      case 0x0007:
	pushw(ssp, sp, 0x000B);
	pushw(ssp, sp, 0x0001);
	pushw(ssp, sp, 0x0001);
	pushw(ssp, sp, 0x0000);
	REG(cs) = REG(es);
	REG(eip) = REG(ebx);
	HI(ax) = 0;
	NOCARRY;
	break;
#endif
     default:
	HI(ax) = 1;
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
  case 0xc9:
    if (LO(ax) == 0x10) {
	HI(ax) = 0;
	HI(cx) = vm86s.cpu_type;
	LO(cx) = 0x20;
	return;
    }
  /* else fall through */
  case 0x24:		/* PS/2 A20 gate support */
  case 0xd8:		/* EISA - should be set in config? */
  case 0xda:
  case 0xdb:
	HI(ax) = 0x86;
	break;
  default:
    g_printf("int 15h error: ax=0x%04x\n", LWORD(eax));
    CARRY;
    return;
  }
}

static void int1a(u_char i)
{
  time_t time_val;
  struct timeval;
  struct timezone;
  struct tm *tm;

#ifdef USE_UNIX_TIME_FOR_INT1A_AH0
  time_t akt_time;
  unsigned int test_date;
#endif

  switch (HI(ax)) {

    /* A timer read should reset the overflow flag */
  case 0:			/* read time counter */
#ifdef USE_UNIX_TIME_FOR_INT1A_AH0
    time(&akt_time);
    tm = localtime((time_t *) &akt_time);
    test_date = tm->tm_year * 10000 + tm->tm_mon * 100 + tm->tm_mday;
    if ( check_date != test_date ) {
      start_time = akt_time;
      check_date = tm->tm_year * 10000 + tm->tm_mon * 100 + tm->tm_mday;
      g_printf("Over 24hrs forward or backward\n");
      *(u_char *) (TICK_OVERFLOW_ADDR) += 0x1;
    }
    last_ticks = (tm->tm_hour * 60 * 60 + tm->tm_min * 60 + tm->tm_sec) * 18.206;
    LO(ax) = *(u_char *) (TICK_OVERFLOW_ADDR);
    LWORD(ecx) = (last_ticks >> 16) & 0xffff;
    LWORD(edx) = last_ticks & 0xffff;
  #if 0
    g_printf("read timer st:%u ticks:%u act:%u, actdate:%d\n",
	     start_time, last_ticks, akt_time, tm->tm_mday);
  #endif
    set_ticks(last_ticks);
#else
    ignore_segv++;
    last_ticks = *((unsigned long *)(BIOS_TICK_ADDR));
    LO(ax) = *(u_char *) (TICK_OVERFLOW_ADDR);
    *(u_char *) (TICK_OVERFLOW_ADDR) = 0;
    ignore_segv--;
    LWORD(ecx) = (last_ticks >> 16) & 0xffff;
    LWORD(edx) = last_ticks & 0xffff;
#endif
    break;
  case 1:			/* write time counter */
    last_ticks = (LWORD(ecx) << 16) | (LWORD(edx) & 0xffff);
    set_ticks(last_ticks);
#ifdef USE_UNIX_TIME_FOR_INT1A_AH0
    time(&start_time);
#endif
    g_printf("set timer to %lu \n", last_ticks);
    break;
  case 2:			/* get time */
    time(&time_val);
    tm = localtime((time_t *) &time_val);
    g_printf("get time %d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
#if 0
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
#endif
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
    /* REG(eflags) &= ~CF; */
    NOCARRY;
    break;
  case 4:			/* get date */
    time(&time_val);
    tm = localtime((time_t *) &time_val);
    tm->tm_year += 1900;
    tm->tm_mon++;
    g_printf("get date %02d.%02d.%04d\n", tm->tm_mday, tm->tm_mon, tm->tm_year);
#if 0
    gettimeofday(&tp, &tzp);
    ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
#endif
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
    /* REG(eflags) &= ~CF; */
    NOCARRY;
    break;

  case 3:			/* set time */
  case 5:			/* set date */
    g_printf("WARNING: timer: can't set time/date\n");
    break;

  case 0xb1:			/* Intel PCI BIOS v 2.0c */
    switch (LO(ax)) {
      case 1: case 0x81:	/* installation check */
        NOCARRY;		/* AH==0 if installed */
        break;
      case 2: case 0x82:	/* find PCI device */
        g_printf("FIND PCI device %d id=%04x vend=%04x\n", LWORD(esi),
        	LWORD(ecx), LWORD(edx));
      case 3: case 0x83:	/* find PCI class code */
	HI(ax) = 0x86;		/* not found */
      default:
        CARRY;
        break;
    }
    break;

  default:
    g_printf("WARNING: unsupported timer call 0x%04x\n", LWORD(eax));
    /* show_regs(__FILE__, __LINE__); */
    /* fatalerr = 9; */
    return;
  }
}

/* ========================================================================= */
/* note that the emulation herein may cause problems with programs
 * that like to take control of certain int 21h functions, or that
 * change functions that the true int 21h functions use.  An example
 * of the latter is ANSI.SYS, which changes int 10h, and int 21h
 * uses int 10h.  for the moment, ANSI.SYS won't work anyway, so it's
 * no problem.
 */
/* XXX - MAJOR HACK!!! this is bad bad wrong.  But it'll probably work
 * unless someone puts "files=200" in his/her config.sys
 */
#define EMM_FILE_HANDLE 200

/* uppercase and truncate to 3 letters the replacement extension */
#define ext_fix(s) { char *r=(s); \
		     while (*r) { *r=toupper(*r); r++; } \
		     if ((r - s) > 3) s[3]=0; }

static int ms_dos(int nr)
{
  /* we trap this for two functions: simulating the EMMXXXX0 device and
   * fudging the CONFIG.XXX and AUTOEXEC.XXX bootup files.
   */
  switch (nr) {
  case 0x3d:       /* DOS handle open */
  {
    char *ptr = SEG_ADR((char *), ds, dx);

    /* ignore explicitly selected drive by incrementing ptr by 1 */
    if (config.emubat && !strncmp(ptr + 1, ":\\AUTOEXEC.BAT", 14)) {
      ext_fix(config.emubat);
      sprintf(ptr + 1, ":\\AUTOEXEC.%-3s", config.emubat);
      d_printf("DISK: Substituted %s for AUTOEXEC.BAT\n", ptr + 1);
    } else if (config.emusys && !strncmp(ptr, "\\CONFIG.SYS", 11)) {
      ext_fix(config.emusys);
      sprintf(ptr, "\\CONFIG.%-3s", config.emusys);
      d_printf("DISK: Substituted %s for CONFIG.SYS\n", ptr);
 } else if (config.emuini && !strncmp(ptr+2, "\\WINDOWS\\SYSTEM.INI", 19)) {
 ext_fix(config.emuini);
      sprintf(ptr+2, "\\WINDOWS\\SYSTEM.%s", config.emuini);
      d_printf("DISK: Substituted %s for system.ini\n", ptr);
#ifndef INTERNAL_EMS
    }
#else
    } else if (config.ems_size && !strncmp(ptr, "EMMXXXX0", 8)) {
      E_printf("EMS: opened EMM file!\n");
      LWORD(eax) = EMM_FILE_HANDLE;
      NOCARRY;
      show_regs(__FILE__, __LINE__);
      return 1;
    }
#endif

  return 0;
  }

#ifdef INTERNAL_EMS
  case 0x3e:       /* DOS handle close */
    if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
      return 0;
    else {
      E_printf("EMS: closed EMM file!\n");
      NOCARRY;
      show_regs(__FILE__, __LINE__);
      return 1;
    }

  case 0x44:      /* DOS ioctl */
    if ((LWORD(ebx) != EMM_FILE_HANDLE) || !config.ems_size)
      return 0;

    switch (LO(ax)) {
    case 0:     /* get device info */
      E_printf("EMS: dos_ioctl getdevinfo emm.\n");
      LWORD(edx) = 0x80;
      NOCARRY;
      show_regs(__FILE__, __LINE__);
      return 1;
      break;
    case 7:     /* check output status */
      E_printf("EMS: dos_ioctl chkoutsts emm.\n");
      LO(ax) = 0xff;
      NOCARRY;
      show_regs(__FILE__, __LINE__);
      return 1;
      break;
    }
  error("ERROR: dos_ioctl shouldn't get here. XXX\n");
  return 0;
#endif

  case 0x2C:                    /* get time & date */
    if (config.hogthreshold)    /* allow linux to idle */
      usleep(INT2F_IDLE_USECS);
    return 0;

  default:
    g_printf("INT21 (0x%02x):  we shouldn't be here! ax=0x%04x, bx=0x%04x\n",
	     nr, LWORD(eax), LWORD(ebx));
    return 0;
  }
  return 1;
}

void
run_int(int i)
{
  unsigned char *ssp;
  unsigned long sp;

  /* ssp = (unsigned char *)(REG(ss)<<4); */
  ssp = (unsigned char *)(READ_SEG_REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, vflags);
  /* pushw(ssp, sp, LWORD(cs)); */
  pushw(ssp, sp, READ_SEG_REG(cs));
  pushw(ssp, sp, LWORD(eip));
  LWORD(esp) -= 6;
  /* LWORD(cs) = ((us *) 0)[(i << 1) + 1]; */
  WRITE_SEG_REG(cs, ((us *) 0)[(i << 1) + 1]);
  LWORD(eip) = ((us *) 0)[i << 1];

  /* clear TF (trap flag, singlestep), VIF/IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   * NOTE: IF-flag only, because we are not sure that we will test it in
   *       some of our own software (...we all are human beeings)
   *       For vm86() 'VIF' is the candidate to reset in order to do CLI !
   */
  WRITE_FLAGSE(READ_FLAGSE() & ~(VIF | TF | IF | NT));
}

int can_revector(int i)
{
/* here's sort of a guideline:
 * if we emulate it completely, but there is a good reason to stick
 * something in front of it, and it seems to work, by all means revector it.
 * if we emulate none of it, say yes, as that is a little bit faster.
 * if we emulate it, but things don't seem to work when it's revectored,
 * then don't let it be revectored.
 */

  switch (i) {
  case 0: case 4: case 5: case 7:
#if 1 /* temp fix for bug in kernel vm86plus code
       * (fixing kernel patch is in 2.1.27 and will appear in 2.0.30, as Linus promised)
       * we will remove this, when time goes by --Hans 970225
       */
    if (running_kversion > 2001026 || (running_kversion > 2000029 && running_kversion < 2000000) )
      return NO_REVECT;
#endif
  case 0x21:			/* we want it first...then we'll pass it on */
#ifdef DJGPP_HACK
  case 0x23:			/* TMP FIX for ^C under DPMI */
#endif
  case 0x28:                    /* keyboard idle interrupt */
  case 0x2f:			/* needed for XMS, redirector, and idling */
  case 0xe6:			/* for redirector and helper (was 0xfe) */
  case 0xe7:			/* for mfs FCB helper */
  case 0xe8:			/* for int_queue_run return */
    return REVECT;

  case 0x74:			/* needed for PS/2 Mouse */
    if ((mice->type == MOUSE_PS2) || (mice->intdrv))
      return REVECT;
    else
      return NO_REVECT;

  case 0x33:			/* Mouse. Wrapper for mouse-garrot as well*/
    if (mice->intdrv || config.hogthreshold)
      return REVECT;
    else
      return NO_REVECT;

#if 0		/* no need to specify all */
  case 0x10:			/* BIOS video */
  case 0x13:			/* BIOS disk */
  case 0x15:
  case 0x16:			/* BIOS keyboard */
  case 0x17:			/* BIOS printer */
  case 0x1b:			/* ctrl-break handler */
  case 0x1c:			/* user timer tick */
  case 0x20:			/* exit program */
  case 0x25:			/* absolute disk read, calls int 13h */
  case 0x26:			/* absolute disk write, calls int 13h */
  case 0x27:			/* TSR */
  case 0x2a:
  case 0x60:
  case 0x61:
  case 0x62:
  case 0x67:			/* EMS */
  case 0xfe:			/* Turbo Debugger and others... */
#endif
  default:
    return NO_REVECT;
  }
}

static int can_revector_int21(int i)
{
  switch (i) {
  case 0x2c:          /* get time */
#ifdef INTERNAL_EMS
  case 0x3e:          /* dos handle close */
  case 0x44:          /* dos ioctl */
#endif
    return REVECT;

  case 0x3d:          /* dos handle open */
    if (config.emusys || config.emubat)
      return REVECT;
    else
      return NO_REVECT;

  default:
    return NO_REVECT;      /* don't emulate most int 21h functions */
  }
}

static void int05(u_char i) {
    g_printf("BOUNDS exception\n");
    default_interrupt(i);
    return;
}

void int_a_b_c_d_e_f(u_char i) {
    g_printf("IRQ->interrupt %x\n", i);
    show_regs(__FILE__, __LINE__);
    default_interrupt(i);
    return;
}

/* IRQ1, keyb data ready */
static void int09(u_char i) {
    fprintf(stderr, "IRQ->interrupt %x\n", i);
    run_int(0x09);
    return;
}

/* CONFIGURATION */
static void int11(u_char i) {
    LWORD(eax) = configuration;
    return;
}

/* MEMORY */
static void int12(u_char i) {
    LWORD(eax) = config.mem_size;
    return;
}

/* KEYBOARD */
static void int16(u_char i) {
    run_int(0x16);
    return;
}

/* BASIC */
static void int18(u_char i) {
  k_printf("BASIC interrupt being attempted.\n");
}

/* LOAD SYSTEM */
static void int19(u_char i) {
  boot();
}

/* MS-DOS */
static void int21(u_char i) {
  if (!ms_dos(HI(ax)))
    default_interrupt(i);
}

#ifdef DJGPP_HACK
/* Ctrl-C */
static void int23(u_char i)
{
  /* Had to revector here int0x23 under DPMI to solve the obnoxious
   * case of ^C under djgpp - actually my DOS (IBM 7.0) gets the ^C
   * and shuts down the program without telling it to dosemu :( - AV
   */
  if (in_dpmi)
	NOCARRY;
  else
	run_int(0x23);
  return; 
}
#endif

/* KEYBOARD BUSY LOOP */
static void int28(u_char i) {
  static int first = 1;
  if (first) {
    first = 0;
    if (!config.keybint && config.console_keyb) {
      /* revector int9, so dos doesn't play with the keybuffer */
      k_printf("revectoring int9 away from dos\n");
      SETIVEC(0x9, BIOSSEG, 16 * 0x8 + 2);	/* point to IRET */
    }
    if (mice->intdrv) {
      /* This code is dupped for now in base/mouse/mouse.c - JES 96/10/20 */
      #define Mouse_INT       (0x33 * 16)
      #define Mouse_INT74     (0x74 * 16)
      SETIVEC(0x33, Mouse_SEG, Mouse_INT);
      SETIVEC(0x74, Mouse_SEG, Mouse_INT74);
    }
  }

  if (config.hogthreshold) {
    /* the hogthreshold value just got redefined to be the 'garrot' value */
    static int time_count = 0;
    if (++time_count >= config.hogthreshold) {
      usleep(INT2F_IDLE_USECS);
      time_count = 0;
    }
  }

  default_interrupt(i);
}

/* FAST CONSOLE OUTPUT */
static void int29(u_char i) {
    /* char in AL */
  char_out(*(char *) &REG(eax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
}

static void int2f(u_char i)
{
  switch (LWORD(eax)) {
  case INT2F_IDLE_MAGIC:   /* magic "give up time slice" value */
    usleep(INT2F_IDLE_USECS);
    LWORD(eax) = 0;
    return;

#ifdef IPX
  case INT2F_DETECT_IPX:  /* TRB - detect IPX in int2f() */
    if (config.ipxsup && IPXInt2FHandler())
      return;
    break;
#endif
      
  case 0x1687:            /* Call for getting DPMI entry point */
    dpmi_get_entry_point();
    return;

  case 0x1683:
    LWORD (ebx) = 0;		/* W95: number of virtual machine */
  case 0x1681:		/* W95: enter critical section */
    if (in_dpmi && in_win31) {
	D_printf ("WIN: enter critical section\n");
	/* LWORD(eax) = 0;	W95 DDK says no return value */
	return;
    }
    break;
  case 0x1682:		/* W95: exit critical section */
    if (in_dpmi && in_win31) {
	D_printf ("WIN: exit critical section\n");
	/* LWORD(eax) = 0;	W95 DDK says no return value */
	return;
    }
    break;
    return;

  case 0x1686:            /* Are we in protected mode? */
    D_printf("DPMI CPU mode check in real mode.\n");
    LWORD(eax) = 0;	/* say ok */
    return;

  case 0x1600:		/* WINDOWS ENHANCED MODE INSTALLATION CHECK */
#if 1			/* it seens this confuse winos2 */
    if (in_dpmi && in_win31) {
      D_printf("WIN: WINDOWS ENHANCED MODE INSTALLATION CHECK\n");
      LWORD(eax) = 0x0a03;	/* let's try enhaced mode 3.1 :-))))))) */
      return;
    } else
      break;
#endif    
    break;
  case 0x1605:		/* Win95 Initialization Notification */
    LWORD(ecx) = 0xffff;	/* say it`s NOT ok to run under Win */
  case 0x1606:		/* Win95 Termination Notification */
  case 0x1607:		/* Win95 Device CallOut */
  case 0x1608:		/* Win95 Init Complete Notification */
  case 0x1609:		/* Win95 Begin Exit Notification */
    return;
  case 0x1684:		/* Win95 Get Device Entry Point */
    LWORD(edi) = 0;
    WRITE_SEG_REG(es, 0);	/* say NO to Win95 ;-) */
    return;
  case 0x1685:		/* Win95 Switch VM + Call Back */
    CARRY;
    LWORD(eax) = 1;
    return;

  case 0x160a:			/* IDENTIFY WINDOWS VERSION AND TYPE */
    if(in_dpmi && in_win31) {
      D_printf ("WIN: WINDOWS VERSION AND TYPE\n");
      LWORD(eax) =0;
      LWORD(ebx) = 0x030a;	/* 3.10 */
#if 1      
      LWORD(ecx) = 0x0003;	/* let\'s try enhaced mode */
#else
      LWORD(ecx) = 0x0002;	/* standard mode */
#endif      
      return;
    } else
      break;
  }

  switch (HI(ax)) {
  case 0x11:              /* redirector call? */
    if (mfs_redirector())
      return;
    break;

  case INT2F_XMS_MAGIC:
    if (!config.xms_size)
      break;
    switch (LO(ax)) {
    case 0:			/* check for XMS */
      x_printf("Check for XMS\n");
      xms_grab_int15 = 0;
      LO(ax) = 0x80;
      break;
    case 0x10:
      x_printf("Get XMSControl address\n");
      /* REG(es) = XMSControl_SEG; */
      WRITE_SEG_REG(es, XMSControl_SEG);
      LWORD(ebx) = XMSControl_OFF;
      break;
    default:
      x_printf("BAD int 0x2f XMS function:0x%02x\n", LO(ax));
    }
    return;
  }

  if IS_REDIRECTED(i)
    default_interrupt(i);
}

/* mouse */
static void int33(u_char i) {
/* New code introduced by Ed Sirett (ed@cityscape.co.uk)  26/1/95 to give 
 * garrot control when the dos app is polling the mouse and the mouse is 
 * taking a break. */
  
  static unsigned short int oldx=0, oldy=0, trigger=0;
   
/* Firstly do the actual mouse function. */   
/* N.B. This code only works with the intdrv since default_interrupt() does not
 * actually call the real mode mouse driver now. (It simply sets up the registers so
 * that when the signal that we are currently handling has completed and the kernel
 * reschedules dosemu it will then start executing the real mode mouse handler). :-( 
 * Do we need/have we got post_interrupt (IRET) handlers? 
 */
  if (mice->intdrv) 
    mouse_int();
  else 
    default_interrupt(i);
/* It seems that the only mouse sub-function that could be plausibly used to 
 * poll the mouse is AX=3 - get mouse buttons and position. 
 * The mouse driver should have left AX=3 unaltered during its call.
 * The correct responce should have the buttons in the low 3 bits in BX and 
 * x,y in CX,DX. 
 * Some programs seem to interleave calls to read mouse with various other 
 * sub-functions (Esp. 0x0b  0x05 and 0x06)
 * As a result we do not reset the  trigger value in these cases. 
 * Sadly, some programs use the user-specified mouse-event handler function (0x0c)
 * after which they then wait for mouse events presumably in a tight loop, I think
 * that we won't be able to stop these programs from burning CPU cycles.
 */
   if (LWORD(eax) ==0x0003)  {
     if (LWORD(ebx) == 0 && oldx == LWORD(ecx) && oldy == LWORD(edx) ) 
        trigger++;
      else  { 
        trigger=0;
        oldx = LWORD(ecx);
        oldy = LWORD(edx);
      } 
   }
m_printf("Called/ing the mouse with AX=%x \n",LWORD(eax));
/* Ok now we test to see if the mouse has been taking a break and we can let the 
 * system get on with some real work. :-) */
   if (config.hogthreshold && trigger >= config.hogthreshold)  {
     m_printf("Ignoring the quiet mouse.\n");
     usleep(INT2F_IDLE_USECS);
     trigger=0;
   }
}

#ifdef USING_NET
/* new packet driver interface */
static void int_pktdrvr(u_char i) {
  if (!pkt_int())
    default_interrupt(i);
}
#endif

/* dos helper and mfs startup (was 0xfe) */
static void inte6(u_char i) {
  if (!dos_helper())
    default_interrupt(i);
}

/* mfs FCB call */
static void inte7(u_char i) {
  SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
  run_int(0xe7);
}

/* End function for interrupt calls from int_queue_run() */
static void inte8(u_char i) {
  static unsigned short *csp;
  static int x;
  csp = SEG_ADR((us *), cs, ip) - 1;

  for (x=1; x<=int_queue_running; x++) {
    /* check if any int is finished */
    if ((int)csp == int_queue_head[x].int_queue_return_addr) {
      /* if it's finished - clean up by calling user cleanup fcn. */
      if (int_queue_head[x].int_queue_ptr.callend) {
	int_queue_head[x].int_queue_ptr.callend(int_queue_head[x].int_queue_ptr.interrupt);
	int_queue_head[x].int_queue_ptr.callend = NULL;
      }

      /* restore registers */
      REGS = int_queue_head[x].saved_regs;
      /* REG(eflags) |= VIF; */
      WRITE_FLAGS(READ_FLAGS() | VIF);
      
      h_printf("e8 int_queue: finished %x\n",
	       int_queue_head[x].int_queue_return_addr);
      *OUTB_ADD=1;
      if (int_queue_running == x) 
	int_queue_running--;
      return;
    }
  }
  h_printf("e8 int_queue: shouldn't get here\n");
  show_regs(__FILE__,__LINE__);
}

/*
 * DANG_BEGIN_FUNCTION DO_INT 
 *
 * description:
 * DO_INT is used to deal with interrupts returned to DOSEMU by the
 * kernel.
 *
 * DANG_END_FUNCTION
 */

void
do_int(int i)
{
  void (* caller_function)();

#if 0 /* 94/07/02 Just for reference */
  saytime("do_int");
#endif
#if 0 /* 94/07/02 Just for reference */
if (i== 0x2f) {
  show_regs(__FILE__, __LINE__);
  k_printf("Do INT0x%02x, eax=0x%04x, ebx=0x%04x ss=0x%04x esp=0x%04x cs=0x%04x ip=0x%04x CF=%d \n", i, LWORD(eax), LWORD(ebx), LWORD(ss), LWORD(esp), LWORD(cs), LWORD(eip), (int)REG(eflags) & CF);
}
#endif

  if ((LWORD(cs)) != BIOSSEG && IS_REDIRECTED(i) && can_revector(i)){
    run_int(i);
    return;
  }

  caller_function = interrupt_function[i];
  caller_function(i);
#if 1 
  /* This is a kludge to avoid immediate respawning of dosemu
   * if we have cs:ip pointing to the BIOS stubs' IRET
   * We do the IRET ourselves, so we directly let the DOSapp
   * control.
   * This perhaps avoids chained timer ints to accumulate
   * without giving the DOSapp a chance to do something.
   * (not sure if it works)
   * - Hans Lermen
   */ 
  if (LWORD(cs) == BIOSSEG) {
    unsigned char *csp;
    csp = SEG_ADR((unsigned char *),cs,ip);
    if ((*csp == 0xcf)) {
      unsigned char *ssp = (unsigned char *)(LWORD(ss)<<4);
      unsigned long sp = (unsigned long) LWORD(esp);
      LWORD(esp) +=6;
      LWORD(eip) = popw(ssp, sp);
      REG(cs) = popw(ssp, sp);
      WRITE_FLAGS(popw(ssp, sp));
      if (READ_FLAGS() & IF) WRITE_FLAGSE(READ_FLAGSE() | VIF);
      else  WRITE_FLAGSE((READ_FLAGSE() | IF) & ~VIF);
    }
  }
#endif
}

/*
 * Called to queue a hardware interrupt - will call "callstart"
 * just before the interrupt occurs and "callend" when it finishes
 */
void queue_hard_int(int i, void (*callstart), void (*callend))
{
  cli();

  int_queue[int_queue_end].interrupt = i;
  int_queue[int_queue_end].callstart = callstart;
  int_queue[int_queue_end].callend = callend;
  int_queue_end = (int_queue_end + 1) % IQUEUE_LEN;

  h_printf("int_queue: (%d,%d) ", int_queue_start, int_queue_end);

  i = int_queue_start;
  while (i != int_queue_end) {
    h_printf("%x ", int_queue[i].interrupt);
    i = (i + 1) % IQUEUE_LEN;
  }
  h_printf("\n");

  if (int_queue_start == int_queue_end)
    leavedos(56);
  sti();
}

/* Called by vm86() loop to handle queueing of interrupts */
void int_queue_run(void)
{
  static int current_interrupt;
  static unsigned char *ssp;
  static unsigned long sp;
  static u_char vif_counter=0; /* Incase someone don't clear things */

  if (int_queue_start == int_queue_end) {
#if 0
    REG(eflags) &= ~(VIP);
#endif
    return;
  }

  g_printf("Still using int_queue_run()\n");

  if (!*OUTB_ADD) {
    if (++vif_counter > 0x08) {
      I_printf("OUTB interrupts renabled after %d attempts\n", vif_counter);
    }
    else {
      REG(eflags) |= VIP;
      I_printf("OUTB_ADD = %d , returning from int_queue_run()\n", *OUTB_ADD);
      return;
    }
  }

  if (!(REG(eflags) & VIF)) {
    if (++vif_counter > 0x08) {
      I_printf("VIF interrupts renabled after %d attempts\n", vif_counter);
    }
    else {
      REG(eflags) |= VIP;
      I_printf("interrupts disabled while int_queue_run()\n");
      return;
    }
  }

  vif_counter=0;
  current_interrupt = int_queue[int_queue_start].interrupt;

  ssp = (unsigned char *) (REG(ss) << 4);
  sp = (unsigned long) LWORD(esp);

#ifndef NEW_KBD_CODE
  if (current_interrupt == 0x09) {
    k_printf("Int9 set\n");
    /* If another program does a keybaord read on port 0x60, we'll know */
    read_next_scancode_from_queue();
  }
#endif
   
  /* call user startup function...don't run interrupt if returns -1 */
  if (int_queue[int_queue_start].callstart) {
    if (int_queue[int_queue_start].callstart(current_interrupt) == -1) {
      fprintf(stderr, "Callstart NOWORK\n");
      return;
    }

    if (int_queue_running + 1 == NUM_INT_QUEUE)
      leavedos(55);

    int_queue_head[++int_queue_running].int_queue_ptr = int_queue[int_queue_start];
    int_queue_head[int_queue_running].in_use = 1;

    /* save our regs */
    int_queue_head[int_queue_running].saved_regs = REGS;

    /* push an illegal instruction onto the stack */
    /*  pushw(ssp, sp, 0xffff); */
    pushw(ssp, sp, 0xe8cd);

    /* this is where we're going to return to */
    int_queue_head[int_queue_running].int_queue_return_addr = (unsigned long) ssp + sp;
    pushw(ssp, sp, vflags);
    /* the code segment of our illegal opcode */
    pushw(ssp, sp, int_queue_head[int_queue_running].int_queue_return_addr >> 4);
    /* and the instruction pointer */
    pushw(ssp, sp, int_queue_head[int_queue_running].int_queue_return_addr & 0xf);
    LWORD(esp) -= 8;
  } else {
    pushw(ssp, sp, vflags);
    /* the code segment of our iret */
    pushw(ssp, sp, LWORD(cs));
    /* and the instruction pointer */
    pushw(ssp, sp, LWORD(eip));
    LWORD(esp) -= 6;
  }

  if (current_interrupt < 0x10)
    *OUTB_ADD = 0;
  else
    *OUTB_ADD = 1;

  LWORD(cs) = ((us *) 0)[(current_interrupt << 1) + 1];
  LWORD(eip) = ((us *) 0)[current_interrupt << 1];

  /* clear TF (trap flag, singlestep), IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   */
#if 0
  REG(eflags) &= ~(VIF | TF | IF | NT);
#endif
  if (int_queue[int_queue_start].callstart)
    REG(eflags) |= VIP;

  int_queue_start = (int_queue_start + 1) % IQUEUE_LEN;
  h_printf("int_queue: running int %x if applicable, return_addr=%x\n",
	   current_interrupt,
	   int_queue_head[int_queue_running].int_queue_return_addr);
}

/*
 * DANG_BEGIN_FUNCTION setup_interrupts
 *
 * description:
 * SETUP_INTERRUPTS is used to initialize the interrupt_function
 * array which directs handling of interrupts in protected mode and
 * also initializes the base vector for interrupts in real mode.
 *
 * DANG_END_FUNCTION
 */

void setup_interrupts(void) {
  int i;
  unsigned char *ptr;
  ushort *seg, *off;

  /* init trapped interrupts called via jump */
  for (i = 0; i < 256; i++) {
#if 0 /* Rick's BUG FIXER */
    /* Go figure? */
    if (i == 115)
      continue;
#endif
    interrupt_function[i] = default_interrupt;
    if ((i & 0xf8) == 0x60) { /* user interrupts */
	SETIVEC(i, 0, 0);
    } else {
	SETIVEC(i, BIOSSEG, 16 * i);
    }
  }
  
  interrupt_function[5] = int05;
  interrupt_function[8] = int08;
  interrupt_function[9] = int09;
  interrupt_function[0xa] = int_a_b_c_d_e_f;
  interrupt_function[0xb] = int_a_b_c_d_e_f;
  interrupt_function[0xc] = int_a_b_c_d_e_f;
  interrupt_function[0xd] = int_a_b_c_d_e_f;
  interrupt_function[0xe] = int_a_b_c_d_e_f;
  interrupt_function[0xf] = int_a_b_c_d_e_f;
  interrupt_function[0x10] = int10;
  interrupt_function[0x11] = int11;
  interrupt_function[0x12] = int12;
  interrupt_function[0x13] = int13;
  interrupt_function[0x14] = int14;
  interrupt_function[0x15] = int15;
  interrupt_function[0x16] = int16;
  interrupt_function[0x17] = int17;
  interrupt_function[0x18] = int18;
  interrupt_function[0x19] = int19;
  interrupt_function[0x1a] = int1a;
  interrupt_function[0x21] = int21;
#ifdef DJGPP_HACK
  interrupt_function[0x23] = int23;
#endif
  interrupt_function[0x28] = int28;
  interrupt_function[0x29] = int29;
  interrupt_function[0x2f] = int2f;
  interrupt_function[0x33] = int33;
#ifdef USING_NET
  interrupt_function[0x60] = int_pktdrvr;
#endif
  interrupt_function[0xe6] = inte6;
  interrupt_function[0xe7] = inte7;
  interrupt_function[0xe8] = inte8;

  /* Let kernel handle this, no need to return to DOSEMU */
  SETIVEC(0x1c, 0xf010, 0xc0);

  /* show EMS as disabled */
  SETIVEC(0x67, 0, 0);

  if (mice->intdrv) {
    /* this is the mouse handler */
    ptr = (unsigned char *) (Mouse_ADD+12);
    off = (u_short *) (Mouse_ADD + 8);
    seg = (u_short *) (Mouse_ADD + 10);
    /* tell the mouse driver where we are...exec add, seg, offset */
    mouse_sethandler(ptr, seg, off);
    SETIVEC(0x74, Mouse_SEG, Mouse_ROUTINE_OFF);
    SETIVEC(0x33, Mouse_SEG, Mouse_ROUTINE_OFF);
  }
  else
    *(unsigned char *) (BIOSSEG * 16 + 16 * 0x33) = 0xcf;	/* IRET */

  SETIVEC(0x16, INT16_SEG, INT16_OFF);
  SETIVEC(0x09, INT09_SEG, INT09_OFF);
  SETIVEC(0x08, INT08_SEG, INT08_OFF);

  install_int_10_handler();	/* Install the handler for video-interrupt */

  /* This is an int e7 used for FCB opens */
  SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
  /* End of int 0xe7 for FCB opens */

  /* set up relocated video handler (interrupt 0x42) */
  if (config.dualmon == 2) {
    interrupt_function[0x42] = interrupt_function[0x10];
  }
  else *(u_char *) 0xff065 = 0xcf;	/* IRET */
}

/*
 * DANG_BEGIN_FUNCTION int_vector_setup
 *
 * description:
 * Setup initial interrupts which can be revectored so that the kernel
 * does not need to return to DOSEMU if such an interrupt occurs.
 *
 * DANG_END_FUNCTION
 */

void int_vector_setup(void)
{
  int i;

  /* set up the redirection arrays */
#ifdef __linux__
  memset(&vm86s.int_revectored, 0x00, sizeof(vm86s.int_revectored));
  memset(&vm86s.int21_revectored, 0x00, sizeof(vm86s.int21_revectored));

  for (i=0; i<0x100; i++)
    if (can_revector(i)==REVECT && i!=0x21)
      set_revectored(i, &vm86s.int_revectored);

  for (i=0; i<0x100; i++)
    if (can_revector_int21(i)==REVECT)
      set_revectored(i, &vm86s.int21_revectored);
#endif
#ifdef __NetBSD__
  memset(&vm86s.int_byuser[0], 0x00, sizeof(vm86s.int_byuser));
  memset(&vm86s.int21_byuser[0], 0x00, sizeof(vm86s.int21_byuser));

  for (i=0; i<0x100; i++)
    if (can_revector(i)==REVECT && i!=0x21)
      set_revectored(i, vm86s.int_byuser);

  for (i=0; i<0x100; i++)
    if (can_revector_int21(i)==REVECT)
      set_revectored(i, vm86s.int21_byuser);
#endif
}

#ifdef USE_MRP_JOYSTICK
void mrp_read_joystick(void)
{
  /* This only reads the first joystick and doesn't handle the buttons. */
  /* You will need to install the joystick module for this to work. */
  static int fd =0;
  static const char * const fname = "/dev/js0" ;
  int status;
  struct JS_DATA_TYPE js;

  if (fd == 0) {
    fd = open (fname, O_RDONLY);
    if (fd < 0)
      perror ("mrp_read_joystick (open)");
  }
  if (fd < 0) {
    CARRY; /* fail */
    return;
  }

  status = read (fd, &js, JS_RETURN);
  if (status != JS_RETURN) {
    perror ("mrp_read_joystick (read)");
    fd = -1;
    CARRY; /* fail */
    return;
  }
  LWORD(eax) = js.x;
  LWORD(ebx) = js.y;
  LWORD(ecx) = 0;		/* second joystick x */
  LWORD(edx) = 0;		/* second joystick y */
  NOCARRY;
  return;
}
#endif
