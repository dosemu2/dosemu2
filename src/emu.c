/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
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

#include <features.h>
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

#ifdef __linux__
#if GLIBC_VERSION_CODE >= 2000
#include <sys/vt.h>
#include <sys/kd.h>
#else
#include <linux/vt.h>
#include <linux/kd.h>
#endif
#include <linux/fd.h>
#include <linux/hdreg.h>
#include <sys/vm86.h>
#include <syscall.h>
#endif

#include "config.h"
#include "memory.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
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
#include "port.h"
#include "pic.h"
#include "dpmi.h"
#include "priv.h"   /* for priv_init */
#include "port.h"   /* for port_init */
#include "pci.h"
#include "speaker.h"
#include "utilities.h"
#include "dos2linux.h"

#include "keyb_clients.h"

#ifdef USE_SBEMU
#include "sound.h"
#endif
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

extern void     stdio_init(void);
extern void     time_setting_init(void);
extern void     low_mem_init(void);
extern void	mapping_init(void);
extern void	mapping_close(void);
extern void	pcibios_init(void);
extern void     shared_memory_exit(void);
extern void     restore_vt(u_short);
extern void     disallocate_vt(void);
extern void     vm86_GP_fault();
extern void     config_init(int argc, char **argv);
extern void	timer_int_engine(void);
extern void	disk_open(struct disk *dp);
extern void     print_version(void);

extern void io_select_init(void);


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

    ignore_segv++;

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
    ignore_segv--;
}

/* Silly Interrupt Generator Initialization/Closedown */

#ifdef SIG
SillyG_t       *SillyG = 0;
static SillyG_t SillyG_[16 + 1];
#endif

/*
 * DANG_BEGIN_FUNCTION SIG_int
 * 
 * description: Allow DOSEMU to be made aware when a hard interrupt occurs
 * The IRQ numbers to monitor are taken from config.sillyint, each bit
 * corresponding to one IRQ. The higher 16 bit are defining the use of
 * SIGIO
 * 
 * DANG_END_FUNCTION
 */
static inline void 
SIG_init()
{
#if defined(SIG)
    PRIV_SAVE_AREA
    /* Get in touch with Silly Interrupt Handling */
    if (config.sillyint) {
	char            prio_table[] =
	{8, 9, 10, 11, 12, 14, 15, 3, 4, 5, 6, 7};
	int             i,
	                irq;
	SillyG_t       *sg = SillyG_;
	for (i = 0; i < sizeof(prio_table); i++) {
	    irq = prio_table[i];
	    if (config.sillyint & (1 << irq)) {
		int ret;
		enter_priv_on();
		ret = vm86_plus(VM86_REQUEST_IRQ, (SIGIO << 8) | irq);
		leave_priv_setting();
		if ( ret > 0) {
		    g_printf("Gonna monitor the IRQ %d you requested\n", irq);
		    sg->fd = -1;
		    sg->irq = irq;
		    g_printf("SIG: IRQ%d, enabling PIC-level %ld\n", irq, pic_irq_list[irq]);
		    { extern int SillyG_do_irq(void);
		    pic_seti(pic_irq_list[irq], SillyG_do_irq, 0);
		    }
		    pic_unmaski(pic_irq_list[irq]);
		    sg++;
		}
	    }
	}
	sg->fd = 0;
	if (sg != SillyG_)
	    SillyG = SillyG_;
    }
#endif
}

static inline void 
SIG_close()
{
#if defined(SIG)
    if (SillyG) {
	SillyG_t       *sg = SillyG;
	while (sg->fd) {
	    vm86_plus(VM86_FREE_IRQ, sg->irq);
	    sg++;
	}
	g_printf("Closing all IRQ you opened!\n");
    }
#endif
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
    SIG_init();			/* silly int generator support */
    memcheck_init();		/* lower 1M memory map support */
}


