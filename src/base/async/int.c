/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */


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
#include "serial.h"
#include "memory.h"
#include "timers.h"
#include "mouse.h"
#include "disks.h"
#include "bios.h"
#include "iodev.h"
#include "pic.h"
#include "lpt.h"
#include "bitops.h"
#include "hma.h"
#include "xms.h"
#include "int.h"
#include "dos2linux.h"
#include "video.h"
#include "vc.h"
#include "priv.h"
#include "doshelpers.h"
#include "utilities.h"
#include "redirect.h"
#include "pci.h"
#include "joystick.h"
#include "aspi.h"
#include "vgaemu.h"
#include "hlt.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

#ifdef USING_NET
#include "ipx.h"
#endif
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

#include "dpmi.h"

#include "keyb_server.h"

#include "userhook.h"

#undef  DEBUG_INT1A

static char win31_title[256];

static void dos_post_boot(void);
static int int33(void);

typedef int interrupt_function_t(void);
static interrupt_function_t *interrupt_function[0x100][2];

static char *dos_io_buffer;
static int dos_io_buffer_size = 0;

static u_char         save_hi_ints[128];

/* set if some directories are mounted during startup */
int redir_state = 0;

static char title_hint[9] = "";
static char title_current[TITLE_APPNAME_MAXLEN];
static int can_change_title = 0;

static void change_window_title(char *title)
{
   if (Video->change_config)
      Video->change_config(CHG_TITLE_APPNAME, title);
} 

static void kill_time(long usecs) {
   hitimer_t t_start;

   t_start = GETusTIME(0);
   while (GETusTIME(0) - t_start < usecs) {
	sigset_t mask;
	sigemptyset(&mask);
	sigsuspend(&mask);
	do_periodic_stuff();
   }
}

static void process_master_boot_record(void)
{
  /* Ok, _we_ do the MBR code in 32-bit C code,
   * so this obviously is _not_ stolen from any DOS code ;-)
   *
   * Now, what _does_ the original MSDOS MBR?
   * 1. It sets DS,ES,SS to zero
   * 2. It sets the stack pinter to just below the loaded MBR (SP=0x7c00)
   * 3. It moves itself down to 0:0x600
   * 4. It searches for a partition having the bootflag set (=0x80)
   * 5. It loads the bootsector of this partition to 0:0x7c00
   * 6. It does a long jump to 0:0x7c00, with following registers set:
   *    DS,ES,SS = 0
   *    BP,SI pointing to the partition entry within 0:600 MBR
   *    DI = 0x7dfe
   */
   struct partition {
     unsigned char bootflag;
     unsigned char start_head;
     unsigned char start_sector;
     unsigned char start_track;
     unsigned char OS_type;
     unsigned char end_head;
     unsigned char end_sector;
     unsigned char end_track;
     unsigned int num_sect_preceding;
     unsigned int num_sectors;
   } __attribute__((packed));
   struct mbr {
     char code[0x1be];
     struct partition partition[4];
     unsigned short bootmagic;
   } __attribute__((packed));
   struct mbr *mbr = LOWMEM(0x600);
   struct mbr *bootrec = LOWMEM(0x7c00);
   int i;

   memcpy(mbr, bootrec, 0x200);	/* move the mbr down */

   for (i=0; i<4; i++) {
     if (mbr->partition[i].bootflag == 0x80) break;
   }
   if (i >=4) {
     /* aiee... no bootflags sets */
     p_dos_str("\n\rno bootflag set, Leaving DOS...\n\r");
     leavedos(99);
   }
   LWORD(cs) = LWORD(ds) = LWORD(es) = LWORD(ss) =0;
   LWORD(esp) = 0x7c00;
   LO(dx) = 0x80;  /* drive C:, DOS boots only from C: */
   HI(dx) = mbr->partition[i].start_head;
   LO(cx) = mbr->partition[i].start_sector;
   HI(cx) = mbr->partition[i].start_track;
   LWORD(eax) = 0x0201;  /* read one sector */
   LWORD(ebx) = 0x7c00;  /* target offset, ES is 0 */
   int13();   /* we simply call our INT13 routine, hence we will not have
                 to worry about future changements to this code */
   if ((REG(eflags) & CF) || (bootrec->bootmagic != 0xaa55)) {
     /* error while booting */
     p_dos_str("\n\rerror on reading bootsector, Leaving DOS...\n\r");
     leavedos(99);
   }
   LWORD(edi)= 0x7dfe;
   LWORD(eip) = 0x7c00;
   LWORD(ebp) = LWORD(esi) = 0x600 + offsetof(struct mbr, partition[i]);
}

static int inte6(void)
{
  return dos_helper();
}

