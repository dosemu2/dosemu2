/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Extensions by Robert Sanders, 1992-93
 * 
 * DANG_BEGIN_MODULE
 * 
 * REMARK
 * Here is where DOSEMU gets booted. From emu.c external calls are made to
 * the specific I/O systems (video/keyboard/serial/etc...) to initialize
 * them. Memory is cleared/set up and the boot sector is read from the
 * boot drive. Many SIGNALS are set so that DOSEMU can exploit things like
 * timers, I/O signals, illegal instructions, etc... When every system
 * gives the green light, vm86() is called to switch into vm86 mode and
 * start executing i86 code.
 * 
 * The vm86() function will return to DOSEMU when certain `exceptions` occur
 * as when some interrupt instructions occur (0xcd).
 * 
 * The top level function emulate() is called from dos.c by way of a dll
 * entry point.
 * 
 * /REMARK
 * DANG_END_MODULE
 * 
 */


/*
 * DANG_BEGIN_REMARK 
 * DOSEMU must not work within the 1 meg DOS limit, so
 * start of code is loaded at a higher address, at some time this could
 * conflict with other shared libs. If DOSEMU is compiled statically
 * (without shared libs), and org instruction is used to provide the jump
 * above 1 meg. 
 * DANG_END_REMARK
 */

#include "config.h"

#ifndef __ELF__
/*
 * DANG_BEGIN_FUNCTION jmp_emulate
 * 
 * description: This function allows the startup program `dos` to know how to
 * call the emulate function by way of the dll headers. Always make sure
 * that this line is the first of emu.c and link emu.o as the first object
 * file to the lib
 * 
 * DANG_END_FUNCTION
 */
__asm__("___START___: jmp _emulate\n");
#endif

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#ifndef EDEADLOCK
  #define EDEADLOCK EDEADLK
#endif
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <getopt.h>
#include <assert.h>
#include <locale.h>

#ifdef __linux__
#include <sys/vt.h>
#include <sys/kd.h>
#include "Linux/fd.h"
#include "Linux/hdreg.h"
#include <sys/vm86.h>
#include <syscall.h>
#endif

#include "memory.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

#ifdef X_SUPPORT
#include "../env/video/X.h"
#endif

#include "emu.h"

#include "bios.h"
#include "termio.h"
#include "video.h"
#include "timers.h"
#include "cmos.h"
#include "mouse.h"
#include "dosio.h"
#include "disks.h"
#include "xms.h"
#include "hgc.h"
#include "ipx.h"		/* TRB - add support for ipx */
#include "serial.h"
#include "int.h"
#include "bitops.h"
#include "pic.h"
#include "dpmi.h"
#include "priv.h"   /* for priv_init */
#include "port.h"   /* for port_init */
#include "pci.h"
#include "speaker.h"
#include "utilities.h"
#include "dos2linux.h"
#include "iodev.h"
#include "mapping.h"
#include "dosemu_config.h"
#include "shared.h"
#include "userhook.h"
#include "pktdrvr.h"

#include "keyb_clients.h"

#ifdef USE_SBEMU
#include "sound.h"
#endif
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

jmp_buf NotJEnv;

void 
boot(void)
{
    char           *buffer;
    struct disk    *dp = NULL;

    switch (config.hdiskboot) {
    case 0:
	if (config.bootdisk)
	    dp = &bootdisk;
	else
	    dp = &disktab[0];
	break;
    case 1:
	dp = &hdisktab[0];
	break;
    case 2:
	dp = &disktab[1];
	break;
    default:
	error("unexpected value for config.hdiskboot\n");
	leavedos(15);
    }

    disk_close();
    disk_open(dp);

    buffer = (char *) 0x7c00;

    if (dp->boot_name) {/* Boot from the specified file */
        int bfd;
        d_printf ("Booting from bootfile=%s...\n",dp->boot_name);
        bfd = open (dp->boot_name, O_RDONLY);
        if (bfd == -1) {/* Abort with error */
            error("Boot file %s missing\n",dp->boot_name);
            leavedos(16);
        }
        if (read(bfd, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
            error("Failed to read exactly %d bytes from %s\n",
                  SECTOR_SIZE, dp->boot_name);
            leavedos(16);
        }
        close(bfd);
    }
    else
    if (dp->type == PARTITION) {/* we boot partition boot record, not MBR! */
	d_printf("Booting partition boot record from part=%s....\n", dp->dev_name);
	if (RPT_SYSCALL(read(dp->fdesc, buffer, SECTOR_SIZE)) != SECTOR_SIZE) {
	    error("reading partition boot sector using partition %s.\n", dp->dev_name);
	    leavedos(16);
	}
    } else if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
	error("can't boot from %s, using harddisk\n", dp->dev_name);
	dp = hdisktab;
	if (read_sectors(dp, buffer, 0, 0, 0, 1) != SECTOR_SIZE) {
	    error("can't boot from hard disk\n");
	    leavedos(16);
	}
    }
    disk_close();
}

