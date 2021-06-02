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
#include <assert.h>
#include <locale.h>
#include <pthread.h>

#include "version.h"
#include "memory.h"
#include "mhpdbg.h"
#include "debug.h"

#include "emu.h"

#include "bios.h"
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
#include "pktdrvr.h"
#include "ne2000.h"
#include "dma.h"
#include "hlt.h"
#include "coopth.h"
#include "keyboard/keyb_server.h"
#include "sig.h"
#include "sound.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif
#include "kvm.h"

static int ld_tid;
static int can_leavedos;
static int leavedos_code;
static int leavedos_called;
static pthread_mutex_t ld_mtx = PTHREAD_MUTEX_INITIALIZER;
union vm86_union vm86u;

volatile __thread int fault_cnt;
volatile int in_vm86;
int terminal_pipe;
int terminal_fd = -1;
int kernel_version_code;
int console_fd = -1;
int mem_fd = -1;
int fatalerr;
int in_leavedos;
pthread_t dosemu_pthread_self;
char * const *dosemu_envp;
FILE *real_stderr;

#define MAX_EXIT_HANDLERS 5
struct exit_hndl {
  void (*handler)(void);
};
static struct exit_hndl exit_hndl[MAX_EXIT_HANDLERS];
static int exit_hndl_num;

static void __leavedos_main(int code, int sig);

static int find_boot_drive(void)
{
    int i;
    for (i = 0; i < config.fdisks; i++) {
	if (disktab[i].boot)
	    return i;
    }
    FOR_EACH_HDISK(i,
	if (disk_is_bootable(&hdisktab[i]))
	    return HDISK_NUM(i);
    );
    return -1;
}

void boot(void)
{
    unsigned buffer;
    struct disk    *dp = NULL;

    if (config.try_freedos && config.hdiskboot == -1 &&
	    config.hdisks > 0 && !disk_is_bootable(&hdisktab[0])) {
	c_printf("Applying freedos boot work-around\n");
	config.swap_bootdrv = 1;
    }
    if (config.hdiskboot == -1)
	config.hdiskboot = find_boot_drive();
    switch (config.hdiskboot) {
    case -1:
	error("Bootable drive not found, exiting\n");
	leavedos(16);
	return;
    case 0:
	if (config.fdisks > 0)
	    dp = &disktab[0];
	else {
	    error("Drive A: not defined, can't boot!\n");
	    leavedos(71);
	}
	break;
    case 1:
      {
	int d = 1;
	if (config.fdisks > 1) {
	    if (config.swap_bootdrv) {
		struct disk tmp = disktab[1];
		disktab[1] = disktab[0];
		disktab[0] = tmp;
		disktab[0].drive_num = disktab[1].drive_num;
		disktab[1].drive_num = tmp.drive_num;
		d = 0;
		disk_reset();
	    }
	    dp = &disktab[d];
	} else if (config.fdisks == 1) {
	    dp = &disktab[0];
	} else {
	    error("Drive B: not defined, can't boot!\n");
	    leavedos(71);
	}
	break;
      }
    default:
      {
	int d = config.hdiskboot - 2;
	struct disk *dd = hdisk_find(d | 0x80);
	struct disk *cc = hdisk_find(0x80);
	if (config.swap_bootdrv && d && dd) {
	    dd->drive_num = 0x80;
	    cc->drive_num = d | 0x80;
	    config.hdiskboot = 2;
	    d = 0;
	    disk_reset();
	}
	if (dd)
	    dp = dd;
	else {
	    error("Drive %c not defined, can't boot!\n", d + 'C');
	    leavedos(71);
	}
	if (dp->type != DIR_TYPE && dp->drive_num != 0x80) {
	    error("Boot from drive %c is not possible.\n", d + 'C');
	    error("@Fix the $_hdimage setting or enable $_swap_bootdrive.\n");
	    leavedos(72);
	}
	break;
      }
    }

    disk_close();
    disk_open(dp);

    buffer = 0x7c00;

    if (dp->type == PARTITION) {/* we boot partition boot record, not MBR! */
	d_printf("Booting partition boot record from part=%s....\n", dp->dev_name);
	if (dos_read(dp->fdesc, buffer, SECTOR_SIZE) != SECTOR_SIZE) {
	    error("reading partition boot sector using partition %s.\n", dp->dev_name);
	    leavedos(16);
	}
    } else if (dp->floppy) {
	if (read_sectors(dp, buffer, 0, 1) != SECTOR_SIZE) {
	    error("can't boot from %s, using harddisk\n", dp->dev_name);
	    dp = hdisktab;
	    goto mbr;
	}
    } else {
	if (dp->type == DIR_TYPE) {
	    if (!disk_is_bootable(dp) || !disk_validate_boot_part(dp)) {
		error("Drive unbootable, exiting\n");
		leavedos(16);
	    }
	}
mbr:
	if (read_mbr(dp, buffer) != SECTOR_SIZE) {
	    error("can't boot from hard disk\n");
	    leavedos(16);
	}
    }
    disk_close();
}

