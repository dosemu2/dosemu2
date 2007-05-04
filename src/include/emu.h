/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* dos emulator, Matthias Lautner 
 * Extensions by Robert Sanders, 1992-93
 *
 */

#ifndef EMU_H
#define EMU_H

#include "config.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <setjmp.h>
#include <signal.h> 

#include "types.h"
#include "extern.h"
#include "machcompat.h"
#include "cpu.h"
#include "priv.h"
#include "mouse.h"

#include "extern.h"

#if 1 /* Set to 1 to use Silly Interrupt generator */
#ifdef __i386__
#define SIG 1
typedef struct { int fd; int irq; } SillyG_t;
extern SillyG_t *SillyG;
#endif
#endif

#define inline __inline__

#define BIT(x)  	(1<<x)

#define us unsigned short

/*
 * DANG_BEGIN_REMARK
   The `vm86_struct` is used to pass all the necessary status/registers to
   DOSEMU when running in vm86 mode.
 * DANG_END_REMARK
*/ 

EXTERN struct vm86plus_struct vm86s INIT ( {
   {0},0,0,0,{{0}},{{0}}, {0}
} );

EXTERN volatile sig_atomic_t signal_pending INIT(0);
EXTERN volatile int fault_cnt INIT(0);
EXTERN fd_set fds_sigio, fds_no_sigio;
EXTERN unsigned int not_use_sigio INIT(0);
EXTERN int terminal_pipe;
EXTERN int terminal_fd INIT(-1);
EXTERN int running_kversion INIT(0);

EXTERN char *(*cstack)[16384];

EXTERN int screen_mode;

/* number of highest vid page - 1 */
EXTERN int max_page INIT(7);

#if 0
/*
 * 1) this stuff is unused
 * 2) it should be FORBIDDEN to use global names less than 4 chars long!
 */
EXTERN char *cl,		/* clear screen */
*le,				/* cursor left */
*cm,				/* goto */
*ce,				/* clear to end */
*sr,				/* scroll reverse */
*so,				/* stand out start */
*se,				/* stand out end */
*md,				/* hilighted */
*mr,				/* reverse */
*me,				/* normal */
*ti,				/* terminal init */
*te,				/* terminal exit */
*ks,				/* init keys */
*ke,				/* ens keys */
*vi,				/* cursor invisible */
*ve;				/* cursor normal */
#endif

/* the fd for the keyboard */ 
EXTERN int console_fd INIT(-1);

/* the file descriptor for /dev/mem mmap'ing */
EXTERN int mem_fd INIT(-1);
EXTERN int in_readkeyboard;

EXTERN volatile int in_vm86 INIT(0);

EXTERN int scanseq;

void dos_ctrl_alt_del(void);	/* disabled */
extern sigjmp_buf NotJEnv;

EXTERN void run_vm86(void);
EXTERN void loopstep_run_vm86(void);
EXTERN void vm86_GP_fault(void);

EXTERN void do_call_back(Bit32u codefarptr);
EXTERN void callback_return(void);
EXTERN void do_intr_call_back(int intno);

#define NOWAIT  0
#define WAIT    1
#define TEST    2
#define POLL    3

void getKeys(void);

#include "dosemu_debug.h"

     void char_out(u_char, int);

     void keybuf_clear(void);

/* this macro can be safely wrapped around a system call with no side
 * effects; using a feature of GCC, it returns the same value as the
 * function call argument inside.
 *
 * this is best used in places where the errors can't be sanely handled,
 * or are not expected...
 */
#define DOS_SYSCALL(sc) ({ int s_tmp = (int)sc; \
  if (s_tmp == -1) \
    error("SYSCALL ERROR: %d, *%s* in file %s, line %d: expr=\n\t%s\n", \
	  errno, strerror(errno), __FILE__, __LINE__, #sc); \
  s_tmp; })

#define SILENT_DOS_SYSCALL(sc) sc

#if 1
#define RPT_SYSCALL(sc) ({ int s_tmp, s_err; \
   do { \
	  s_tmp = sc; \
	  s_err = errno; \
      } while ((s_tmp == -1) && (s_err == EINTR)); \
  s_tmp; })