/* returns 1 if dos_helper() handles it, 0 otherwise */
/* dos helper and mfs startup (was 0xfe) */
int dos_helper(void)
{
  switch (LO(ax)) {
  case DOS_HELPER_DOSEMU_CHECK:			/* Linux dosemu installation test */
    LWORD(eax) = 0xaa55;
    LWORD(ebx) = VERSION * 0x100 + SUBLEVEL; /* major version 0.49 -> 0049 */
    /* The patch level in the form n.n is a float...
     * ...we let GCC at compiletime translate it to 0xHHLL, HH=major, LL=minor.
     * This way we avoid usage of float instructions.
     */
    LWORD(ecx) = ((((int)(PATCHLEVEL * 100))/100) << 8) + (((int)(PATCHLEVEL * 100))%100);
    LWORD(edx) = (config.X)? 0x1:0;  /* Return if running under X */
    g_printf("WARNING: dosemu installation check\n");
    show_regs(__FILE__, __LINE__);
    break;

  case DOS_HELPER_SHOW_REGS:     /* SHOW_REGS */
    show_regs(__FILE__, __LINE__);
    break;

  case DOS_HELPER_SHOW_INTS:	/* SHOW INTS, BH-BL */
    show_ints(HI(bx), LO(bx));
    break;

  case DOS_HELPER_PRINT_STRING:	/* PRINT STRING ES:DX */
    g_printf("DOS likes us to print a string\n");
    ds_printf("DOS to EMU: \"%s\"\n",SEG_ADR((char *), es, dx));
    break;



  case DOS_HELPER_ADJUST_IOPERMS:  /* SET IOPERMS: bx=start, cx=range,
				   carry set for get, clear for release */
  {
    int cflag = LWORD(eflags) & CF ? 1 : 0;

    i_printf("I/O perms: 0x%04x 0x%04x %d\n", LWORD(ebx), LWORD(ecx), cflag);
    if (set_ioperm(LWORD(ebx), LWORD(ecx), cflag)) {
      error("SET_IOPERMS request failed!!\n");
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

  case DOS_HELPER_CONTROL_VIDEO:	/* initialize video card */
    if (LO(bx) == 0) {
      if (set_ioperm(0x3b0, 0x3db - 0x3b0, 0))
	warn("couldn't shut off ioperms\n");
      SETIVEC(0x10, BIOSSEG, INT_OFF(0x10));	/* restore our old vector */
      config.vga = 0;
    } else {
      unsigned char *ssp;
      unsigned long sp;

      if (!config.mapped_bios) {
	error("CAN'T DO VIDEO INIT, BIOS NOT MAPPED!\n");
	return 1;
      }
      if (set_ioperm(0x3b0, 0x3db - 0x3b0, 1))
	warn("couldn't get range!\n");
      config.vga = 1;
      set_vc_screen_page();
      warn("WARNING: jumping to 0[c/e]000:0003\n");

      ssp = SEG2LINEAR(REG(ss));
      sp = (unsigned long) LWORD(esp);
      pushw(ssp, sp, LWORD(cs));
      pushw(ssp, sp, LWORD(eip));
      LWORD(esp) -= 4;
      LWORD(cs) = config.vbios_seg;
      LWORD(eip) = 3;
      show_regs(__FILE__, __LINE__);
    }

  case DOS_HELPER_SHOW_BANNER:		/* show banner */
    if (!config.console_video)
      install_dos(1);
    if (!config.dosbanner)
      break;
    p_dos_str("\n\nLinux DOS emulator " VERSTR " $" "Date: " VERDATE "$\n");
    p_dos_str("Last configured at %s on %s\n", CONFIG_TIME, CONFIG_HOST);
#if 1 
    p_dos_str("This is work in progress.\n");
    p_dos_str("Please test against a recent version before reporting bugs and problems.\n");
    /* p_dos_str("Formerly maintained by Robert Sanders, gt8134b@prism.gatech.edu\n\n"); */
    p_dos_str("Submit Bug Reports, Patches & New Code to linux-msdos@vger.kernel.org or via\n");
    p_dos_str("the SourceForge tracking system at http://www.sourceforge.net/projects/dosemu\n\n");
#endif
    if (config.dpmi)
      p_dos_str("DPMI-Server Version 0.9 installed\n\n");
    break;

   case DOS_HELPER_INSERT_INTO_KEYBUFFER:
      k_printf("KBD: WARNING: outdated keyboard helper fn 6 was called!\n");
      break;

   case DOS_HELPER_GET_BIOS_KEY:                /* INT 09 "get bios key" helper */
      _AX=get_bios_key(_AH);
      k_printf("HELPER: get_bios_key() returned %04x\n",_AX);
      break;
     
  case DOS_HELPER_VIDEO_INIT:
    v_printf("Starting Video initialization\n");
    /* DANG_BEGIN_REMARK
     * Many video BIOSes use hi interrupt vector locations as
     * scratchpad area - this is because they come before DOS and feel
     * safe to do it. But we are initializing vectors before video, so
     * this only causes trouble. I assume no video BIOS will ever:
     * - change vectors < 0xe0 (0:380-0:3ff area)
     * - change anything in the vector area _after_ installation - AV
     * DANG_END_REMARK
     */
    v_printf("Save hi vector area\n");
    MEMCPY_2UNIX(save_hi_ints,0x380,128);
    _AL = config.vbios_post;
    break;

  case DOS_HELPER_VIDEO_INIT_DONE:
    v_printf("Finished with Video initialization\n");
    MEMCPY_2DOS(0x380,save_hi_ints,128);
    config.emuretrace <<= 1;
    emu_video_retrace_on();
    break;

  case DOS_HELPER_GET_DEBUG_STRING:
    /* TRB - handle dynamic debug flags in dos_helper() */
    LWORD(eax) = GetDebugFlagsHelper((char *) (((_regs.es & 0xffff) << 4) +
					       (_regs.edi & 0xffff)), 1);
    g_printf("DBG: Get flags\n");
    break;

  case DOS_HELPER_SET_DEBUG_STRING:
    g_printf("DBG: Set flags\n");
    LWORD(eax) = SetDebugFlagsHelper((char *) (((_regs.es & 0xffff) << 4) +
					       (_regs.edi & 0xffff)));
    g_printf("DBG: Flags set\n");
    break;

  case DOS_HELPER_SET_HOGTHRESHOLD:
    g_printf("IDLE: Setting hogthreshold value to %u\n", LWORD(ebx));
    config.hogthreshold = LWORD(ebx);
    break;

  case DOS_HELPER_MFS_HELPER:
    mfs_inte6();
    return 1;

  case DOS_HELPER_DOSC:
    if (HI(ax) == 0xdc) {
      /* install check and notify */
      if (!dosc_interface()) return 0;
      running_DosC = LWORD(ebx);
      return 1;
    }
    if (running_DosC) {
      return dosc_interface();
    }
    return 0;

  case DOS_HELPER_EMS_HELPER:
    ems_helper();
    return 1;

  case DOS_HELPER_EMS_BIOS:
  {
    unsigned char *ssp;
    unsigned long sp;

    ssp = SEG2LINEAR(REG(ss));
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

  case DOS_HELPER_XMS_HELPER:
    xms_helper();
    return 1;

  case DOS_HELPER_GARROT_HELPER:             /* Mouse garrot helper */
    if (!LWORD(ebx))   /* Wait sub-function requested */
      usleep(INT28_IDLE_USECS);
    else {             /* Get Hogthreshold value sub-function*/
      LWORD(ebx)= config.hogthreshold;
      LWORD(eax)= config.hogthreshold;
    }
    break;

  case DOS_HELPER_SERIAL_HELPER:   /* Serial helper */
    serial_helper();
    break;

  case DOS_HELPER_BOOTDISK:	/* set/reset use bootdisk flag */
    use_bootdisk = LO(bx) ? 1 : 0;
    break;

  case DOS_HELPER_MOUSE_HELPER:	/* set mouse vector */
    mouse_helper(&vm86s.regs);
    break;

  case DOS_HELPER_PAUSE_KEY:
    pic_set_callback(Pause_SEG, Pause_OFF);
    break;

  case DOS_HELPER_CDROM_HELPER:{
      E_printf("CDROM: in 0x40 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x, "
	       "cx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx), LWORD(ecx));
      cdrom_helper(NULL, NULL);
      break;
    }

  case DOS_HELPER_ASPI_HELPER: {
#ifdef ASPI_SUPPORT
      A_printf("ASPI: in 0x41 handler! ax=0x%04x, bx=0x%04x, dx=0x%04x, "
           "cx=0x%04x\n", LWORD(eax), LWORD(ebx), LWORD(edx), LWORD(ecx));
      aspi_helper(HI(ax));
#else
      LWORD(eax) = 0;
#endif
      break;
  }

  case DOS_HELPER_RUN_UNIX:
    g_printf("Running Unix Command\n");
    run_unix_command(SEG_ADR((char *), es, dx));
    break;   

  case DOS_HELPER_GET_USER_COMMAND:
    /* Get DOS command from UNIX in es:dx (a null terminated buffer) */
    g_printf("Locating DOS Command\n");
    LWORD(eax) = misc_e6_commandline(SEG_ADR((char *), es, dx));
    break;   

  case DOS_HELPER_GET_UNIX_ENV:
    /* Interrogate the UNIX environment in es:dx (a null terminated buffer) */
    g_printf("Interrogating UNIX Environment\n");
    LWORD(eax) = misc_e6_envvar(SEG_ADR((char *), es, dx));
    break;   

  case DOS_HELPER_0x53:
    {
        LWORD(eax) = run_system_command(SEG_ADR((char *), es, dx));
	break;
    }

  case DOS_HELPER_GET_CPU_SPEED:
    {
	if (config.rdtsc)
		REG(eax) = (LLF_US << 16) / config.cpu_spd;
	else	REG(eax) = 0;
	break;
    }

  case DOS_HELPER_GET_TERM_TYPE:
    {
	int i;

	/* NOTE: we assume terminal/video init has completed before coming here */
	if (config.X) i = 2;			/* X keyboard */
	else if (config.console_keyb) i = 0;	/* raw keyboard */
	else i = 1;				/* Slang keyboard */

	if (config.console_video) i |= 0x10;
	if (config.vga)           i |= 0x20;
	if (config.dualmon)       i |= 0x40;
	LWORD(eax) = i;
	break;
    }

    case DOS_HELPER_GETCWD:
        LWORD(eax) = (short)((uintptr_t)getcwd(SEG_ADR((char *), es, dx), (size_t)LWORD(ecx)));
        break;

    case DOS_HELPER_GETPID:
	LWORD(eax) = getpid();
	LWORD(ebx) = getppid();
	break;

    case DOS_HELPER_PUT_DATA: {
	unsigned int offs = REG(edi);
	unsigned int size = REG(ecx);
	unsigned char *dos_ptr = SEG_ADR((unsigned char *), ds, dx);
	if (offs + size <= dos_io_buffer_size)
	    MEMCPY_DOS2DOS(dos_io_buffer + offs, dos_ptr, size);
	break;
    }

    case DOS_HELPER_GET_DATA: {
	unsigned int offs = REG(edi);
	unsigned int size = REG(ecx);
	unsigned char *dos_ptr = SEG_ADR((unsigned char *), ds, dx);
	if (offs + size <= dos_io_buffer_size)
	    MEMCPY_DOS2DOS(dos_ptr, dos_io_buffer + offs, size);
	break;
    }

  case DOS_HELPER_CHDIR:
        LWORD(eax) = chdir(SEG_ADR((char *), es, dx));
        break;
#ifdef X86_EMULATOR
  case DOS_HELPER_CPUEMUON:
#ifdef TRACE_DPMI
	if (debug_level('t')==0)
#endif
	{
#ifdef DONT_DEBUG_BOOT
	    memcpy(&debug,&debug_save,sizeof(debug));
#endif
	    /* we could also enter from inside dpmi, provided we already
	     * mirrored the LDT into the emu's own one */
	    if ((config.cpuemu==1) && !in_dpmi) enter_cpu_emu();
  	}
        break;
  case DOS_HELPER_CPUEMUOFF:
	if ((config.cpuemu>1) 
#ifdef TRACE_DPMI
	&& (debug_level('t')==0) 
#endif
	&& !in_dpmi)
	    leave_cpu_emu();
        break;
#endif
    case DOS_HELPER_XCONFIG:
	if (Video->change_config) {
		LWORD(eax) = Video->change_config((unsigned) LWORD(edx), SEG_ADR((void *), es, bx));
	} else {
		_AX = -1;
	}
        break;
  case DOS_HELPER_BOOTSECT: {
      fdkernel_boot_mimic();
      break;
    }
  case DOS_HELPER_MBR:
    if (LWORD(eax) == 0xfffe) {
      process_master_boot_record();
      break;
    }
    LWORD(eax) = 0xffff;
    /* ... and fall through */
  case DOS_HELPER_EXIT:
    if (LWORD(eax) == DOS_HELPER_REALLY_EXIT) {
      /* terminate code is in bx */
      dbug_printf("DOS termination requested\n");
      if (config.cardtype != CARD_NONE)
	p_dos_str("\n\rLeaving DOS...\n\r");
      leavedos(LO(bx));
    }
    break;

  /* here we try to hook a possible plugin */
  #include "plugin_inte6.h"

  default:
    error("bad dos helper function: AX=0x%04x\n", LWORD(eax));
    return 0;
  }

  return 1;
}

static int int15(void)
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
      case 0x00: {		/* giveup timeslice */
        idle(0, 100, 0, INT15_IDLE_USECS, "topview");
        break;
      }
    }
    CARRY;
    break;
  case 0x41:			/* wait on external event */
    break;
  case 0x4f:			/* Keyboard intercept */
    HI(ax) = 0x86;
    /*k_printf("INT15 0x4f CARRY=%x AX=%x\n", (LWORD(eflags) & CF),LWORD(eax));*/
    k_printf("INT15 0x4f CARRY=%x AX=%x\n", (READ_FLAGS() & CF),LWORD(eax));
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
  case 0x80:		/* default BIOS hook: device open */
  case 0x81:		/* default BIOS hook: device close */
  case 0x82:		/* default BIOS hook: program termination */
    HI(ax) = 0;
  case 0x83:
    h_printf("int 15h event wait:\n");
    show_regs(__FILE__, __LINE__);
    CARRY;
    break;			/* no event wait */
  case 0x84:
    joy_bios_read ();
    break;
  case 0x85:
    num = LWORD(eax) & 0xFF;	/* default bios handler for sysreq key */
    if (num == 0 || num == 1) {
      LWORD(eax) &= 0x00FF;
      break;
    }
    LWORD(eax) &= 0xFF00;
    LWORD(eax) |= 1;
    CARRY;
    break;
  case 0x86:
    /* wait...cx:dx=time in usecs */
    g_printf("doing int15 wait...ah=0x86\n");
    show_regs(__FILE__, __LINE__);
    kill_time((long)((LWORD(ecx) << 16) | LWORD(edx)));
    NOCARRY;
    break;

  case 0x87: {
    unsigned int *lp;
    unsigned long src_addr, dst_addr;
    unsigned long src_limit, dst_limit;
    unsigned int length;
    lp = SEG_ADR((int*), es, si);
    lp += 4;
    src_addr = (*lp >> 16) & 0x0000FFFF;
    src_limit = *lp & 0x0000FFFF;
    lp++;
    src_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
    src_limit |= (*lp & 0x000F0000);
    lp++;
    dst_addr = (*lp >> 16) & 0x0000FFFF;
    dst_limit = *lp & 0x0000FFFF;
    lp++;
    dst_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
    dst_limit |= (*lp & 0x000F0000);

    length = LWORD(ecx) << 1;

    x_printf("int 15: block move: src=%#lx dst=%#lx len=%#x\n",
      src_addr, dst_addr, length);

    if (src_limit < length - 1 || dst_limit < length - 1 ||
        src_addr + length > LOWMEM_SIZE + HMASIZE + EXTMEM_SIZE ||
        dst_addr + length > LOWMEM_SIZE + HMASIZE + EXTMEM_SIZE) {
      x_printf("block move failed\n");
      LWORD(eax) = 0x0200;
      CARRY;
    } else {
      unsigned int old_a20 = a20;
      /* Have to enable a20 before moving */
      if (!a20)
        set_a20(1);
      extmem_copy((void*)dst_addr, (void*)src_addr, length);
      if (old_a20 != a20)
        set_a20(old_a20);
      LWORD(eax) = 0;
      NOCARRY;
    }
    break;
   }

  case 0x88:
    if (config.xms_size)
      LWORD(eax) = 0;
    else
      LWORD(eax) = (EXTMEM_SIZE + HMASIZE) >> 10;
    NOCARRY;
    break;

  case 0x89:			/* enter protected mode : kind of tricky! */
    LWORD(eax) |= 0xFF00;		/* failed */
    CARRY;
    break;
  case 0x90:			/* no device post/wait stuff */
    CARRY;
    break;
  case 0x91:
    CARRY;
    break;
  case 0xbf:			/* DOS/16M,DOS/4GW */
    switch (REG(eax) &= 0x00FF)
      {
        case 0: case 1: case 2:		/* installation check */
        default:
          REG(edx) = 0;
          CARRY;
          break;
      }
    break;
  case 0xc0:
    LWORD(es) = ROM_CONFIG_SEG;
    LWORD(ebx) = ROM_CONFIG_OFF;
    HI(ax) = 0;
    break;
  case 0xc1:
    CARRY;
    break;			/* no ebios area */
  case 0xc2:
    mouse_ps2bios();
    break;
  case 0xc3:
    /* no watchdog */
    CARRY;
    break;
  case 0xc4:
    /* no post */
    CARRY;
    break;
  case 0xc9:
    if (LO(ax) == 0x10) {
	HI(ax) = 0;
	HI(cx) = vm86s.cpu_type;
	LO(cx) = 0x20;
	break;
    }
  /* else fall through */
  case 0x24:		/* PS/2 A20 gate support */
  case 0xd8:		/* EISA - should be set in config? */
  case 0xda:
  case 0xdb:
	HI(ax) = 0x86;
	break;

  case 0xe8:
#if 0
--------b-15E801-----------------------------
INT 15 - Phoenix BIOS v4.0 - GET MEMORY SIZE FOR >64M CONFIGURATIONS
	AX = E801h
Return: CF clear if successful
	    AX = extended memory between 1M and 16M, in K (max 3C00h = 15MB)
	    BX = extended memory above 16M, in 64K blocks
	    CX = configured memory 1M to 16M, in K
	    DX = configured memory above 16M, in 64K blocks
	CF set on error
Notes:	supported by the A03 level (6/14/94) and later XPS P90 BIOSes, as well
	  as the Compaq Contura, 3/8/93 DESKPRO/i, and 7/26/93 LTE Lite 386 ROM
	  BIOS
	supported by AMI BIOSes dated 8/23/94 or later
	on some systems, the BIOS returns AX=BX=0000h; in this case, use CX
	  and DX instead of AX and BX
	this interface is used by Windows NT 3.1, OS/2 v2.11/2.20, and is
	  used as a fall-back by newer versions if AX=E820h is not supported
SeeAlso: AH=8Ah"Phoenix",AX=E802h,AX=E820h,AX=E881h"Phoenix"
---------------------------------------------
#endif
    if (LO(ax) == 1) {
	Bit32u mem = (EXTMEM_SIZE + HMASIZE) >> 10;
	if (mem < 0x3c00) {
	  LWORD(eax) = mem;
	  LWORD(ebx) = 0;
	} else {
	  LWORD(eax) = 0x3c00;
	  LWORD(ebx) = ((mem - 0x3c00) >>6);
	}
	NOCARRY;
	break;
    } else if (REG(eax) == 0xe820 && REG(edx) == 0x534d4150) {
	REG(eax) = REG(edx);
	if (REG(ebx) < system_memory_map_size) {
	  REG(ecx) = max(REG(ecx), 20L);
	  if (REG(ebx) + REG(ecx) >= system_memory_map_size)
	    REG(ecx) = system_memory_map_size - REG(ebx);
	  MEMCPY_2DOS(MK_FP32(_ES, _DI), (char *)system_memory_map + REG(ebx),
		      REG(ecx));
	  REG(ebx) += REG(ecx);
	} else
	  REG(ecx) = 0;
	if (REG(ebx) >= system_memory_map_size)
	  REG(ebx) = 0;
	NOCARRY;
	break;
    }
    /* Fall through !! */

  default:
    g_printf("int 15h error: ax=0x%04x\n", LWORD(eax));
    CARRY;
    break;
  }
  return 1;
}

/* Set the DOS ticks value in BIOS area, then clear midnight flag */
static void set_ticks(unsigned long new_ticks)
{
  WRITE_DWORD(BIOS_TICK_ADDR, new_ticks);
  /* A timer read/write should reset the overflow flag */
  WRITE_BYTE(TICK_OVERFLOW_ADDR, 0);
  h_printf("TICKS: update ticks to %ld\n", new_ticks);
}

/*
 * DANG_BEGIN_FUNCTION int1a
 *
 * int 0x1A call
 *
 * This has (among other things) the calls that DOS makes to get/set its sense
 * of time. On booting, DOS gets the RTC time and date with AH=2 and AH=4,
 * after that it should use AH=0 calls to read the 'tick' counter from BIOS
 * memory. Each time this crosses midnight, a flag is set that DOS uses to
 * increment its date.
 *
 * Here we can now change the 'view' of time so the calls either return BIOS
 * tick (most DOS like), read the PIT counter (avoids INT-8 changes) or gets
 * LINUX time (most accurate for long term NTP-adjusted time keeping).
 *
 * DANG_END_FUNCTION
 */

static int int1a(void)
{
  switch (HI(ax)) {

/*
--------B-1A00-------------------------------
INT 1A - TIME - GET SYSTEM TIME
	AH = 00h
Return: CX:DX = number of clock ticks since midnight
	AL = midnight flag, nonzero if midnight passed since time last read
Notes:	there are approximately 18.2 clock ticks per second, 1800B0h per 24 hrs
	  (except on Tandy 2000, where the clock runs at 20 ticks per second)
	IBM and many clone BIOSes set the flag for AL rather than incrementing
	  it, leading to loss of a day if two consecutive midnights pass
	  without a request for the time (e.g. if the system is on but idle)
->	since the midnight flag is cleared, if an application calls this
->	  function after midnight before DOS does, DOS will not receive the
->	  midnight flag and will fail to advance the date

Note that DOSEMU's int8 (just like Bochs and some others, see bios.S)
increments AL so we *don't* lose a day if two consecutive midnights pass.
*/
  case 0:			/* read time counter */
   {
   int day_rollover;
   if(config.timemode == TM_LINUX)
   {
     /* Set BIOS area flags to LINUX time computed values always */
     last_ticks = get_linux_ticks(0, &day_rollover);
   }
   else if(config.timemode == TM_BIOS)
   {
    /* BIOSTIMER_ONLY_VIEW
     *
     * We rely on the INT8 routine doing the right thing,
     * DOS apps too rely on the relationship between INT1A and 0x46c timer.
     * We already do all appropriate things to trigger the simulated INT8
     * correctly (well, sometimes faking it), so the 0x46c timer incremented
     * by the (realmode) INT8 handler should be always in sync.
     * Therefore, we keep INT1A,AH0 simple instead of trying to be too clever;-)
     */
      static int first = 1;
      if (first)
      {
        /* take over the correct value _once_ only */
        last_ticks = (unsigned long)(pic_sys_time >> 16)
             + (sys_base_ticks + usr_delta_ticks);
        set_ticks(last_ticks);
        first = 0;
      }

      last_ticks = READ_DWORD(BIOS_TICK_ADDR);
      day_rollover = READ_BYTE(TICK_OVERFLOW_ADDR);
    }
    else /* (config.timemode == TM_PIT) assumed */
    {
    /* not BIOSTIMER_ONLY_VIEW
     * pic_sys_time is a zero-based tick (1.19MHz) counter. As such, if we
     * shift it right by 16 we get the number of PIT0 overflows, that is,
     * the number of 18.2ms timer ticks elapsed since starting dosemu. This
     * is independent of any int8 speedup a program can set, since the PIT0
     * counting frequency is fixed. The count overflows after 7 1/2 years.
     * usr_delta_ticks is 0 as long as nobody sets a new time (B-1A01)
     */
#ifdef DEBUG_INT1A
    g_printf("TIMER: sys_base_ticks=%ld usr_delta_ticks=%ld pic_sys_time=%#Lx\n",
    	sys_base_ticks, usr_delta_ticks, pic_sys_time);
#endif
    last_ticks = (unsigned long)(pic_sys_time >> 16);
    last_ticks += (sys_base_ticks + usr_delta_ticks);

    /* has the midnight passed? */
    day_rollover = last_ticks / TICKS_IN_A_DAY;
    last_ticks %= TICKS_IN_A_DAY;
    /* since pic_sys_time continues to increase, avoid further midnight overflows */
    sys_base_ticks -= day_rollover * TICKS_IN_A_DAY;
    }

    LWORD(eax) = day_rollover;
    LWORD(ecx) = (last_ticks >> 16) & 0xffff;
    LWORD(edx) = last_ticks & 0xffff;

#ifdef DEBUG_INT1A
    if (debug_level('g'))
    {
      time_t time_val;
      long k = last_ticks/18.2065;	/* sorry */
      time(&time_val);
      g_printf("INT1A: read timer = %ld (%ld:%ld:%ld) %s\n", last_ticks,
      	k/3600, (k%3600)/60, (k%60), ctime(&time_val));
    }
#else
    g_printf("INT1A: read timer=%ld, midnight=%d\n", last_ticks, LO(ax));
#endif
    set_ticks(last_ticks);	/* Write to BIOS_TICK_ADDR & clear TICK_OVERFLOW_ADDR */
    }
    break;

/*
--------B-1A01-------------------------------
INT 1A - TIME - SET SYSTEM TIME
	AH = 01h
	CX:DX = number of clock ticks since midnight
Return: nothing
Notes:	there are approximately 18.2 clock ticks per second, 1800B0h per 24 hrs
	  (except on Tandy 2000, where the clock runs at 20 ticks per second)
	this call resets the midnight-passed flag
SeeAlso: AH=00h,AH=03h,INT 21/AH=2Dh
*/
  case 1:			/* write time counter */
    if(config.timemode == TM_LINUX)
    {
      g_printf("INT1A: can't set DOS timer\n");	/* Allow time set except in 'LINUX view' case. */
    }
    else
    {
      /* get current system time and check it (previous usr_delta could be != 0) */
      long t;
      do {
        t = (pic_sys_time >> 16) + sys_base_ticks;
        if (t < 0) sys_base_ticks += TICKS_IN_A_DAY;
      } while (t < 0);

      /* get user-requested time */
      last_ticks = (LWORD(ecx) << 16) | (LWORD(edx) & 0xffff);

      usr_delta_ticks = last_ticks - t;

#ifdef DEBUG_INT1A
      g_printf("TIMER: sys_base_ticks=%ld usr_delta_ticks=%ld pic_sys_time=%#Lx\n",
    	sys_base_ticks, usr_delta_ticks, pic_sys_time);
#endif
      g_printf("INT1A: set timer to %ld\n", last_ticks);

      set_ticks(last_ticks);	/* Write to BIOS_TICK_ADDR & clear TICK_OVERFLOW_ADDR */
    }
    break;

/*
--------B-1A02-------------------------------
INT 1A - TIME - GET REAL-TIME CLOCK TIME (AT,XT286,PS)
	AH = 02h
Return: CF clear if successful
	    CH = hour (BCD)
	    CL = minutes (BCD)
	    DH = seconds (BCD)
	    DL = daylight savings flag (00h standard time, 01h daylight time)
	CF set on error (i.e. clock not running or in middle of update)
Note:	this function is also supported by the Sperry PC, which predates the
	  IBM AT; the data is returned in binary rather than BCD on the Sperry,
	  and DL is always 00h
SeeAlso: AH=00h,AH=03h,AH=04h,INT 21/AH=2Ch
*/
  case 2:			/* get time */
    if(config.timemode != TM_BIOS)
    {
      get_linux_ticks(1, NULL); /* Except BIOS view time, force RTC to LINUX time. */
    }
    HI(cx) = rtc_read(CMOS_HOUR);
    LO(cx) = rtc_read(CMOS_MIN);
    HI(dx) = rtc_read(CMOS_SEC);
    LO(dx) = 0;      /* No daylight saving - yuch */
    g_printf("INT1A: RTC time %02x:%02x:%02x\n",HI(cx),LO(cx),HI(dx));
    NOCARRY;
    break;

/*
--------B-1A03-------------------------------
INT 1A - TIME - SET REAL-TIME CLOCK TIME (AT,XT286,PS)
	AH = 03h
	CH = hour (BCD)
	CL = minutes (BCD)
	DH = seconds (BCD)
	DL = daylight savings flag (00h standard time, 01h daylight time)
Return: nothing
Note:	this function is also supported by the Sperry PC, which predates the
	  IBM AT; the data is specified in binary rather than BCD on the
	  Sperry, and the value of DL is ignored
*/
  case 3:			/* set time */
    if(config.timemode != TM_BIOS)
    {
      g_printf("INT1A: RTC: can't set time\n");
    }
    else
    {
    rtc_write(CMOS_HOUR, HI(cx));
    rtc_write(CMOS_MIN,  LO(cx));
    rtc_write(CMOS_SEC,  HI(dx));
    g_printf("INT1A: RTC set time %02x:%02x:%02x\n",HI(cx),LO(cx),HI(dx));
    }
    NOCARRY;
    break;
	  
/*
--------B-1A04-------------------------------
INT 1A - TIME - GET REAL-TIME CLOCK DATE (AT,XT286,PS)
	AH = 04h
Return: CF clear if successful
	    CH = century (BCD)
	    CL = year (BCD)
	    DH = month (BCD)
	    DL = day (BCD)
	CF set on error
SeeAlso: AH=02h,AH=04h"Sperry",AH=05h,INT 21/AH=2Ah,INT 4B/AH=02h"TI"
*/
  case 4:			/* get date */
    if(config.timemode != TM_BIOS)
    {
      get_linux_ticks(1, NULL);
    }
    HI(cx) = rtc_read(CMOS_CENTURY);
    LO(cx) = rtc_read(CMOS_YEAR);
    HI(dx) = rtc_read(CMOS_MONTH);
    LO(dx) = rtc_read(CMOS_DOM);
    /* REG(eflags) &= ~CF; */
    g_printf("INT1A: RTC date %04x%02x%02x (DOS format)\n", LWORD(ecx), HI(dx), LO(dx));
    NOCARRY;
    break;

/*
--------B-1A05-------------------------------
INT 1A - TIME - SET REAL-TIME CLOCK DATE (AT,XT286,PS)
	AH = 05h
	CH = century (BCD)
	CL = year (BCD)
	DH = month (BCD)
	DL = day (BCD)
Return: nothing
*/
  case 5:			/* set date */
    if(config.timemode != TM_BIOS)
    {
      g_printf("INT1A: RTC: can't set date\n");
    }
    else
    {
      rtc_write(CMOS_CENTURY, HI(cx));
      rtc_write(CMOS_YEAR,    LO(cx));
      rtc_write(CMOS_MONTH,   HI(dx));
      rtc_write(CMOS_DOM,     LO(dx));
      g_printf("INT1A: RTC set date %04x/%02x/%02x\n", LWORD(ecx), HI(dx), LO(dx));
    }
    NOCARRY;
    break;

  /* Notes: the alarm occurs every 24 hours until turned off, invoking INT 4A
  	each time the BIOS does not check for invalid values for the time, so
  	the CMOS clock chip's "don't care" setting (any values between C0h
  	and FFh) may be used for any or all three parts.  For example, to
  	create an alarm once a minute, every minute, call with CH=FFh, CL=FFh,
  	and DH=00h. (R.Brown)
   */
  case 6:			/* set alarm */
    {
      unsigned char h,m,s;

      if (rtc_read(CMOS_STATUSB) & 0x20) {
        CARRY;
      } else {
        rtc_write(CMOS_HOURALRM, (h=_CH));
        rtc_write(CMOS_MINALRM, (m=_CL));
        rtc_write(CMOS_SECALRM, (s=_DH));
        r_printf("RTC: set alarm to %02d:%02d:%02d\n",h,m,s);  /* BIN! */
        /* This has been VERIFIED on an AMI BIOS -- AV */
        rtc_write(CMOS_STATUSB, rtc_read(CMOS_STATUSB) | 0x20);
        NOCARRY;
      }
      break;
    }

  case 7:			/* clear alarm but NOT PIC mask */
    /* This has been VERIFIED on an AMI BIOS -- AV */
    rtc_write(CMOS_STATUSB, rtc_read(CMOS_STATUSB) & ~0x20);
    break;
 
  case 0xb1:			/* Intel PCI BIOS v 2.0c */
      pci_bios();
    break;

  default:
    g_printf("WARNING: unsupported INT0x1a call 0x%02x\n", HI(ax));
    CARRY;
  } /* End switch(HI(ax)) */

  return 1;
}

/* ========================================================================= */
/*
 * DANG_BEGIN_FUNCTION ms_dos
 *
 * int0x21 call
 *
 * we trap this for two functions: simulating the EMMXXXX0 device and
 * fudging the CONFIG.XXX and AUTOEXEC.XXX bootup files.
 *
 * note that the emulation herein may cause problems with programs
 * that like to take control of certain int 21h functions, or that
 * change functions that the true int 21h functions use.  An example
 * of the latter is ANSI.SYS, which changes int 10h, and int 21h
 * uses int 10h.  for the moment, ANSI.SYS won't work anyway, so it's
 * no problem.
 *
 * DANG_END_FUNCTION
 */
/* XXX - MAJOR HACK!!! this is bad bad wrong.  But it'll probably work
 * unless someone puts "files=200" in his/her config.sys
 */
#define EMM_FILE_HANDLE 200

/* MS-DOS */
/* see config.c: int21 is redirected here only when debug_level('D)>0 !! */

static int redir_it(void);

static unsigned short int21seg, int21off;

static void int21_post_boot(void)
{
  if (config.lfn) {
    int21seg = ISEG(0x21);
    int21off = IOFF(0x21);
    SETIVEC(0x21, BIOSSEG, INT_OFF(0x21));
  }
}

static int int21lfnhook(void)
{
  if (HI(ax) != 0x71 || !mfs_lfn())
    fake_int_to(int21seg, int21off);
  return 1;
}

static int msdos(void)
{
  ds_printf("INT21 (%d) at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
       redir_state, LWORD(cs), LWORD(eip),
       LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));

  if(redir_state && redir_it()) return 0;

#if 1
  if(HI(ax) == 0x3d) {
    char *p = MK_FP32(REG(ds), LWORD(edx));
    int i;

    ds_printf("INT21: open file \"");
    for(i = 0; i < 64 && p[i]; i++) ds_printf("%c", p[i]);
    ds_printf("\"\n");
  }
#endif

  switch (HI(ax)) {
  case 0x06:
    if (LO(dx) == 0xff)
      return 0;
    /* fallthrough */
  case 0x02:
  case 0x04:
  case 0x05:
  case 0x09:
  case 0x40:       /* output functions: reset idle */
    reset_idle(0);
    return 0;
  case 0x3d:       /* DOS handle open */
  case 0x6c:
#ifdef INTERNAL_EMS
    if (config.ems_size && !strncmp(ptr, "EMMXXXX0", 8)) {
      E_printf("EMS: opened EMM file!\n");
      LWORD(eax) = EMM_FILE_HANDLE;
      NOCARRY;
      show_regs(__FILE__, __LINE__);
      return 1;
    }
#endif
    subst_file_ext(SEG_ADR((char *), ds, dx));
    return 0;

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
  error("dos_ioctl shouldn't get here. XXX\n");
  return 0;
#endif

  case 0x2C: {                   /* get time & date */
      idle(2, 100, 0, INT2F_IDLE_USECS, "dos_time");
      return 0;
    }

  case 0x4B: {			/* program load */
      char *ptr, *tmp_ptr;
      char cmdname[256];
      char *str = SEG_ADR((char*), ds, dx);

#if WINDOWS_HACKS
      if ((ptr = strstr(str, "\\system\\dosx.exe")) ||
	  (ptr = strstr(str, "\\system\\win386.exe"))) {
        struct param4a *pa4 = SEG_ADR((struct param4a *), es, bx);
        struct lowstring *args = FARt_PTR(pa4->cmdline);
        int have_args = 0;
        strncpy(cmdname, args->s, args->len);
        cmdname[args->len] = 0;
        tmp_ptr = strstr(cmdname, "krnl386");
        if (!tmp_ptr)
          tmp_ptr = strstr(cmdname, "krnl286");
        if (!tmp_ptr)
          tmp_ptr = (ptr[8] == 'd' ? "krnl286" : "krnl386");
        else
          have_args = 1;
#if 1
	/* ignore everything and use krnl386.exe */
        memcpy(ptr+8, "krnl386", 7);
#else
        memcpy(ptr+8, tmp_ptr, 7);
#endif
        strcpy(ptr+8+7, ".exe");
        sprintf(win31_title, "Windows 3.1 in %c86 mode", tmp_ptr[4]);
        str = win31_title;
        win31_mode = tmp_ptr[4] - '0';
        if (have_args) {
          tmp_ptr = strchr(tmp_ptr, ' ');
          if (tmp_ptr) {
            strcpy(args->s, tmp_ptr);
            args->len -= tmp_ptr - cmdname;
          }
        }
	/* WinOS2 mouse driver calls this vector */
	/* INT_OFF(0x68) is where we have an IRET (hack) -
	   INT_OFF(0x66) didn't have one. */
	SETIVEC(0x66, BIOSSEG, INT_OFF(0x68));
      }
#endif

      if (!Video->change_config)
        return 0;
      if ((!title_hint[0] || strcmp(title_current, title_hint) != 0) &&
          str != win31_title)
        return 0;

      ptr = strrchr(str, '\\');
      if (!ptr) ptr = str;
      else ptr++;
      ptr += strspn(ptr, " \t");
      if (!ptr[0])
        return 0;
      tmp_ptr = ptr;
      while (*tmp_ptr) {	/* Check whether the name is valid */
        if (iscntrl(*tmp_ptr++))
          return 0;
      }
      strncpy(cmdname, ptr, TITLE_APPNAME_MAXLEN-1);
      cmdname[TITLE_APPNAME_MAXLEN-1] = 0;
      ptr = strchr(cmdname, '.');
      if (ptr && str != win31_title) *ptr = 0;
      /* change the title */
      strcpy(title_current, cmdname);
      change_window_title(title_current);
      can_change_title = 0;
      return 0;
  }

  default:
    return 0;
  }
  return 1;
}

