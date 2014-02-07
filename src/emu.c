/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
#include <syscall.h>
#endif

#include "version.h"
#include "memory.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif
#include "debug.h"

#include "emu.h"
#include "vm86plus.h"

#include "bios.h"
#include "termio.h"
#include "video.h"
#include "timers.h"
#include "cmos.h"
#include "mouse.h"
#include "disks.h"
#include "xms.h"
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
#include "userhook.h"
#include "pktdrvr.h"
#include "dma.h"
#include "hlt.h"
#include "coopth.h"
#include "keyb_server.h"
#include "keyb_clients.h"

#ifdef USE_SBEMU
#include "sound.h"
#endif
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

sigjmp_buf NotJEnv;
static int ld_tid;

void
boot(void)
{
    unsigned buffer;
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

    buffer = 0x7c00;

    if (dp->boot_name) {/* Boot from the specified file */
        int bfd;
        d_printf ("Booting from bootfile=%s...\n",dp->boot_name);
        bfd = open (dp->boot_name, O_RDONLY);
        if (bfd == -1) {/* Abort with error */
            error("Boot file %s missing\n",dp->boot_name);
            leavedos(16);
        }
        if (dos_read(bfd, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
            error("Failed to read exactly %d bytes from %s\n",
                  SECTOR_SIZE, dp->boot_name);
            leavedos(16);
        }
        close(bfd);
    }
    else
    if (dp->type == PARTITION) {/* we boot partition boot record, not MBR! */
	d_printf("Booting partition boot record from part=%s....\n", dp->dev_name);
	if (dos_read(dp->fdesc, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
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
#ifdef __i386__
    if (!vm86_plus(VM86_PLUS_INSTALL_CHECK,0)) return;
    if (!syscall(SYS_vm86old, (void *)0xffffff01)) {
      fprintf(stderr, "your kernel contains an older (interim) vm86plus version\n\r"
      		      "please upgrade to an newer one\n\r");
    }
    else
#endif
    {
#ifdef X86_EMULATOR
      warn("WARN: vm86plus service not available in your kernel\n");
      warn("WARN: using CPU emulation for vm86()\n");
      if (config.cpuemu < 3) {
	config.cpuemu = 3;
      }
      if (config.cpuemu < 3) config.cpuemu = 3;
      return;
#else
      fprintf(stderr, "vm86plus service not available in your kernel\n\r");
#endif
    }
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


void do_liability_disclaimer_prompt(int dosboot, int prompt)
{
  FILE *f;
  char buf[32];
  char *disclaimer_file_name;
  static char text[] =
  "\nWelcome to DOSEMU "VERSTR", a DOS emulator"
#ifdef __linux__
    " for Linux"
#endif
  ".\nCopyright (C) 1992-2006 the 'DOSEMU-Development-Team'.\n"
  "This program is  distributed  in  the  hope that it will be useful,\n"
  "but  WITHOUT  ANY  WARRANTY;   without even the implied warranty of\n"
  "MERCHANTABILITY  or  FITNESS FOR A PARTICULAR PURPOSE. See the files\n"
  "COPYING.DOSEMU and COPYING for more details.\n"
  "Use  this  program  at  your  own  risk!\n\n";

  static char text2[] =
  "Press ENTER to confirm, and boot DOSEMU, or [Ctrl-C] to abort\n";

  disclaimer_file_name = assemble_path(LOCALDIR, "disclaimer", 0);
  if (exists_file(disclaimer_file_name)) {
    free(disclaimer_file_name);
    return;
  }

  if (dosboot) {
    p_dos_str("%s", text);
  } else {
    fputs(text, stdout);
  }

  buf[0] = '\0';
  if (prompt) {
    if (dosboot) {
      p_dos_str("%s", text2);
      com_biosread(buf, sizeof(buf)-2);
    } else {
      fputs(text2, stdout);
      fgets(buf, sizeof(buf), stdin);
    }
    if (buf[0] == 3) {
      leavedos(1);
    }
  }

  /*
   * We now remember this by writing the above text to a
   * in LOCALDIR/disclaimer
   */
   f = fopen(disclaimer_file_name, "w");
   if (!f) {
     fprintf(stderr, "cannot create %s\n", disclaimer_file_name);
     leavedos(1);
   }
   fprintf(f, "%s%s\n", text, buf);
   fclose(f);
   free(disclaimer_file_name);
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

    if ((e=sigsetjmp(NotJEnv, 1))) {
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

    /* the transposal of (config_|stdio_)init allows the addition of -o */
    /* to specify a debug out filename, if you're wondering */

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
    time_setting_init();	/* get the startup time */
    cpu_setup();		/* setup the CPU */
    pci_setup();
    device_init();		/* priv initialization of video etc. */
    extra_port_init();		/* setup ports dependent on config */
    signal_pre_init();          /* initialize sig's & sig handlers */
    SIG_init();			/* Silly Interrupt Generator */
    pkt_priv_init();            /* initialize the packet driver interface */

    mapping_init();		/* initialize mapping drivers */
    low_mem_init();		/* initialize the lower 1Meg */

    if (can_do_root_stuff && !under_root_login) {
        g_printf("dropping root privileges\n");
	open_kmem();
    }
    priv_drop();

    map_hardware_ram();         /* map the direct hardware ram */
    map_video_bios();           /* map (really: copy) the video bios */
    close_kmem();

    /*
     * Do the 'liability disclaimer' stuff we need to avoid problems
     * with laws in some countries.
     *
     * show it at the beginning only for terminals and direct console
     * else the banner code in int.c does it.
     */
    if (!config.X)
	install_dos(0);

    /* the following duo have to be done before others who use hlt or coopth */
    hlt_init();
    coopth_init();
    ld_tid = coopth_create("leavedos");
    coopth_set_ctx_handlers(ld_tid, sig_ctx_prepare, sig_ctx_restore);

    vm86_init();
    cputime_late_init();
    HMA_init();			/* HMA can only be done now after mapping
                                   is initialized*/
    memory_init();		/* initialize the memory contents */
    /* iodev_init() can load plugins, like SDL, that can spawn a thread.
     * This must be done before initializing signals, or problems ensue.
     * This also must be done when the signals are blocked, so after
     * the io_select_init(), which right now blocks the signals. */
    iodev_init();		/* initialize devices */
    dos2tty_init();
    signal_init();              /* initialize sig's & sig handlers */
    /* here we include the hooks to possible plug-ins */
    #include "plugin_init.h"

    if (config.exitearly) {
      dbug_printf("Leaving DOS before booting\n");
      leavedos(0);
    }

    ems_init();			/* initialize ems */
    xms_init();			/* initialize xms */
    dpmi_setup();

    g_printf("EMULATE\n");

    fflush(stdout);

#ifdef USE_MHPDBG
    mhp_debug(DBG_INIT, 0, 0);
#endif
    timer_interrupt_init();	/* start sending int 8h int signals */

    /* unprotect conventional memory just before booting */
    mprotect_mapping(MAPPING_LOWMEM, mem_base, config.mem_size * 1024,
		     PROT_READ | PROT_WRITE | PROT_EXEC);

    /* check DOSDRIVE_D (used to be done in the script) */
    if (getenv("HOME"))
      setenv("DOSDRIVE_D", getenv("HOME"), 0);

    while (!fatalerr && !config.exitearly) {
	loopstep_run_vm86();
    }

    if (fatalerr) {
      sync();
      fprintf(stderr, "Not a good day to die!!!!!\n");
    }
    leavedos(99);
    return 0;  /* just to make gcc happy */
}

void
dos_ctrl_alt_del(void)
{
    dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
    while (in_dpmi) {
	in_dpmi_dos_int = 1;
	dpmi_cleanup();
    }
    cpu_reset();
}

static void leavedos_thr(void *arg)
{
    dbug_printf("leavedos thread started\n");
    /* this may require working vm86() */
    video_close();
    dbug_printf("leavedos thread ended\n");
}

/* "graceful" shutdown */
void __leavedos(int sig, const char *s, int num)
{
    struct itimerval itv;
    int tmp;
    dbug_printf("leavedos(%s:%i|%i) called - shutting down\n", s, num, sig);
    if (in_leavedos)
      {
       error("leavedos called recursively, forgetting the graceful exit!\n");
       _exit(1);
      }
    in_leavedos++;
    registersig(SIGALRM, NULL);

    /* abandon current thread if any */
    coopth_leave();
    /* close coopthreads-related stuff first */
    dos2tty_done();
    /* try to clean up threads */
    tmp = coopth_flush(run_vm86);
    if (tmp)
      dbug_printf("%i threads still active\n", tmp);
    coopth_start(ld_tid, leavedos_thr, NULL);
    /* vc switch may require vm86() so call it while waiting for thread */
    coopth_join(ld_tid, run_vm86);
    coopth_done();

    /* try to notify dosdebug */
#ifdef USE_MHPDBG
    if (fault_cnt > 0)
      dbug_printf("leavedos() called from within a signal context!\n");
    else
      mhp_exit_intercept(sig);
    g_printf("closing debugger pipes\n");
    mhp_close();
#endif

    itv.it_interval.tv_sec = itv.it_interval.tv_usec = 0;
    itv.it_value = itv.it_interval;
    if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
	g_printf("can't turn off timer at shutdown: %s\n", strerror(errno));
    }

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
        port_safe_outb(0x61, port_safe_inb(0x61)&0xFC); /* turn off any sound */
    }

#ifdef SIG
    g_printf("calling SIG_close\n");
#endif
    SIG_close();

    /* try to regain control of keyboard and video first */
    g_printf("calling keyboard_close\n");
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
    g_printf("calling HMA exit\n");
    hma_exit();
    close_uhook();

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

    flush_log();

    /* We don't need to use _exit() here; this is the graceful exit path. */
    exit(sig);
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
    ioctl(kbd_fd, VT_ACTIVATE, con_num);
}
#endif

void hardware_run(void)
{
	dma_run ();
#ifdef USE_SBEMU
	run_sb(); /* Beat Karcher to this one .. 8-) - AM */
#endif
	keyb_server_run();
	rtc_run();
}