#else
#define RPT_SYSCALL(sc) (sc)
#endif

typedef struct vesamode_type_struct {
  struct vesamode_type_struct *next;
  unsigned width, height, color_bits;
} vesamode_type;


     typedef struct config_info {
       int hdiskboot;

#ifdef X86_EMULATOR
       boolean cpuemu;
       boolean cpusim;
#endif
       int CPUSpeedInMhz;
       /* for video */
       boolean console_video;
       boolean vga;
       boolean X;
       boolean X_fullscreen;
       u_short cardtype;
       u_short chipset;
       boolean pci;
       boolean pci_video;
       long gfxmemsize;		/* for SVGA card, in K */
       /* u_short term_method; */	/* Terminal method: ANSI or NCURSES */
       u_short term_color;		/* Terminal color support on or off */
       /* u_short term_updatelines; */	/* Amount to update at a time */
       u_short term_updatefreq;		/* Terminal update frequency */
       u_short term_esc_char;	        /* ASCII value used to access slang help screen */
       char    *xterm_title;	        /* xterm/putty window title */
       /* u_short term_corner; */       /* Update char at lower-right corner */
       u_short X_updatelines;           /* Amount to update at a time */
       u_short X_updatefreq;            /* X update frequency */
       char    *X_display;              /* X server to use (":0") */
       char    *X_title;                /* X window title */
       int X_title_show_appname;        /* show name of running app in caption */
       char    *X_icon_name;
       char    *X_font;
       char    *X_mgrab_key;		/* KeySym name to activate mouse grab */
					/* "" turns it of, NULL gives the default ("Home") */
       int     X_blinkrate;
       int     X_sharecmap;
       int     X_mitshm;                /* use MIT SHM extension */
       int     X_fixed_aspect;          /* keep initial aspect ratio while resizing windows */
       int     X_aspect_43;             /* set aspect ratio to 4:3 */
       int     X_lin_filt;              /* interpolate linear */
       int     X_bilin_filt;            /* dto, bilinear */
       int     X_winsize_x;             /* initial window width */
       int     X_mode13fact;            /* initial size factor for mode 0x13 */
       int     X_winsize_y;             /* initial window height */
       unsigned X_gamma;		/* gamma correction value */
       u_long vgaemu_memsize;		/* for VGA emulation */
       vesamode_type *vesamode_list;	/* chained list of VESA modes */
       int     X_lfb;			/* support VESA LFB modes */
       int     X_pm_interface;		/* support protected mode interface */
       int     X_background_pause;	/* pause xdosemu if it loses focus */
       boolean fullrestore;
       boolean force_vt_switch;         /* in case of console_video force switch to emu VT at start */
       int     dualmon;

       int     console_keyb;
       boolean kbd_tty;
       boolean X_keycode;	/* use keycode field of event structure */
       boolean exitearly;
       boolean quiet;
       int     realcpu;
       boolean mathco, smp, cpuprefetcht0, cpufxsr;
       boolean ipxsup;
       long    ipx_net;
       int     vnet;
       char   *netdev;
       boolean pktdrv;
       boolean dosbanner;
       boolean emuretrace;
       boolean rdtsc;
       boolean mapped_bios;	/* video BIOS */
       char *vbios_file;	/* loaded VBIOS file */
       boolean vbios_copy;
       int vbios_seg;           /* VGA-BIOS-segment for mapping */
       int vbios_size;          /* size of VGA-BIOS (64K for vbios_seg=0xe000
       						     32K for vbios_seg=0xc000) */
       boolean vbios_post;

       boolean bootdisk;	/* Special bootdisk defined */
       int  fastfloppy;
       char *emusys;		/* map CONFIG.SYS to CONFIG.EMU */
       char *emuini;           /* map system.ini to  system.EMU */
       char *install;          /* directory to point ~/.dosemu/drives/c to */

       u_short speaker;		/* 0 off, 1 native, 2 emulated */
       u_short fdisks, hdisks;
       u_short num_lpt;
       u_short num_ser;
       mouse_t mouse;

       int pktflags;		/* global flags for packet driver */

       int update, freq;	/* temp timer magic */
       unsigned long cpu_spd;		/* (1/speed)<<32 */
       unsigned long cpu_tick_spd;	/* (1.19318/speed)<<32 */

       int hogthreshold;

       int mem_size, ext_mem, xms_size, ems_size, max_umb;
       unsigned int ems_frame;
       int dpmi, pm_dos_api, no_null_checks;
       unsigned long dpmi_base;

       int sillyint;            /* IRQ numbers for Silly Interrupt Generator 
       				   (bitmask, bit3..15 ==> IRQ3 .. IRQ15) */

       struct keytable_entry *keytable;
       struct keytable_entry *altkeytable;

       unsigned short detach;
       char *debugout;
       char *pre_stroke;        /* pointer to keyboard pre strokes */

       /* Lock File business */
       boolean full_file_locks;
       char *tty_lockdir;	/* The Lock directory  */
       char *tty_lockfile;	/* Lock file pretext ie LCK.. */
       boolean tty_lockbinary;	/* Binary lock files ? */

       /* LFN support */
       boolean lfn;

       /* type of mapping driver */
       char *mappingdriver;

       /* List of temporary hacks
        * (at minimum 16, will be increased when needed)
        *
        * If a 'features' becomes obsolete (problem solved) it will
        * remain dummy for a while before re-used.
        *
        * NOTE: 'features' are not subject to permanent documentation!
        *
        * Currently assigned:
        *
        *   (none)
        */
       int features[16];

       /* Time mode is TM_BIOS / TM_PIT / TM_LINUX, see iodev.h */
       int timemode;

       /* Sound emulation */
       int sound;
       uint16_t sb_base;
       uint8_t sb_dma;
       uint8_t sb_hdma;
       uint8_t sb_irq;
       char *sb_dsp;
       char *sb_mixer;
       uint16_t mpu401_base;
       char *sound_driver;
       /* OSS-specific options */
       int oss_min_frags;
       int oss_max_frags;
       int oss_stalled_frags;
       int oss_do_post;
       int oss_min_extra_frags;
       int oss_dac_freq;

       /* joystick */
       char *joy_device[2];
       
       /* range for joystick axis values */
       int joy_dos_min;		/* must be > 0 */
       int joy_dos_max;		/* avoid setting this to > 250 */
       
       int joy_granularity;	/* the higher, the less sensitive - for wobbly joysticks */
       int joy_latency;		/* delay between nonblocking linux joystick reads */

       int cli_timeout;		/* cli timeout hack */
       int pic_watchdog;        /* pic watchdog reschedule hack */      
     }