static int int21(void)
{
  int ret = msdos();
  if (ret == 0 && !IS_REDIRECTED(0x21))
    return int21lfnhook();
  return ret;
}
/* ========================================================================= */

void real_run_int(int i)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = SEG2LINEAR(_SS);
  sp = (unsigned long) _SP;

  pushw(ssp, sp, read_FLAGS());
  pushw(ssp, sp, _CS);
  pushw(ssp, sp, _IP);
  _SP -= 6;
  _CS = ISEG(i);
  _IP = IOFF(i);

  /* clear TF (trap flag, singlestep), VIF/IF (interrupt flag), and
   * NT (nested task) bits of EFLAGS
   * NOTE: IF-flag only, because we are not sure that we will test it in
   *       some of our own software (...we all are human beings)
   *       For vm86() 'VIF' is the candidate to reset in order to do CLI !
   */
  if (vm86s.vm86plus.vm86dbg_TFpendig)
    set_TF();
  else
    clear_TF();
  clear_NT();
  clear_IF();
}

/* DANG_BEGIN_FUNCTION run_caller_func(i, revect)
 *
 * This function runs the specified caller function in response to an
 * int instruction.  Where i is the interrupt function to execute.
 *
 * revect specifies whether we call a non-revectored leaf interrupt function
 * or a "watcher" that sits in between:
 * the leaf interrupt function is called if cs:ip is at f000:i*10 or if
 *  (the int vector points there and the int is labelled non-revectored)
 * otherwise the non-leaf interrupt function is called, which may chain
 * through to the real interrupt function (if it returns 0)
 *
 * This function runs the instruction with the following model _CS:_IP is the
 * address to start executing at after the caller function terminates, and
 * _EFLAGS are the flags to use after termination.  For the simple case of an
 * int instruction this is easy.  _CS:_IP = retCS:retIP and _FLAGS = retFLAGS
 * as well equally the current values (retIP = curIP +2 technically).
 *
 * However if the function is called (from dos) by simulating an int instruction
 * (something that is common with chained interrupt vectors) 
 * _CS:_IP = BIOS_SEG:HLT_OFF(i) and _FLAGS = curFLAGS 
 * while retCS, retIP, and retFlags are on the stack.  These I pop and place in
 * the appropriate registers.  
 *
 * This functions actions certainly correct for functions executing an int/iret
 * discipline.  And almost certianly correct for functions executing an
 * int/retf#2 discipline (with flag changes), as an iret is always possilbe.
 * However functions like dos int 0x25 and 0x26 executing with a int/retf will
 * not be handled correctlty by this function and if you need to handle them
 * inside dosemu use a halt handler instead.
 *
 * Finally there is a possible trouble spot lurking in this code.  Interrupts
 * are only implicitly disabled when it calls the caller function, so if for
 * some reason the main loop would be entered before the caller function returns
 * wrong code may execute if the retFLAGS have interrupts enabled!  
 *
 * This is only a real handicap for sequences of dosemu code execute for long
 * periods of time as we try to improve timer response and prevent signal queue
 * overflows! -- EB 10 March 1997
 *
 * Grumble do to code that executes before interrupts, and the
 * semantics of default_interupt, I can't implement this function as I
 * would like.  In the tricky case of being called from dos by
 * simulating an int instruction, I must leave retCS, retIP, on the
 * stack.  But I can safely read retFlags so I do.  
 * I pop retCS, and retIP just before returning to dos, as well as
 * dropping the stack slot  that held retFlags.
 *
 * This improves consistency of interrupt handling, but not quite as
 * much as if I could handle it the way I would like.
 * -- EB 30 Nov 1997
 *
 * Trying to get it right now -- BO 25 Jan 2003
 *
 * This function returns 1 if it's completely finished (no need to run
 * real_run_int()), otherwise 0.
 *
 * DANG_END_FUNCTION
 */