static inline void 
vm86plus_init(void)
{
    static inline int vm86_old(struct vm86_struct* v86)
    {
        int __res;
        __asm__ __volatile__("int $0x80\n"
        :"=a" (__res):"a" ((int)113), "b" ((int)v86));
        return __res;
    }
    if (!vm86_plus(VM86_PLUS_INSTALL_CHECK,0)) return;
    if (!vm86_old((void *)0xffffff01)) {
      fprintf(stderr, "your kernel contains an older (interim) vm86plus version\n\r"
      		      "please upgrade to an newer one\n\r");
    }
    else fprintf(stderr, "vm86plus service not available in your kernel\n\r");
    fflush(stdout);
    fflush(stderr);
    _exit(1);
}

static inline void
module_init(void)
{
    vm86plus_init();		/* emumodule support */
    memcheck_init();		/* lower 1M memory map support */
}


static void do_liability_disclaimer_prompt(void)
{
  FILE *f;
  char buf[32];
  char disclaimer_file_name[256];
  static char text[] =
  "The Linux DOSEMU, Copyright (C) 2003 the 'DOSEMU-Development-Team'.\n"
  "This program is  distributed  in  the  hope that it will be useful,\n"
  "but  WITHOUT  ANY  WARRANTY;   without even the implied warranty of\n"
  "MERCHANTABILITY  or  FITNESS FOR A PARTICULAR PURPOSE. See the file\n"
  "COPYING for more details.  Use  this  program  at  your  own  risk!\n\n"
  "By continuing execution of this program,  you  are stating that you\n"
  "have read the file  COPYING  and the above liability disclaimer and\n"
  "that you accept these conditions.\n\n"
  "Enter 'yes' to confirm/continue: ";


  snprintf(disclaimer_file_name, 256, "%s/disclaimer", LOCALDIR);
  if (exists_file(disclaimer_file_name)) return;

  fputs(text, stdout);
  fgets(buf, sizeof(buf), stdin);
  if (strncmp(buf, "yes", 3)) {
    exit(1);
  }

  /*
   * We now remember this by writing the above text to a
   * in LOCALDIR/disclaimer
   */
   f = fopen(disclaimer_file_name, "w");
   if (!f) {
     fprintf(stderr, "cannot create %s\n", disclaimer_file_name);
     exit(1);
   }
   fprintf(f, "%s%s\n", text, buf);
   fclose(f);
}


/*
 * DANG_BEGIN_FUNCTION emulate
 * 
 * arguments: 
 * argc - Argument count. 
 * argv - Arguments.
 * 
 * description: 
 * Emulate gets called from dos.c. It initializes DOSEMU to
 * prepare it for running in vm86 mode. This involves catching signals,
 * preparing memory, calling all the initialization functions for the I/O
 * subsystems (video/serial/etc...), getting the boot sector instructions
 * and calling vm86().
 * 
 * DANG_END_FUNCTION
 * 
 */