config_t;


#define SPKR_OFF	0
#define SPKR_NATIVE	1
#define SPKR_EMULATED	2

/*
 * Right now, dosemu only supports two serial ports.
 */
#define SIG_SER		SIGTTIN

#define IO_READ  1
#define IO_WRITE 2
#define IO_RDWR	 (IO_READ | IO_WRITE)

EXTERN int port_readable(unsigned short);
EXTERN int port_writeable(unsigned short);
EXTERN unsigned char read_port(unsigned short);
EXTERN int write_port(unsigned int, unsigned short);
EXTERN __inline__ void parent_nextscan(void);
EXTERN __inline__ void disk_close(void);
EXTERN void cpu_setup(void);
EXTERN void cpu_reset(void);
EXTERN void real_run_int(int);
#define run_int do_int
EXTERN int mfs_redirector(void);
EXTERN int mfs_lfn(void);
EXTERN int int10(void);
EXTERN int int13(void);
EXTERN int int16(void);
EXTERN int int17(void);
EXTERN void io_select(fd_set);
EXTERN void io_select_init(void);
EXTERN int pd_receive_packet(void);
EXTERN int printer_tick(u_long);
EXTERN void floppy_tick(void);
EXTERN void open_kmem(void);
EXTERN void close_kmem(void);
EXTERN int parse_config(char *, char *);
EXTERN void prepare_dexe_load(char *name);
EXTERN void disk_init(void);
EXTERN void disk_reset(void);
EXTERN void serial_init(void);
EXTERN void serial_reset(void);
EXTERN void close_all_printers(void);
EXTERN void serial_close(void);
EXTERN void disk_close_all(void);
EXTERN void init_all_printers(void);
EXTERN int mfs_inte6(void);
EXTERN int mfs_helper(state_t *regs);
EXTERN void pkt_helper(void);
EXTERN short pop_word(struct vm86_regs *);
EXTERN void leavedos(int) NORETURN;
EXTERN void add_to_io_select(int, u_char, void(*)(void));
EXTERN void remove_from_io_select(int, u_char);
#ifdef __linux__
EXTERN void SIG_init(void);
EXTERN void SIG_close(void);
#endif