static int run_caller_func(int i, int revect)
{
	interrupt_function_t *caller_function;
	g_printf("Do INT0x%02x: Using caller_function()\n", i);

	caller_function = interrupt_function[i][revect];
	if (caller_function) {
		return caller_function();
	} else {
		di_printf("int 0x%02x, ax=0x%04x\n", i, LWORD(eax));
		g_printf("DEFIVEC: int 0x%02x @ 0x%04x:0x%04x\n", i, ISEG(i), IOFF(i));
		/* This is here for old SIGILL's that modify IP */
		if (i == 0x00)
			LWORD(eip)+=2;
		return 0;
	}
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
  case 0x21:			/* we want it first...then we'll pass it on */
  case 0x28:                    /* keyboard idle interrupt */
  case 0x2f:			/* needed for XMS, redirector, and idling */
  case DOS_HELPER_INT:		/* e6 for redirector and helper (was 0xfe) */
  case 0xe7:			/* for mfs FCB helper */
    return REVECT;
  /* following 3 vectors must be revectored for DPMI */
  case 0x1c:			/* ROM BIOS timer tick interrupt */
  case 0x23:			/* DOS Ctrl+C interrupt */
  case 0x24:			/* DOS critical error interrupt */
    return config.dpmi ? REVECT : NO_REVECT;

  case 0x33:			/* Mouse. Wrapper for mouse-garrot as well*/
    /* hogthreshold may be changed using "speed". Easiest to leave it
       permanently enabled for now */
    return REVECT;

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
  case 0x02:
  case 0x04:
  case 0x05:
  case 0x06:
  case 0x09:
  case 0x40:          /* output functions: reset idle */
  case 0x2c:          /* get time */
#ifdef INTERNAL_EMS
  case 0x3e:          /* dos handle close */
  case 0x44:          /* dos ioctl */
#endif
  case 0x4b:          /* program load */
  case 0x4c:          /* program exit */
    return REVECT;

  case 0x3d:          /* dos handle open */
    if (config.emusys)
      return REVECT;
    else
      return NO_REVECT;

  default:
    return NO_REVECT;      /* don't emulate most int 21h functions */
  }
}

