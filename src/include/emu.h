/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* dos emulator, Matthias Lautner 
 * Extensions by Robert Sanders, 1992-93
 *
 */

#ifndef EMU_H
#define EMU_H

#include "config.h"

#include <sys/types.h>
#include <setjmp.h>
#include <signal.h> 

#if defined(HAVE_KEYBOARD_V1) && (HAVE_KEYBOARD_V1 > 1)
  #error "Sorry, wrong keyboard code version for this DOSEMU version"
#endif

#if defined(HAVE_UNICODE_KEYB) && (HAVE_UNICODE_KEYB != 2)
  #error "Sorry, wrong unicode keyboard code version for this DOSEMU version"
#endif

#include "types.h"
#include "extern.h"
#include "machcompat.h"
#include "cpu.h"
#include "vm86plus.h"
#include "priv.h"
#include "mouse.h"

#include "extern.h"

#if 1 /* Set to 1 to use Silly Interrupt generator */
#define SIG 1
typedef struct { int fd; int irq; } SillyG_t;
extern SillyG_t *SillyG;
extern int SillyG_do_irq(void);
extern inline void irq_select(void);
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
EXTERN fd_set fds_sigio, fds_no_sigio;
EXTERN unsigned int not_use_sigio INIT(0);
EXTERN int terminal_pipe;
EXTERN int terminal_fd INIT(-1);
EXTERN int running_kversion INIT(0);

EXTERN char *cstack[16384];

/* this is DEBUGGING code! */
EXTERN int sizes INIT(0);

EXTERN int screen, screen_mode;

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

EXTERN int in_vm86 INIT(0);

EXTERN int scanseq;

#if 0
void dos_ctrl_alt_del(void);	/* disabled */
#endif
extern jmp_buf NotJEnv;

EXTERN void run_vm86(void);
EXTERN void loopstep_run_vm86(void);
EXTERN void vm86_GP_fault(void);

EXTERN void do_call_back(Bit32u codefarptr);
EXTERN void do_intr_call_back(int intno, int inter);

#define NOWAIT  0
#define WAIT    1
#define TEST    2
#define POLL    3

void getKeys(void);

#include "dosemu_debug.h"

     void char_out(u_char, int);

     struct ioctlq {
       int fd, req, param3;
       int queued;
     };

     void do_queued_ioctl(void);
     int queue_ioctl(int, int, int), do_ioctl(int, int, int);
     void keybuf_clear(void);

EXTERN u_char in_sighandler;
/* one-entry queue ;-( for ioctl's */
EXTERN struct ioctlq iq INIT({0, 0, 0, 0}); 
EXTERN u_char in_ioctl INIT(0);
EXTERN struct ioctlq curi INIT({0, 0, 0, 0});


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

#if 0
#define RPT_SYSCALL(sc) ({ int s_tmp, s_err; \
   do { \
	  s_tmp = sc; \
	  s_err = errno; \
	  if (errno == EINTR) {\
	    g_printf("Recursive run_irqs() RPT_SYSCALL()\n"); \
	    handle_signals(); \
	/*    run_irqs(); */ \
	  } \
      } while ((s_tmp == -1) && (s_err == EINTR)); \
  s_tmp; })
#else
#define RPT_SYSCALL(sc) (sc)
#endif

#define RPT_SYSCALL2(sc) ({ int s_tmp; \
   do { \
	  s_tmp = sc; \
	  s_err = errno; \
	  if (errno == EINTR) {\
	    g_printf("Recursive run_irqs() RPT_SYSCALL2()\n"); \
	    handle_signals(); \
	/*    run_irqs(); */ \
	  } \
      } while ((s_tmp == -1) ); \
  s_tmp; })

/* would like to have this in memory.h (but emu.h is included before memory.h !) */
#define HARDWARE_RAM_START 0xc8000
#define HARDWARE_RAM_STOP  0xf0000