EXTERN unsigned long int stack_init_top INIT(0xffffffff);
EXTERN unsigned long int stack_init_bot INIT(0xffffffff);

/* signals for Linux's process control of consoles */
#define SIG_RELEASE     SIGUSR1
#define SIG_ACQUIRE     SIGUSR2

EXTERN inline void SIGNAL_save( void (*signal_call)(void) );
extern void handle_signals(void);
extern void handle_signals_force_reentry(void);

extern void addset_signals_that_queue(sigset_t *x);
extern void registersig(int sig, void (*handler)(struct sigcontext_struct *));
extern void init_handler(struct sigcontext_struct *scp);
#ifdef __x86_64__
extern int check_fix_fs_gs_base(unsigned char prefix);
#endif

/* 
 * DANG_BEGIN_REMARK
 * DOSEMU keeps system wide configuration status in a structure
 * called config.
 * DANG_END_REMARK
 */
EXTERN struct config_info config;


/*
 * DANG_BEGIN_REMARK
 * The var `fatalerr` can be given a true value at any time to have DOSEMU
 * exit on the next return from vm86 mode.
 * DANG_END_REMARK
 */
EXTERN int fatalerr INIT(0);
EXTERN int in_leavedos;

/*
 * DANG_BEGIN_REMARK
 * The var 'running_DosC' is set by the DosC kernel and is used to handle
 * some things differently, e.g. the redirector.
 * It interfaces via INTe6,0xDC (DOS_HELPER_DOSC), but only if running_DosC
 * is !=0. At the very startup DosC issues a INTe6,0xdcDC to set running_DosC
 * with the contents of BX (which is the internal DosC version).
 * DANG_END_REMARK
 */
EXTERN int running_DosC INIT(0);
EXTERN int dosc_interface(void);

EXTERN void dump_config_status(void *);
EXTERN void signal_init(void);
EXTERN void device_init(void);
EXTERN void memory_init(void);
EXTERN void map_video_bios(void);
EXTERN void stdio_init(void);
EXTERN void time_setting_init(void);
EXTERN void timer_interrupt_init(void);
EXTERN void low_mem_init(void);
EXTERN void print_version(void);
EXTERN void keyboard_flags_init(void);
EXTERN void video_config_init(void);
EXTERN void video_post_init(void);
EXTERN void video_mem_setup(void);
EXTERN void printer_init(void);
EXTERN void printer_mem_setup(void);
EXTERN void video_close(void);
EXTERN void hma_exit(void);
EXTERN void ems_helper(void);
EXTERN boolean_t ems_fn(struct vm86_regs *);
EXTERN void mouse_helper(struct vm86_regs *);
EXTERN void cdrom_helper(unsigned char *, unsigned char *);
EXTERN int mscdex(void);
EXTERN void boot(void);
EXTERN void do_liability_disclaimer_prompt(int stage, int prompt);
EXTERN void install_dos(int post_boot);
EXTERN int pkt_int(void);
EXTERN int ipx_int7a(void);
EXTERN void read_next_scancode_from_queue (void);
EXTERN unsigned short detach (void);
EXTERN void disallocate_vt (void);
EXTERN void restore_vt (unsigned short vt);
EXTERN void HMA_init(void);
EXTERN void HMA_MAP(int HMA);
EXTERN void hardware_run(void);

extern char *Path_cdrom[];

#endif /* EMU_H */