static void do_print_screen(void) {
int x_pos, y_pos;
int li = READ_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1) + 1;
int co = READ_WORD(BIOS_SCREEN_COLUMNS);
ushort *base=screen_adr(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
    g_printf("PrintScreen: base=%p, lines=%i columns=%i\n", base, li, co);
    printer_open(0);
    for (y_pos=0; y_pos < li; y_pos++) {
	for (x_pos=0; x_pos < co; x_pos++) 
	    printer_write(0, vga_read((unsigned char *)(base + y_pos*co + x_pos)));
	printer_write(0, 0x0d);
	printer_write(0, 0x0a);
    }
    printer_close(0);
}

static int int05(void) 
{
     /* FIXME does this test actually catch an unhandled bound exception */
    if( *SEG_ADR((Bit8u *), cs, ip) == 0x62 ) {	/* is this BOUND ? */
	    /* avoid deadlock: eip is not advanced! */
	    error("Unhandled BOUND exception!\n");
	    leavedos(54);
    }
    g_printf("INT 5: PrintScreen\n");
    do_print_screen();
    return 1;
}

/* CONFIGURATION */
static int int11(void) {
    LWORD(eax) = READ_WORD(BIOS_CONFIGURATION);
    return 1;
}

/* MEMORY */
static int int12(void) {
    LWORD(eax) = READ_WORD(BIOS_MEMORY_SIZE);
    return 1;
}