typedef struct vesamode_type_struct {
  struct vesamode_type_struct *next;
  unsigned width, height, color_bits;
} vesamode_type;


     typedef struct config_info {
       int hdiskboot;

#ifdef X86_EMULATOR
       boolean cpuemu;
#endif
       int CPUSpeedInMhz;
       /* for video */
       boolean console;
       boolean console_video;
       boolean vga;
       boolean X;
       u_short cardtype;
       u_short chipset;
       boolean pci;
       boolean pci_video;
       u_short gfxmemsize;		/* for SVGA card, in K */
       /* u_short term_method; */	/* Terminal method: ANSI or NCURSES */
       u_short term_color;		/* Terminal color support on or off */
       /* u_short term_updatelines; */	/* Amount to update at a time */
       u_short term_updatefreq;		/* Terminal update frequency */
       u_short term_charset;		/* Terminal Character set */
       u_short term_esc_char;	        /* ASCII value used to access slang help screen */
       /* u_short term_corner; */       /* Update char at lower-right corner */
       u_short X_updatelines;           /* Amount to update at a time */
       u_short X_updatefreq;            /* X update frequency */
       char    *X_display;              /* X server to use (":0") */
       char    *X_title;                /* X window title */
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
       u_short vgaemu_memsize;		/* for VGA emulation */
       vesamode_type *vesamode_list;	/* chained list of VESA modes */
       int     X_lfb;			/* support VESA LFB modes */
       int     X_pm_interface;		/* support protected mode interface */
       int     X_background_pause;	/* pause xdosemu if it loses focus */
       boolean fullrestore;
       boolean force_vt_switch;         /* in case of console_video force switch to emu VT at start */
       int     dualmon;

       boolean console_keyb;
       boolean kbd_tty;
       boolean X_keycode;	/* use keycode field of event structure */
       boolean exitearly;
       int     realcpu;
       boolean mathco, smp, cpummx;
       boolean ipxsup;
       int     vnet;
       char   *netdev;
       boolean pktdrv;
       boolean dosbanner;
       boolean emuretrace;
       boolean rdtsc;
       boolean mouse_flag;
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

       u_short speaker;		/* 0 off, 1 native, 2 emulated */
       u_short fdisks, hdisks;
       u_short num_lpt;
       u_short num_ser;
       mouse_t mouse;

       int pktflags;		/* global flags for packet driver */

       unsigned int update, freq;	/* temp timer magic */
       unsigned long cpu_spd;		/* (1/speed)<<32 */
       unsigned long cpu_tick_spd;	/* (1.19318/speed)<<32 */

       unsigned int hogthreshold;

       int mem_size, xms_size, ems_size, dpmi, max_umb;
       unsigned int ems_frame;
       char must_spare_hardware_ram;
       char hardware_pages[ ((HARDWARE_RAM_STOP-HARDWARE_RAM_START) >> 12)+1 ];


       int sillyint;            /* IRQ numbers for Silly Interrupt Generator 
       				   (bitmask, bit3..15 ==> IRQ3 .. IRQ15) */

       struct keytable_entry *keytable;
       struct keytable_entry *altkeytable;

       unsigned short detach;
       unsigned char *debugout;
       unsigned char *pre_stroke;  /* pointer to keyboard pre strokes */
       unsigned char *pre_stroke_mem;

       /* Lock File business */
       boolean full_file_locks;
       char *tty_lockdir;	/* The Lock directory  */
       char *tty_lockfile;	/* Lock file pretext ie LCK.. */
       boolean tty_lockbinary;	/* Binary lock files ? */

       /* LFN support */
       boolean lfn;

       /* type of mapping driver */
       char *mappingdriver;

       /* list of arbitrary features
        * (at minimum 1, will be increased when needed)
        * The purpose of these parameters is to switch between code
        * or code pieces that in principal do the same, but for some
        * unknown reasons behave different between machines.
        * If a 'features' becomes obsolete (problem solved) it will
        * remain dummy for a while before re-used.
        *
        * NOTE: 'features' are not subject to permanent documentation!
        *       They should be considered 'temporary hacks'
        *
        * Currently assigned:
        *
        *   features[0]		use new(0) or old(1) Slangcode
        */
       int features[1];

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

#undef cli
#undef sti
EXTERN void cli(void);
EXTERN void sti(void);
EXTERN int port_readable(unsigned short);
EXTERN int port_writeable(unsigned short);
EXTERN unsigned char read_port(unsigned short);
EXTERN int write_port(unsigned int, unsigned short);
EXTERN __inline__ void parent_nextscan(void);
EXTERN __inline__ void disk_close(void);
EXTERN void cpu_setup(void);
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
EXTERN void serial_init(void);
EXTERN void close_all_printers(void);
EXTERN void serial_close(void);
EXTERN void disk_close_all(void);
EXTERN void init_all_printers(void);
EXTERN int mfs_inte6(void);
EXTERN void pkt_helper(void);
EXTERN short pop_word(struct vm86_regs *);
EXTERN void ems_init(void);
EXTERN void leavedos(int) NORETURN;
EXTERN void add_to_io_select(int, u_char, void(*)(void));
EXTERN void remove_from_io_select(int, u_char);
EXTERN void sigquit(int);
#ifdef __linux__
EXTERN void sigalrm(int, struct sigcontext_struct);
EXTERN void e_sigalrm(struct sigcontext_struct *context);
EXTERN void sigio(int, struct sigcontext_struct);
EXTERN int dosemu_sigaction(int sig, struct sigaction *new, struct sigaction *old);
EXTERN void SIG_init(void);
EXTERN void SIG_close(void);
#endif

/* set if sigaltstack(2) is available */
EXTERN int have_working_sigaltstack;

/* signals for Linux's process control of consoles */
#define SIG_RELEASE     SIGWINCH
#define SIG_ACQUIRE     SIGUSR2

 /* DANG_BEGIN_REMARK
  * We assume system call restarting... under linux 0.99pl8 and earlier,
  * this was the default.  SA_RESTART was defined in 0.99pl8 to explicitly
  * request restarting (and thus does nothing).  However, if this ever
  * changes, I want to be safe
  * DANG_END_REMARK
  */
#ifndef SA_RESTART
#define SA_RESTART 0
#error SA_RESTART Not defined
#endif

/* DANG_BEGIN_FUNCTION NEWSETQSIG
 *
 * arguments:
 * sig - the signal to have a handler installed to.
 * fun - the signal handler function to install
 *
 * description:
 *  All signals that wish to be handled properly in context with the
 * execution of vm86() mode, and signals that wish to use non-reentrant
 * functions should add themselves to the ADDSET_SIGNALS_THAT_QUEUE define
 * and use SETQSIG(). To that end they will also need to be set up in an
 * order such as SIGIO.
 *
 * DANG_END_FUNCTION
 *
 */
#define ADDSET_SIGNALS_THAT_QUEUE(x) \
do { \
       sigaddset(x, SIGIO); \
       sigaddset(x, SIGALRM); \
       sigaddset(x, SIG_RELEASE); \
       sigaddset(x, SIG_ACQUIRE); \
} while(0)

#ifdef __linux__
#ifndef SA_ONSTACK
#define SA_ONSTACK 0
#undef HAVE_SIGALTSTACK
#endif
#define SignalHandler __sighandler_t
#define NEWSETQSIG(sig, fun) \
	sa.sa_handler = (__sighandler_t)fun; \
	sa.sa_flags = SA_RESTART; \
	sigemptyset(&sa.sa_mask); \
	ADDSET_SIGNALS_THAT_QUEUE(&sa.sa_mask); \
	if (have_working_sigaltstack) { \
		sa.sa_flags |= SA_ONSTACK; \
		sigaction(sig, &sa, NULL); \
	} else { \
		/* Point to the top of the stack, minus 4 \
		   just in case, and make it aligned  */ \
		sa.sa_restorer = \
		(void (*)(void)) (((unsigned int)(cstack) + sizeof(cstack) - 4) & ~3); \
		dosemu_sigaction(sig, &sa, NULL); \
	}

#define SETSIG(sig, fun) \
	sa.sa_handler = (SignalHandler)fun; \
	sa.sa_flags = SA_RESTART; \
	sigemptyset(&sa.sa_mask); \
	sigaddset(&sa.sa_mask, SIGALRM); \
	sigaction(sig, &sa, NULL);

#define NEWSETSIG(sig, fun) \
	sa.sa_handler = (__sighandler_t) fun; \
	sa.sa_flags = SA_RESTART; \
	sigemptyset(&sa.sa_mask); \
	sigaddset(&sa.sa_mask, SIGALRM); \
	if (have_working_sigaltstack) { \
		sa.sa_flags |= SA_ONSTACK; \
		sigaction(sig, &sa, NULL); \
	} else { \
		/* Point to the top of the stack, minus 4 \
		   just in case, and make it aligned  */ \
		sa.sa_restorer = \
		(void (*)(void)) (((unsigned int)(cstack) + sizeof(cstack) - 4) & ~3); \
		dosemu_sigaction(sig, &sa, NULL); \
	}
#endif

EXTERN inline void SIGNAL_save( void (*signal_call)(void) );
EXTERN inline void handle_signals(void);


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
EXTERN void hardware_setup(void);
EXTERN void memory_init(void);
EXTERN void map_hardware_ram(void);
EXTERN void map_video_bios(void);
EXTERN void stdio_init(void);
EXTERN void time_setting_init(void);
EXTERN void timer_interrupt_init(void);
EXTERN void low_mem_init(void);
EXTERN void print_version(void);
EXTERN void keyboard_flags_init(void);
EXTERN void video_config_init(void);
EXTERN void printer_init(void);
EXTERN void video_close(void);
EXTERN void hma_exit(void);
EXTERN void ems_helper(void);
EXTERN boolean_t ems_fn(struct vm86_regs *);
EXTERN void mouse_helper(void);
EXTERN void cdrom_helper(void);
EXTERN void boot(void);
EXTERN int pkt_int(void);
EXTERN void read_next_scancode_from_queue (void);
EXTERN unsigned short detach (void);
EXTERN void disallocate_vt (void);
EXTERN void restore_vt (unsigned short vt);
EXTERN void HMA_init(void);
EXTERN void HMA_MAP(int HMA);

extern char *Path_cdrom[];

#endif /* EMU_H */