#ifdef __ELF__
int 
main(int argc, char **argv)
#else
int
emulate(int argc, char **argv)
#endif
{
    int e;

    srand(time(NULL));
    memset(&config, 0, sizeof(config));

    if ((e=setjmp(NotJEnv))) {
        flush_log();
    	fprintf(stderr,"EMERGENCY JUMP %x!!!\n",e);
    	/* there's no other way to stop a signal 11 from hanging dosemu
    	 * but politely ask the kernel to terminate ourselves */
	kill(0, SIGKILL);
	_exit(1);		/* just in case */
    }

		setlocale(LC_ALL,"");

    /* NOW! it is safe to touch the priv code.  */
    priv_init();  /* This must come first! */

    /* Before we even try to give options to the parser,
     * we pre-filter some dangerous options and delete them
     * from the arguments list
     */
    secure_option_preparse(&argc, argv);

    /* This has to come next:
     * Parse dosemu.users _before_ any argument usage to catch
     * a not allowed user playing with overruns and kick him out
     * Additionally, we check a non-suid root condition, if dosemu.user
     * says so and exit if needed.
     */
    parse_dosemu_users();

    /*
     * Do the 'liability disclaimer' stuff we need to avoid problems
     * with laws in some countries.
     */
    do_liability_disclaimer_prompt();

    /* the transposal of (config_|stdio_)init allows the addition of -o */
    /* to specify a debug out filename, if you're wondering */

    get_time_init();		/* debug can use CPUtime */
    io_select_init();
    port_init();		/* setup port structures, before config! */
    version_init();		/* Check the OS version */
    config_init(argc, argv);	/* parse the commands & config file(s) */
#ifdef X86_EMULATOR
#ifdef DONT_DEBUG_BOOT		/* cpuemu only */
    memcpy(&debug_save, &debug, sizeof(debug));
    set_debug_level('e', 0);
#ifdef TRACE_DPMI
    set_debug_level('t', 0);
#endif
#endif
#endif
    get_time_init();
    stdio_init();		/* initialize stdio & open debug file */
    print_version();            /* log version information */
    module_init();
    low_mem_init();		/* initialize the lower 1Meg */
    time_setting_init();	/* get the startup time */
    cpu_setup();		/* setup the CPU */
    pcibios_init();
    pci_setup();
    extra_port_init();		/* setup ports dependent on config */
    signal_init();              /* initialize sig's & sig handlers */
    device_init();		/* initialize keyboard, disk, video, etc. */
    map_video_bios();           /* map the video bios */
    map_hardware_ram();         /* map the direct hardware ram */
    pkt_priv_init();

    /* here we include the hooks to possible plug-ins */
    #include "plugin_init.h"

    if (config.exitearly) {
      dbug_printf("Leaving DOS before booting\n");
      leavedos(0);
    }

    if (can_do_root_stuff && !under_root_login) {
        g_printf("dropping root privileges\n");
    }
    priv_drop();
    
    mapping_init();		/* initialize mapping drivers */
    HMA_init();			/* HMA can only be done now after mapping
                                   is initialized*/
#ifdef X_SUPPORT
    if (config.X)		/* and the same is true for vgaemu */
      X_init_videomode();
#endif
    
    memory_init();		/* initialize the memory contents */
    boot();			/* read the boot sector & get moving */

    if (not_use_sigio)
	k_printf("Atleast 1 NON-SIGIO file handle in use.\n");
    else
	k_printf("No NON-SIGIO file handles in use.\n");
    g_printf("EMULATE\n");

    fflush(stdout);

#ifdef USE_MHPDBG  
    mhp_debug(DBG_INIT, 0, 0);
#endif
    timer_interrupt_init();	/* start sending int 8h int signals */

    while (!fatalerr) {
	loopstep_run_vm86();
    }

    sync();
    fprintf(stderr, "Not a good day to die!!!!!\n");
    leavedos(99);
    return 0;  /* just to make gcc happy */
}

#if 0    /* disable C-A-D until someone will fix it (if really needed) */
static int      special_nowait = 0;

void
dos_ctrl_alt_del(void)
{
    dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
    HMA_MAP(1);
    time_setting_init();
#ifdef HAVE_KEYBOARD_V1
    keyb_server_reset();
#endif
    iodev_reset();
    video_config_init();
    serial_init();
    dosemu_mouse_init();
    printer_init();
    disk_close();
    disk_init();
    scr_state_init();
    clear_screen(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE), 7);
    special_nowait = 0;
    p_dos_str("Rebooting DOS.  Be careful...this is partially implemented\r\n");
    disk_init();
    cpu_setup();
    hardware_setup();
    memcheck_init();		/* lower 1M memory map support */
    memory_init();
    boot();
    timer_interrupt_init();	/* start sending int 8h int signals */
}
#endif

static void
ign_sigs(int sig)
{
    static int      timerints = 0;
    static int      otherints = 0;

    error("signal %d received in leavedos()\n", sig);
    show_regs(__FILE__, __LINE__);
    flush_log();
    if (sig == SIGALRM)
	timerints++;
    else
	otherints++;

#define LEAVEDOS_TIMEOUT (3 * config.freq)
#define LEAVEDOS_SIGOUT  5
    if ((sig==11) || (timerints >= LEAVEDOS_TIMEOUT) || (otherints >= LEAVEDOS_SIGOUT)) {
	error("timed/signalled out in leavedos()\n");
	longjmp(NotJEnv, 0x11);
    }
}

int leavedos_recurse_check = 0;