/* BASIC */
static int int18(void) {
  k_printf("BASIC interrupt being attempted.\n");
  return 1;
}

/* LOAD SYSTEM */
static int int19(void) {
  boot();
  return 1;
}


/*
 * Turn all simulated FAT devices into network drives.
 */
void redirect_devices(void)
{
  static char s[256] = "\\\\LINUX\\FS", *t = s + 10;
  int i, j;

  for (i = 0; i < MAX_HDISKS; i++) {
    if(hdisktab[i].type == DIR_TYPE && hdisktab[i].fatfs) {
      strncpy(t, hdisktab[i].dev_name, 245);
      s[255] = 0;
      j = RedirectDisk(i + 2, s, hdisktab[i].rdonly);

      ds_printf("INT21: redirecting %c: %s (err = %d)\n", i + 'C', j ? "failed" : "ok", j);
    }
  }
}

/*
 * Activate the redirector just before the first int 21h file open call.
 *
 * To use this feature, set redir_state = 1 and make sure int 21h is
 * revectored.
 */
static int redir_it(void)
{
  /*
   * Declaring the following struct volatile works around an EGCS bug
   * (at least up to egcs-2.91.66). Otherwise the line below marked
   * with '###' will modify the *old* (saved) struct too.
   * -- sw
   */
  volatile static struct vm86_regs save_regs;
  static unsigned x0, x1, x2, x3, x4;
  unsigned u;

  /*
   * To start up the redirector we need (1) the list of list, (2) the DOS version and
   * (3) the swappable data area. To get these, we reuse the original file open call.
   */
  switch(redir_state) {
    case 1:
      if(HI(ax) == 0x3d) {
        /*
         * FreeDOS will get confused by the following calling sequence (e.i. it
         * is not reentrant 'enough'. So we will abort here - it cannot use a
         * redirector anyway.
         * -- sw
         */
        if (running_DosC) {
          ds_printf("INT21: FreeDOS detected - no check for redirector\n");
          return redir_state = 0;
        }
        save_regs = REGS;
        redir_state = 2;
        LWORD(eip) -= 2;
        LWORD(eax) = 0x5200;		/* ### , see above EGCS comment! */
        ds_printf("INT21 +1 (%d) at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
          redir_state, LWORD(cs), LWORD(eip), LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));
        return 1;
      }
      break;

    case 2:
      x0 = LWORD(ebx); x1 = REG(es);
      redir_state = 3;
      LWORD(eip) -= 2;
      LWORD(eax) = 0x3000;
      ds_printf("INT21 +2 (%d) at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
        redir_state, LWORD(cs), LWORD(eip), LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));
      return 2;
      break;

    case 3:
      x4 = LWORD(eax);
      redir_state = 4;
      LWORD(eip) -= 2;
      LWORD(eax) = 0x5d06;
      ds_printf("INT21 +3 (%d) at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
        redir_state, LWORD(cs), LWORD(eip), LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));
      return 3;
      break;

    case 4:
      x2 = LWORD(esi); x3 = REG(ds);
      redir_state = 0;
      u = x0 + (x1 << 4);
      ds_printf("INT21: lol = 0x%x\n", u);
      ds_printf("INT21: sda = 0x%x\n", x2 + (x3 << 4));
      ds_printf("INT21: ver = 0x%02x\n", x4);

      if(READ_DWORD(u + 0x16)) {		/* Do we have a CDS entry? */
        /* Init the redirector. */
        LWORD(ecx) = x4;
        LWORD(edx) = x0; REG(es) = x1;
        LWORD(esi) = x2; REG(ds) = x3;
        LWORD(ebx) = 0x500;
        LWORD(eax) = 0x20;
        mfs_inte6();

        redirect_devices();
      }
      else {
        ds_printf("INT21: this DOS has no CDS entry - redirector not used\n");
      }

      REGS = save_regs;
      set_int21_revectored(-1);
      break;
  }

  return 0;
}

static int post_boot = 0;

void dos_post_boot_reset(void)
{
  post_boot = 0;
}

static void dos_post_boot(void)
{
  if (!post_boot) {
    post_boot = 1;
    mouse_post_boot();
    int21_post_boot();
  }
}

/* KEYBOARD BUSY LOOP */
static int int28(void) {
  idle(0, 50, 0, INT28_IDLE_USECS, "int28");
  return 0;
}