static void do_liability_disclaimer_prompt(void)
{
  FILE *f;
  char buf[32];
  char disclaimer_file_name[256];
  static char text[] ="
  The Linux DOSEMU, Copyright (C) 2000 the 'DOSEMU-Development-Team'.
  This program is  distributed  in  the  hope that it will be useful,
  but  WITHOUT  ANY  WARRANTY;   without even the implied warranty of
  MERCHANTABILITY  or  FITNESS FOR A PARTICULAR PURPOSE. See the file
  COPYING for more details.  Use  this  programm  at  your  own risk!

  By continuing execution of this programm,  you are stating that you
  have read the file  COPYING  and the above liability disclaimer and
  that you accept these conditions.

  Enter 'yes' to confirm/continue: ";


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
    extern void parse_dosemu_users(void);
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

    /* NOW! it is safe to touch the priv code.  */
    priv_init();  /* This must come first! */

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
    memcpy(&d_save,&d,sizeof(struct debug_flags));
    if (d.emu) memset(&d,0,sizeof(struct debug_flags));
#endif
#endif
    get_time_init();
    stdio_init();		/* initialize stdio & open debug file */
    mapping_init();		/* initialize mapping drivers */
    print_version();            /* log version information */
    module_init();
    low_mem_init();		/* initialize the lower 1Meg */
    time_setting_init();	/* get the startup time */
    signal_init();		/* initialize sig's & sig handlers */
    device_init();		/* initialize keyboard, disk, video, etc. */
    cpu_setup();		/* setup the CPU */
    pcibios_init();
    pci_setup();
    hardware_setup();		/* setup any hardware */
    extra_port_init();		/* setup ports dependent on config */
    memory_init();		/* initialize the memory contents */

    /* here we include the hooks to possible plug-ins */
    #include "plugin_init.h"

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

    error("error exit: (%d,0x%04x) in_sigsegv: %d ignore_segv: %d\n",
	  fatalerr, fatalerr, in_sigsegv, ignore_segv);

    sync();
    fprintf(stderr, "Not a good day to die!!!!!\n");
    leavedos(99);
    return 0;  /* just to make gcc happy */
}

#if 0    /* disable C-A-D until someone will fix it (if really needed) */
extern void HMA_MAP(int);
static int      special_nowait = 0;

void
dos_ctrl_alt_del(void)
{
    dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
    HMA_MAP(1);
    time_setting_init();
    keyb_server_reset();
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
    extern int errno;
    extern void do_r3da_pending (void);	/* emuretrace stuff */
   
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
    {
      extern void mhp_exit_intercept(int errcode);
      mhp_exit_intercept(sig);
    }

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
       port_safe_outb(0x61, port_safe_inb(0x61)&0xFC); /* turn off any sound */
    }

#ifdef SIG
    g_printf("calling SIG_close\n");
#endif
    SIG_close();

    /* try to regain control of keyboard and video first */
    g_printf("calling keyboard_close\n");
    keyb_server_close();
    keyb_client_close();

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

    g_printf("releasing ports and blocked devices\n");
    release_ports();

    g_printf("calling shared memory exit\n");
    shared_memory_exit();
    g_printf("calling HMA exit\n");
    hma_exit();
    {
	extern void close_uhook(void);
	close_uhook();
    }
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
    {
      extern void ipx_close(void);
      ipx_close();
    }
#endif

    g_printf("calling close_all_printers\n");
    close_all_printers();

    g_printf("calling serial_close\n");
    serial_close();
    g_printf("calling mouse_close\n");
    dosemu_mouse_close();

    flush_log();

    /* remove per process tmpdir and its contents */
    {
       char *command = strcatdup("rm -rf >/dev/null 2>&1 ", TMPDIR_PROCESS);
       if (command) {
          priv_drop(); /* drop any priviledges before running system() !! */
          system(command);
       }
    }

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
#endif


void
activate(int con_num)
{
    if (in_ioctl) {
	k_printf("KBD: can't ioctl for activate, in a signal handler\n");
	do_ioctl(kbd_fd, VT_ACTIVATE, con_num);
    } else
	do_ioctl(kbd_fd, VT_ACTIVATE, con_num);
}