static int c_chk(void)
{
    /* return 1 if the context is safe for coopth to do a thread switch */
    return !in_dpmi_pm();
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
int main(int argc, char **argv, char * const *envp)
{
    dosemu_envp = envp;
    setlocale(LC_ALL,"");
    srand(time(NULL));
    memset(&config, 0, sizeof(config));

    /* NOW! it is safe to touch the priv code.  */
    priv_init();  /* This must come first! */

    /* Before we even try to give options to the parser,
     * we pre-filter some dangerous options and delete them
     * from the arguments list
     */
    secure_option_preparse(&argc, argv);

    /* the transposal of (config_|stdio_)init allows the addition of -o */
    /* to specify a debug out filename, if you're wondering */

    port_init();		/* setup port structures, before config! */
    version_init();		/* Check the OS version */
    stdio_init();		/* initialize stdio & open debug file */
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
    print_version();            /* log version information */
    memcheck_init();
    time_setting_init();	/* get the startup time */
    /* threads can be created only after signal_pre_init() so
     * it should be above device_init(), iodev_init(), cpu_setup() etc */
    signal_pre_init();          /* initialize sig's & sig handlers */
    cpu_setup();		/* setup the CPU */
    pci_setup();
    device_init();		/* priv initialization of video etc. */
    extra_port_init();		/* setup ports dependent on config */
    SIG_init();			/* Silly Interrupt Generator */
    pkt_priv_init();            /* initialize the packet driver interface */
    ne2000_priv_init();

    mapping_init();		/* initialize mapping drivers */
    low_mem_init();		/* initialize the lower 1Meg */

    if (can_do_root_stuff && !under_root_login) {
        g_printf("dropping root privileges\n");
	open_kmem();
    }
    priv_drop();

    init_hardware_ram();         /* map the direct hardware ram */
    map_video_bios();           /* map (really: copy) the video bios */
    close_kmem();

    /* the following duo have to be done before others who use hlt or coopth */
    hlt_init();
    coopth_init();
    coopth_set_ctx_checker(c_chk);
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
     * the signal_pre_init(), which right now blocks the signals. */
    iodev_init();		/* initialize devices */
    init_all_DOS_tables();	/* longest init function! needs to be optimized */
    dos2tty_init();
    signal_init();              /* initialize sig's & sig handlers */
    if (config.exitearly) {
      dbug_printf("Leaving DOS before booting\n");
      leavedos(0);
    }
    g_printf("EMULATE\n");

    fflush(stdout);

#ifdef USE_MHPDBG
    mhp_debug(DBG_INIT, 0, 0);
#endif
    timer_interrupt_init();	/* start sending int 8h int signals */

    /* map KVM memory */
    if (config.cpu_vm == CPUVM_KVM || config.cpu_vm_dpmi == CPUVM_KVM)
      set_kvm_memory_regions();

    cpu_reset();

    can_leavedos = 1;

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
    SETIVEC(0x19, BIOSSEG, INT_OFF(0x19));
    dbug_printf("DOS ctrl-alt-del requested.  Rebooting!\n");
    real_run_int(0x19);
}

int register_exit_handler(void (*handler)(void))
{
  assert(exit_hndl_num < MAX_EXIT_HANDLERS);
  exit_hndl[exit_hndl_num].handler = handler;
  exit_hndl_num++;
  return 0;
}

static void leavedos_thr(void *arg)
{
    dbug_printf("leavedos thread started\n");
    /* this may require working vm86() */
    video_early_close();
    dbug_printf("leavedos thread ended\n");
}

/* "graceful" shutdown */
void __leavedos(int code, int sig, const char *s, int num)
{
    int tmp;
    dbug_printf("leavedos(%s:%i|%i) called - shutting down\n", s, num, sig);
    if (in_leavedos)
      {
       error("leavedos called recursively, forgetting the graceful exit!\n");
       _exit(1);
      }

    if (!can_leavedos) {
      config.exitearly = 1;
      return;
    }

    in_leavedos++;
    if (fault_cnt > 0) {
      dosemu_error("leavedos() called from within a signal context!\n");
      leavedos_main(sig);
      return;
    }

#ifdef USE_MHPDBG
    /* try to notify dosdebug */
    mhp_exit_intercept(sig);
#endif

    /* try to regain control of keyboard and video first */
    keyb_close();
    /* abandon current thread if any */
    coopth_abandon();
    /* close coopthreads-related stuff first */
    dpmi_done();
    dos2tty_done();
    if (!config.exitearly) {  // in exitearly case nothing to join
      /* try to clean up threads */
      tmp = coopth_flush(vm86_helper);
      if (tmp)
        dbug_printf("%i threads still active\n", tmp);
      coopth_start(ld_tid, leavedos_thr, NULL);
      /* vc switch may require vm86() so call it while waiting for thread */
      coopth_join(ld_tid, vm86_helper);
    }
    __leavedos_main(code, sig);
}

static void __leavedos_main(int code, int sig)
{
    int i;

#ifdef USE_MHPDBG
    g_printf("closing debugger pipes\n");
    mhp_close();
#endif
    /* async signals must be disabled after coopthreads are joined, but
     * before coopth_done(). */
    signal_done();
    /* now it is safe to shut down coopth. Can be done any later, if need be */
    coopth_done();
    dbug_printf("coopthreads stopped\n");

    video_close();
    if (config.cpu_vm == CPUVM_KVM || config.cpu_vm_dpmi == CPUVM_KVM)
      kvm_done();
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

    SIG_close();

    g_printf("calling keyboard_close\n");
    iodev_term();

#if defined(X86_EMULATOR)
    /* if we are here with config.cpuemu>1 something went wrong... */
    if (IS_EMU()) {
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
    g_printf("calling mapping_close()\n");
    mapping_close();

    g_printf("calling close_all_printers\n");
    close_all_printers();
    ioselect_done();

    for (i = 0; i < exit_hndl_num; i++)
      exit_hndl[i].handler();

    flush_log();

    /* We don't need to use _exit() here; this is the graceful exit path. */
    exit(sig ? sig + 128 : code);
}

void __leavedos_main_wrp(int code, int sig, const char *s, int num)
{
    dbug_printf("leavedos_main(%s:%i|%i) called - shutting down\n", s, num, sig);
    __leavedos_main(code, sig);
}

void leavedos_from_thread(int code)
{
    pthread_mutex_lock(&ld_mtx);
    leavedos_code = code;
    leavedos_called++;
    pthread_mutex_unlock(&ld_mtx);
}

void check_leavedos(void)
{
    int ld_code, ld_called;
    pthread_mutex_lock(&ld_mtx);
    ld_code = leavedos_code;
    ld_called = leavedos_called;
    leavedos_called = 0;
    pthread_mutex_unlock(&ld_mtx);
    if (ld_called)
        leavedos(ld_code);
}

void hardware_run(void)
{
	run_sb(); /* Beat Karcher to this one .. 8-) - AM */
	keyb_server_run();
	rtc_run();
}