/* FAST CONSOLE OUTPUT */
static int int29(void) {
    /* char in AL */
  char_out(*(char *) &REG(eax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
  return 1;
}

static int int2f(void)
{
  reset_idle(0);
#if 1
  ds_printf("INT2F at %04x:%04x: AX=%04x, BX=%04x, CX=%04x, DX=%04x, DS=%04x, ES=%04x\n",
       LWORD(cs), LWORD(eip),
       LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx), LWORD(ds), LWORD(es));
#endif
  switch (LWORD(eax)) {
    case INT2F_IDLE_MAGIC: {  /* magic "give up time slice" value */
      idle(0, 100, 0, INT2F_IDLE_USECS, "int2f_idle_magic");
      LWORD(eax) = 0;
      return 1;
    }

#ifdef IPX
    case INT2F_DETECT_IPX:  /* TRB - detect IPX in int2f() */
      if (config.ipxsup && IPXInt2FHandler())
        return 1;
      break;
#endif

    case 0xae00: {
      char cmdname[TITLE_APPNAME_MAXLEN];
      char appname[TITLE_APPNAME_MAXLEN];
      struct lowstring *str = SEG_ADR((struct lowstring *), ds, si);
      u_short psp_seg;
      struct MCB *mcb;
      int len;
      char *ptr, *tmp_ptr;

      dos_post_boot();

      if (!Video->change_config)
        break;
      if (!sda)
        break;
      psp_seg = sda_cur_psp(sda);
      if (!psp_seg)
        break;
      mcb = (struct MCB *)SEG2LINEAR(psp_seg - 1);
      if (!mcb)
        break;
      strncpy(title_hint, mcb->name, 8);
      title_hint[8] = 0;
      len = min(str->len, (unsigned char)(TITLE_APPNAME_MAXLEN - 1));
      memcpy(cmdname, str->s, len);
      cmdname[len] = 0;
      ptr = cmdname + strspn(cmdname, " \t");
      if (!ptr[0])
	return 0;
      tmp_ptr = ptr;
      while (*tmp_ptr) {	/* Check whether the name is valid */
        if (iscntrl(*tmp_ptr++))
          return 0;
      }
      strcpy(title_current, title_hint);
      snprintf(appname, TITLE_APPNAME_MAXLEN, "%s ( %s )",
        title_current, strlower(ptr));
      change_window_title(appname);
      return 0;
    }
  }

  switch (HI(ax)) {
  case 0x11:              /* redirector call? */
    if (LO(ax) == 0x23) subst_file_ext(SEG_ADR((char *), ds, si));
    if (mfs_redirector()) return 1;
    break;

  case 0x15:
    if (mscdex()) return 1;
    break;

  case 0x16:		/* misc PM/Win functions */
    if (!config.dpmi) {
      break;		/* fall into real_run_int() */
    }
    switch (LO(ax)) {
      case 0x00:		/* WINDOWS ENHANCED MODE INSTALLATION CHECK */
    if (in_dpmi && in_win31) {
      D_printf("WIN: WINDOWS ENHANCED MODE INSTALLATION CHECK: %i\n", in_win31);
      if (win31_mode == 3)
        LWORD(eax) = 0x0a03;
      else
        LWORD(eax) = 0;
      return 1;
      }
    break;

      case 0x05:		/* Win95 Initialization Notification */
    LWORD(ecx) = 0xffff;	/* say it`s NOT ok to run under Win */
      case 0x06:		/* Win95 Termination Notification */
      case 0x07:		/* Win95 Device CallOut */
      case 0x08:		/* Win95 Init Complete Notification */
      case 0x09:		/* Win95 Begin Exit Notification */
    return 1;

      case 0x0a:			/* IDENTIFY WINDOWS VERSION AND TYPE */
    if(in_dpmi && in_win31) {
      D_printf ("WIN: WINDOWS VERSION AND TYPE\n");
      LWORD(eax) = 0;
      LWORD(ebx) = 0x030a;	/* 3.10 */
      LWORD(ecx) = win31_mode;
      return 1;
        }
      break;

      case 0x83:
        if (in_dpmi && in_win31)
            LWORD (ebx) = 0;	/* W95: number of virtual machine */
      case 0x81:		/* W95: enter critical section */
        if (in_dpmi && in_win31) {
	    D_printf ("WIN: enter critical section\n");
	    /* LWORD(eax) = 0;	W95 DDK says no return value */
	    return 1;
  }
      break;
      case 0x82:		/* W95: exit critical section */
        if (in_dpmi && in_win31) {
	    D_printf ("WIN: exit critical section\n");
	    /* LWORD(eax) = 0;	W95 DDK says no return value */
	    return 1;
  }
        break;

      case 0x84:		/* Win95 Get Device Entry Point */
        LWORD(edi) = 0;
        WRITE_SEG_REG(es, 0);	/* say NO to Win95 ;-) */
        return 1;
      case 0x85:		/* Win95 Switch VM + Call Back */
        CARRY;
        LWORD(eax) = 1;
        return 1;

      case 0x86:            /* Are we in protected mode? */
        D_printf("DPMI CPU mode check in real mode.\n");
        if (in_dpmi && !in_dpmi_dos_int) /* set AX to zero only if program executes in protected mode */
	    LWORD(eax) = 0;	/* say ok */
		 /* else let AX untouched (non-zero) */
      return 1;

      case 0x87:            /* Call for getting DPMI entry point */
	dpmi_get_entry_point();
	return 1;
    }
    break;

  case INT2F_XMS_MAGIC:
    if (!config.xms_size)
      break;
    switch (LO(ax)) {
    case 0:			/* check for XMS */
      x_printf("Check for XMS\n");
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
    return 1;
  }

  return !IS_REDIRECTED(0x2f);
}

static void int33_check_hog(void);

/* mouse */
static int int33(void) {
/* New code introduced by Ed Sirett (ed@cityscape.co.uk)  26/1/95 to give 
 * garrot control when the dos app is polling the mouse and the mouse is 
 * taking a break. */

/* Firstly do the actual mouse function. */   
/* N.B. This code lets the real mode mouse driver return at a HLT, so
 * after it returns the hogthreshold code can do its job.
 */
  if (IS_REDIRECTED(0x33)) {
    fake_int_to(Mouse_SEG, Mouse_HLT_OFF);
    return 0;
  }
  mouse_int();
  int33_check_hog();
  return 1;
}

static void int33_check_hog(void)
{
  static unsigned short int oldx=0, oldy=0;

/* It seems that the only mouse sub-function that could be plausibly used to 
 * poll the mouse is AX=3 - get mouse buttons and position. 
 * The mouse driver should have left AX=3 unaltered during its call.
 * The correct response should have the buttons in the low 3 bits in BX and 
 * x,y in CX,DX. 
 * Some programs seem to interleave calls to read mouse with various other 
 * sub-functions (Esp. 0x0b  0x05 and 0x06)
 * As a result we do not reset the  trigger value in these cases. 
 * Sadly, some programs use the user-specified mouse-event handler function (0x0c)
 * after which they then wait for mouse events presumably in a tight loop, I think
 * that we won't be able to stop these programs from burning CPU cycles.
 */
  if (LWORD(eax) == 0x0003) {
    if (LWORD(ebx) == 0 && oldx == LWORD(ecx) && oldy == LWORD(edx))
      trigger_idle();
    else {
      reset_idle(0);
      oldx = LWORD(ecx);
      oldy = LWORD(edx);
    }
  }
  m_printf("Called/ing the mouse with AX=%x \n",LWORD(eax));
  /* Ok now we test to see if the mouse has been taking a break and we can let the 
   * system get on with some real work. :-) */
  idle(200, 20, 20, INT15_IDLE_USECS, "mouse");
}

static void fake_iret(void);

/* this function is called from the HLT at Mouse_SEG:Mouse_HLT_OFF */
void int33_post(void)
{
  fake_iret();
  int33_check_hog();
}

/* mfs FCB call */
static int inte7(void) {
  SETIVEC(0xe7, INTE7_SEG, INTE7_OFF);
  return 0;
}

static void debug_int(const char *s, int i)
{
 	if (((i != 0x28) && (i != 0x2f)) || in_dpmi) {
 		di_printf("%s INT0x%02x eax=0x%08x ebx=0x%08x ss=0x%04x esp=0x%08x\n"
 			  "           ecx=0x%08x edx=0x%08x ds=0x%04x  cs=0x%04x ip=0x%04x\n"
 			  "           esi=0x%08x edi=0x%08x es=0x%04x flg=0x%08x\n",
 			  s, i, _EAX, _EBX, _SS, _ESP,
 			  _ECX, _EDX, _DS, _CS, _IP,
 			  _ESI, _EDI, _ES, (int) read_EFLAGS());
 	}
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

static void do_int_from_hlt(Bit32u i)
{
	if (debug_level('#') > 2)
		debug_int("Do", i);
  
 	/* Always use the caller function: I am calling into the
 	   interrupt table at the start of the dosemu bios */
	fake_iret();
	run_caller_func(i, NO_REVECT);
	if (debug_level('#') > 2)
		debug_int("RET", i);
#ifdef USE_MHPDBG
	  mhp_debug(DBG_INTx + (i << 8), 0, 0);
#endif
}

void do_int(int i)
{
        /* we must clear the AC flag here since real mode INT instructions
           do that too. An IRET (not IRETD) instruction then does not set AC
           because the AC flag is in the high part of the eflags.
           DOS applications usually set AC to try to detect the presence of
           a 486. They hopefully protect this test using cli and sti, or
           hardware INTs will mess things up.
        */
        clear_AC();
	
	if (debug_level('#') > 2)
		debug_int("Do", i);
  
#if 1  /* This test really ought to be in the main loop before
 	*  instruction execution not here. --EB 10 March 1997 
 	*/

 	/* try to catch jumps to 0:0 (e.g. uninitialized user interrupt vectors),
 	   which sometimes can crash the whole system, not only dosemu... */
 	if (SEGOFF2LINEAR(_CS, _IP) < 1024) {
 		error("OUCH! attempt to execute interrupt table - quickly dying\n");
 		leavedos(57);
 	}
#endif
  
  
 	/* see if I want to use the caller function */
 	/* I want to use it if I must always use it */
 	/* assume IP was just incremented by 2 past int int instruction which set us
 	   off */
	
 	if (SEGOFF2LINEAR(BIOSSEG, INT_OFF(i)) == IVEC(i)) {
		run_caller_func(i, can_revector(i));
	} else if (can_revector(i) != REVECT || !run_caller_func(i, REVECT)) {
		di_printf("int 0x%02x, ax=0x%04x\n", i, LWORD(eax));
		if (IS_IRET(i)) {
			if ((i != 0x2a) && (i != 0x28))
				g_printf("just an iret 0x%02x\n", i);
		} else {
			real_run_int(i);
		}
	}
}

void fake_int(int cs, int ip)
{
  unsigned char *ssp;
  unsigned long sp;

  g_printf("fake_int: CS:IP %04x:%04x\n", cs, ip);
  ssp = SEG2LINEAR(LWORD(ss));
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, vflags);
  pushw(ssp, sp, cs);
  pushw(ssp, sp, ip);
  LWORD(esp) -= 6;

  clear_TF();
  clear_NT();
  clear_IF();
}

void fake_int_to(int cs, int ip)
{
  fake_int(REG(cs), LWORD(eip));
  REG(cs) = cs;
  REG(eip) = ip;
}

void fake_call(int cs, int ip)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = SEG2LINEAR(LWORD(ss));
  sp = (unsigned long) LWORD(esp);

  g_printf("fake_call() CS:IP %04x:%04x\n", cs, ip);
  pushw(ssp, sp, cs);
  pushw(ssp, sp, ip);
  LWORD(esp) -= 4;
}

void fake_call_to(int cs, int ip)
{
  fake_call(REG(cs), LWORD(eip));
  REG(cs) = cs;
  REG(eip) = ip;
}

void fake_pusha(void)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = SEG2LINEAR(LWORD(ss));
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, LWORD(eax));
  pushw(ssp, sp, LWORD(ecx));
  pushw(ssp, sp, LWORD(edx));
  pushw(ssp, sp, LWORD(ebx));
  pushw(ssp, sp, LWORD(esp));
  pushw(ssp, sp, LWORD(ebp));
  pushw(ssp, sp, LWORD(esi));
  pushw(ssp, sp, LWORD(edi));
  LWORD(esp) -= 16;
  pushw(ssp, sp, REG(ds));
  pushw(ssp, sp, REG(es));
  LWORD(esp) -= 4;
}