/* "graceful" shutdown */
void
leavedos(int sig)
{
    struct sigaction sa;
    struct itimerval itv;
   
    if (leavedos_recurse_check)
      {
       error("leavedos called recursively, forgetting the graceful exit!\n");
       flush_log();
       longjmp(NotJEnv, sig);
      }
    leavedos_recurse_check = 1;
    dbug_printf("leavedos(%d|0x%x) called - shutting down\n", sig, sig);
#if 1 /* BUG CATCHER */
    if (in_vm86) {
      g_printf("\nkilled while in vm86(), trying to dump DOS-registers:\n");
      show_regs(__FILE__, __LINE__);
    }
#endif
    in_vm86 = 0;

    /* try to notify dosdebug */
#ifdef USE_MHPDBG
    if (fault_cnt > 0)
      error("leavedos() called from within a signal context!\n");
    else
      mhp_exit_intercept(sig);
#endif
    
    itv.it_interval.tv_sec = itv.it_interval.tv_usec = 0;
    itv.it_value = itv.it_interval;
    if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
	g_printf("can't turn off timer at shutdown: %s\n", strerror(errno));
    }
    SETSIG(SIGALRM, ign_sigs);
#ifdef X86_EMULATOR
    setitimer(ITIMER_PROF, &itv, NULL);
    SETSIG(SIGPROF, ign_sigs);
#endif
    SETSIG(SIGSEGV, ign_sigs);
    SETSIG(SIGILL, ign_sigs);
    SETSIG(SIGFPE, ign_sigs);
    SETSIG(SIGTRAP, ign_sigs);

    /* here we include the hooks to possible plug-ins */
    #include "plugin_close.h"

    if (config.speaker == SPKR_EMULATED) {
      g_printf("SPEAKER: sound off\n");
      speaker_off();		/* turn off any sound */
    }
    else if (config.speaker==SPKR_NATIVE) {
       g_printf("SPEAKER: sound off\n");
       /* Since the speaker is native hardware use port manipulation,
	* we don't know what is actually implementing the kernel's
	* ioctls.
	* My port logic is actually stolen from kd_nosound in the kernel.
	* 		--EB 21 September 1997
	*/
       if (portserver_pid)
          port_safe_outb(0x61, port_safe_inb(0x61)&0xFC); /* turn off any sound */
    }

#ifdef SIG
    g_printf("calling SIG_close\n");
#endif
    SIG_close();

    /* try to regain control of keyboard and video first */
    g_printf("calling keyboard_close\n");
#ifdef HAVE_KEYBOARD_V1
    keyb_server_close();
    keyb_client_close();
#endif
    iodev_term();

#if defined(X86_EMULATOR)
    /* if we are here with config.cpuemu>1 something went wrong... */
    if (config.cpuemu>1) {
    	leave_cpu_emu();
    }
#endif
    show_ints(0, 0x33);
    g_printf("calling disk_close_all\n");
    disk_close_all();
    g_printf("calling video_close\n");
    video_close();
   
    if (config.emuretrace) {
      do_r3da_pending ();
      set_ioperm (0x3da, 1, 1);
      set_ioperm (0x3c0, 1, 1);
      config.emuretrace = 0;
    }

    /* terminate port server */
    port_exit();

    g_printf("releasing ports and blocked devices\n");
    release_ports();

    g_printf("calling shared memory exit\n");
    shared_memory_exit();
    g_printf("calling HMA exit\n");
    hma_exit();
    close_uhook();
#ifdef USE_MHPDBG
    g_printf("closing debugger pipes\n");
    mhp_close();
#endif

    g_printf("calling mapping_close()\n");
    mapping_close();

    if (config.detach) {
	restore_vt(config.detach);
	disallocate_vt();
    }

#ifdef IPX
    ipx_close();
#endif

    g_printf("calling close_all_printers\n");
    close_all_printers();

    g_printf("calling mouse_close\n");
    dosemu_mouse_close();

    flush_log();

    _exit(sig);
}

#if 0
/* check the fd for data ready for reading */
int
d_ready(int fd)
{
    int selrtn;
    struct timeval  w_time;
    fd_set          checkset;

    w_time.tv_sec = 0;
    w_time.tv_usec = 200000;

    FD_ZERO(&checkset);
    FD_SET(fd, &checkset);

    do {
        selrtn = select(fd + 1, &checkset, NULL, NULL, &w_time);
    } while(selrtn == -1 && errno == EINTR);

    if (selrtn == 1) {
	if (FD_ISSET(fd, &checkset))
	    return (1);
	else
	    return (0);
    } else
	return (0);
}

void activate(int con_num)
{
    if (in_ioctl) {
	k_printf("KBD: can't ioctl for activate, in a signal handler\n");
	do_ioctl(kbd_fd, VT_ACTIVATE, con_num);
    } else
	do_ioctl(kbd_fd, VT_ACTIVATE, con_num);
}
#endif