void fake_retf(unsigned pop_count)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = SEG2LINEAR(REG(ss));
  sp = (unsigned long) LWORD(esp);

  _IP = popw(ssp, sp);
  _CS = popw(ssp, sp);
  _SP += 4 + 2 * pop_count;
}

static void fake_iret(void)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = SEG2LINEAR(REG(ss));
  sp = (unsigned long) LWORD(esp);

  _SP += 6;
  _IP = popw(ssp, sp);
  _CS = popw(ssp, sp);
  set_FLAGS(popw(ssp, sp));
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
  emu_hlt_t hlt_hdlr;

  /* init trapped interrupts called via jump */
  for (i = 0; i < 256; i++) {
    interrupt_function[i][NO_REVECT] =
      interrupt_function[i][REVECT] = NULL;
  }
  
  interrupt_function[5][NO_REVECT] = int05;
  /* This is called only when revectoring int10 */
  interrupt_function[0x10][NO_REVECT] = int10;
  interrupt_function[0x11][NO_REVECT] = int11;
  interrupt_function[0x12][NO_REVECT] = int12;
  interrupt_function[0x13][NO_REVECT] = int13;
  interrupt_function[0x14][NO_REVECT] = int14;
  interrupt_function[0x15][NO_REVECT] = int15;
  interrupt_function[0x16][NO_REVECT] = int16;
  interrupt_function[0x17][NO_REVECT] = int17;
  interrupt_function[0x18][NO_REVECT] = int18;
  interrupt_function[0x19][NO_REVECT] = int19;
  interrupt_function[0x1a][NO_REVECT] = int1a;
  if (config.lfn)
    interrupt_function[0x21][NO_REVECT] = int21lfnhook;
  interrupt_function[0x21][REVECT] = int21;
  interrupt_function[0x28][REVECT] = int28;
  interrupt_function[0x29][NO_REVECT] = int29;
  interrupt_function[0x2f][REVECT] = int2f;
  interrupt_function[0x33][NO_REVECT] = mouse_int;
  interrupt_function[0x33][REVECT] = int33;
#ifdef USING_NET
  if (config.pktdrv)
    interrupt_function[0x60][NO_REVECT] = pkt_int;
#endif
#ifdef IPX
  if (config.ipxsup)
    interrupt_function[0x7a][NO_REVECT] = ipx_int7a;
#endif
  interrupt_function[0xe6][REVECT] = inte6;
  interrupt_function[0xe7][REVECT] = inte7;

  /* set up relocated video handler (interrupt 0x42) */
  if (config.dualmon == 2) {
    interrupt_function[0x42][NO_REVECT] = interrupt_function[0x10][NO_REVECT];
  }

  set_int21_revectored(redir_state = 1);

  hlt_hdlr.name       = "interrupts";
  hlt_hdlr.start_addr = 0x0000;
  hlt_hdlr.end_addr   = 0x00ff;
  hlt_hdlr.func       = do_int_from_hlt;
  hlt_register_handler(hlt_hdlr);
}


void set_int21_revectored(int a)
{
  static int rv_all = 0;
  int i;

  ds_printf("INT21: rv_all: %d + %d = ", rv_all, a);

  rv_all += a;

  if(rv_all > 0) {
    memset(&vm86s.int21_revectored, 0xff, sizeof(vm86s.int21_revectored));
  }
  else {
    memset(&vm86s.int21_revectored, 0x00, sizeof(vm86s.int21_revectored));
    for(i = 0; i < 0x100; i++)
      if(can_revector_int21(i) == REVECT) set_revectored(i, &vm86s.int21_revectored);
  }

  ds_printf("%d\n", rv_all);
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

  set_int21_revectored(0);
#endif

}

static void update_xtitle(void)
{
  char cmdname[9];
  char *cmd_ptr, *tmp_ptr;
  u_short psp_seg;
  struct MCB *mcb;
  int force_update;

  if (!sda)
    return;
  psp_seg = sda_cur_psp(sda);
  if (!psp_seg)
    return;
  mcb = (struct MCB *)SEG2LINEAR(psp_seg - 1);
  if (!mcb)
    return;
  force_update = !title_hint[0];

  MEMCPY_2UNIX(cmdname, mcb->name, 8);
  cmdname[8] = 0;
  cmd_ptr = tmp_ptr = cmdname + strspn(cmdname, " \t");
  while (*tmp_ptr) {	/* Check whether the name is valid */
    if (iscntrl(*tmp_ptr++))
      return;
  }

  if (in_win31 && memcmp(cmd_ptr, "krnl", 4) == 0) {
    cmd_ptr = win31_title;
    force_update = 1;
  }

  if (force_update || strcmp(title_current, title_hint) != 0) {
    if (force_update || strcmp(cmd_ptr, title_hint) == 0) {
      if (force_update || can_change_title) {
	if (strcmp(title_current, cmd_ptr) == 0)
	  return;
        strcpy(title_current, cmd_ptr);
        change_window_title(title_current);
      }
    } else {
      can_change_title = 1;
    }
  }
}

void do_periodic_stuff(void)
{
    static hitimer_t last_time = 0;

    if (in_crit_section)
	return;

    handle_signals();
    /* dont go to poll the I/O if <10mS elapsed */
    if (GETusTIME(0) - last_time >= 10000) {
	last_time = GETusTIME(0);
	io_select(fds_sigio);	/* we need this in order to catch lost SIGIOs */
	if (not_use_sigio)
	    io_select(fds_no_sigio);

	/* catch user hooks here */
	if (uhook_fdin != -1) uhook_poll();

	/* here we include the hooks to possible plug-ins */
	#define VM86_RETURN_VALUE retval
	#include "plugin_poll.h"
	#undef VM86_RETURN_VALUE
    }

#ifdef USE_MHPDBG  
    if (mhpdbg.active) mhp_debug(DBG_POLL, 0, 0);
#endif

    if (Video->change_config)
	update_xtitle();
}

void set_io_buffer(char *ptr, unsigned int size)
{
  dos_io_buffer = ptr;
  dos_io_buffer_size = size;
}

void unset_io_buffer()
{
  dos_io_buffer_size = 0;
}
