/* dos emulator, Matthias Lautner 
 * Extensions by Robert Sanders, 1992-93
 *
 */

#ifndef EMU_H
#define EMU_H

#include <features.h>
#include <sys/types.h>
#include <signal.h> 
#if __GLIBC__ > 1
#include <sigcontext.h>
#endif
#include "config.h"
#include "types.h"
#include "extern.h"
#include "machcompat.h"
#include "cpu.h"
#include "vm86plus.h"

/* Note: priv.h needs 'error' !!! */
#define error(f,a...)	 	fprintf(stderr, f, ##a)
#include "priv.h"

#include "extern.h"

#if 1 /* Set to 1 to use Silly Interrupt generator */
#define SIG 1
typedef struct { int fd; int irq; } SillyG_t;
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

#ifdef REQUIRES_VM86PLUS
  EXTERN struct vm86plus_struct vm86s INIT ( {
   {0},0,0,0,{{0}},{{0}}, {0}
  } );
#else
  EXTERN struct vm86_struct vm86s;
#endif

EXTERN fd_set fds_sigio, fds_no_sigio;
EXTERN unsigned int use_sigio INIT(0);
EXTERN unsigned int not_use_sigio INIT(0);
EXTERN int terminal_pipe;
EXTERN int terminal_fd INIT(-1);
EXTERN int running_kversion INIT(0);

/* set to one if runing setuid as root */
#if 1
#define i_am_root 1  /* neadless to have it variable, it will be allways 1
                      * If dosemu isn't suid root, it will terminate
                      */
#else
EXTERN int i_am_root INIT(0);
#endif

/* set, if dosemu was started from a 'root login',
 * unset, if dosemu was started from a 'user login' (running suid root)
 */
EXTERN int under_root_login INIT(0);

EXTERN char *cstack[16384];

/* this is DEBUGGING code! */
EXTERN int sizes INIT(0);

EXTERN char *tmpdir;
EXTERN int screen, screen_mode;

/* number of highest vid page - 1 */
EXTERN int max_page INIT(7);

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

/* the fd for the keyboard */ 
EXTERN int console_fd INIT(-1);
#ifndef NEW_KBD_CODE
  #define kbd_fd console_fd
#endif

/* the file descriptor for /dev/mem when mmap'ing the video mem */
EXTERN int mem_fd INIT(-1);
EXTERN int in_readkeyboard;

/* X-pipes */
EXTERN int keypipe;
EXTERN int mousepipe;

EXTERN int in_vm86 INIT(0);

EXTERN int li, co;	/* lines, columns */
EXTERN int scanseq;
EXTERN int cursor_row;
EXTERN int cursor_col;

#if 0
void dos_ctrl_alt_del(void);	/* disabled */
#endif

EXTERN void run_vm86(void);
EXTERN void     vm86_GP_fault();

#define NOWAIT  0
#define WAIT    1
#define TEST    2
#define POLL    3

void getKeys(void);


struct debug_flags {
  unsigned char
   disk,		/* disk msgs         "d" */
   read,		/* disk read         "R" */
   write,		/* disk write        "W" */
   dos,			/* unparsed int 21h  "D" */
   cdrom,               /* cdrom             "C" */
   video,		/* video             "v" */
   X,			/* X support         "X" */
   keyb,		/* keyboard          "k" */
   io,			/* port I/O          "i" */
   io_trace,		/* port I/O tracing  "T" */
   serial,		/* serial            "s" */
   mouse,		/* mouse             "m" */
   defint,		/* default ints      "#" */
   printer,		/* printer           "p" */
   general,		/* general           "g" */
   config,		/* configuration     "c" */
   warning,		/* warning           "w" */
   hardware,		/* hardware          "h" */
   IPC,			/* IPC               "I" */
   EMS,			/* EMS               "E" */
   xms,			/* xms               "x" */
   dpmi,		/* dpmi              "M" */
   network,		/* IPX network       "n" */
   pd,			/* Packet driver     "P" */
   request,		/* PIC               "r" */
   sound;		/* SOUND	     "S" */
};

#if __GNUC__ >= 2
# define FORMAT(T,A,B)  __attribute__((format(T,A,B)))
#else
# define FORMAT(T,A,B)
#endif

#if __GNUC__ >= 2
# define NORETURN	__attribute__((noreturn))
#else
# define NORETURN
#endif

EXTERN void saytime(char *m_str);

int log_printf(int, const char *,...) FORMAT(printf, 2, 3);

void p_dos_str(char *,...) FORMAT(printf, 1, 2);

#if 0  /* set this to 0, if you want dosemu to honor the -D flags */
 #define NO_DEBUGPRINT_AT_ALL
#endif

extern FILE *dbg_fd;

#define ifprintf(flg,fmt,a...)	do{ if (flg) log_printf(flg,fmt,##a); }while(0)

/* Note: moved it at top of emu.h, we need to define 'error' for priv.h 
  #define error(f,a...)	 	fprintf(stderr, f, ##a) */

#define hard_error(f, a...)	fprintf(stderr, f, ##a)
#define dbug_printf(f,a...)	ifprintf(2,f,##a)
#define flush_log()		{ if (dbg_fd) log_printf(-1, "\n"); }

#ifndef NO_DEBUGPRINT_AT_ALL

#define k_printf(f,a...) 	ifprintf(d.keyb,f,##a)
#define h_printf(f,a...) 	ifprintf(d.hardware,f,##a)
#define v_printf(f,a...) 	ifprintf(d.video,f,##a)
#define X_printf(f,a...)        ifprintf(d.X,f,##a)
#define s_printf(f,a...) 	ifprintf(d.serial,f,##a)
#define p_printf(f,a...) 	ifprintf(d.printer,f,##a)
#define d_printf(f,a...) 	ifprintf(d.disk,f,##a)
#define i_printf(f,a...) 	ifprintf(d.io,f,##a)
#define T_printf(f,a...) 	ifprintf(d.io_trace,f,##a)
#define R_printf(f,a...) 	ifprintf(d.read,f,##a)
#define W_printf(f,a...) 	ifprintf(d.write,f,##a)
#define C_printf(f,a...)        ifprintf(d.cdrom,f,##a)
#define g_printf(f,a...)	ifprintf(d.general,f,##a)
#define x_printf(f,a...)	ifprintf(d.xms,f,##a)
#define D_printf(f,a...)	ifprintf(d.dpmi,f,##a)
#define m_printf(f,a...)	ifprintf(d.mouse,f,##a)
#define I_printf(f,a...) 	ifprintf(d.IPC,f,##a)
#define E_printf(f,a...) 	ifprintf(d.EMS,f,##a)
#define c_printf(f,a...) 	ifprintf(d.config,f,##a)
#define e_printf(f,a...) 	ifprintf(1,f,##a)
#define n_printf(f,a...)        ifprintf(d.network,f,##a)  /* TRB     */
#define pd_printf(f,a...)       ifprintf(d.pd,f,##a)	   /* pktdrvr */
#define r_printf(f,a...)        ifprintf(d.request,f,##a)
#define warn(f,a...)     	ifprintf(d.warning,f,##a)
#define S_printf(f,a...)     	ifprintf(d.sound,f,##a)
#define ds_printf(f,a...)     	ifprintf(d.dos,f,##a)

#else

#define k_printf(f,a...)
#define h_printf(f,a...)
#define v_printf(f,a...)
#define X_printf(f,a...)
#define s_printf(f,a...)
#define p_printf(f,a...)
#define d_printf(f,a...)
#define i_printf(f,a...)
#define R_printf(f,a...)
#define W_printf(f,a...)
#define C_printf(f,a...)
#define g_printf(f,a...)
#define x_printf(f,a...)
#define D_printf(f,a...)
#define m_printf(f,a...)
#define I_printf(f,a...)
#define E_printf(f,a...)
#define c_printf(f,a...)
#define e_printf(f,a...)
#define n_printf(f,a...)
#define pd_printf(f,a...)
#define r_printf(f,a...)
#define warn(f,a...)
#define S_printf(f,a...)
#define ds_printf(f,a...)

#endif

     void char_out(u_char, int);

     struct ioctlq {
       int fd, req, param3;
       int queued;
     };

     void do_queued_ioctl(void);
     int queue_ioctl(int, int, int), do_ioctl(int, int, int);
     void keybuf_clear(void);

     int set_ioperm(int, int, int);

     EXTERN struct debug_flags d INIT({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
     EXTERN u_char in_sighandler, in_ioctl;
/* one-entry queue ;-( for ioctl's */
EXTERN struct ioctlq iq INIT({0, 0, 0, 0}); 
EXTERN u_char in_ioctl INIT(0);
EXTERN struct ioctlq curi INIT({0, 0, 0, 0});


/* int 11h config single bit tests */
#define CONF_FLOP	BIT(0)
#define CONF_MATHCO	BIT(1)
#define CONF_MOUSE	BIT(2)
#define CONF_DMA	BIT(8)
#define CONF_GAME	BIT(12)

/* don't use CONF_NSER with num > 4, CONF_NLPT with num > 3, CONF_NFLOP
 * with num > 4
 */
#define CONF_NSER(c,num)	{c&=~(BIT(9)|BIT(10)|BIT(11)); c|=(num<<9);}
#define CONF_NLPT(c,num) 	{c&=~(BIT(14)|BIT(14)); c|=(num<<14);}
#define CONF_NFLOP(c,num) 	{c&=~(CONF_FLOP|BIT(6)|BIT(7)); \
				   if (num) c|=((num-1)<<6)|CONF_FLOP;}

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

#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

#ifndef True
#define True  TRUE
#endif
#ifndef False
#define False FALSE
#endif

     typedef unsigned char boolean;

/* would like to have this in memory.h (but emu.h is included before memory.h !) */
#define HARDWARE_RAM_START 0xc8000
#define HARDWARE_RAM_STOP  0xf0000

typedef struct vesamode_type_struct {
  struct vesamode_type_struct *next;
  unsigned width, height, color_bits;
} vesamode_type;


     typedef struct config_info {
       int hdiskboot;

       /* for video */
       boolean console;
       boolean console_video;
       boolean graphics;
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
       boolean fullrestore;
       boolean force_vt_switch;         /* in case of console_video force switch to emu VT at start */
       int     dualmon;

       u_short usesX;  /* !=0 if dosemu owns an X window */

       boolean console_keyb;
       boolean kbd_tty;
       boolean X_keycode;	/* use keycode field of event structure */
       boolean exitearly;
       int     realcpu;
       boolean mathco;
       boolean ipxsup;
       boolean keybint;
       boolean dosbanner;
       boolean allowvideoportaccess;
       boolean rdtsc;
       boolean timers;
       boolean mouse_flag;
       boolean mapped_bios;	/* video BIOS */
       boolean mapped_sbios;	/* system BIOS */
       char *vbios_file;	/* loaded VBIOS file */
       boolean vbios_copy;
       int vbios_seg;           /* VGA-BIOS-segment for mapping */
       int vbios_size;          /* size of VGA-BIOS (64K for vbios_seg=0xe000
       						     32K for vbios_seg=0xc000) */

       boolean bootdisk;	/* Special bootdisk defined */
       int  fastfloppy;
       char *emusys;		/* map CONFIG.SYS to CONFIG.EMU */
       char *emubat;		/* map AUTOEXEC.BAT to AUTOEXEC.EMU */
       char *emuini;           /* map system.ini to  system.EMU */

       u_short speaker;		/* 0 off, 1 native, 2 emulated */
       u_short fdisks, hdisks;
       u_short num_lpt;
       u_short num_ser;
       u_short num_mice;

       int pktflags;		/* global flags for packet driver */

       unsigned int update, freq;	/* temp timer magic */
       unsigned int wantdelta, realdelta;
       unsigned long cpu_spd;		/* (1/speed)<<32 */
       unsigned long cpu_tick_spd;	/* (1.19318/speed)<<32 */

       unsigned int hogthreshold;

       int mem_size, xms_size, ems_size, dpmi, max_umb;
       int secure;
       unsigned int ems_frame;
       char must_spare_hardware_ram;
       char hardware_pages[ ((HARDWARE_RAM_STOP-HARDWARE_RAM_START) >> 12)+1 ];


       int sillyint;            /* IRQ numbers for Silly Interrupt Generator 
       				   (bitmask, bit3..15 ==> IRQ3 .. IRQ15) */

       int keyboard;
       unsigned char *key_map;     /* pointer to the correct keyboard-map */
       unsigned char *shift_map;
       unsigned char *alt_map;
       unsigned char *num_table;
       unsigned short detach;
       unsigned char *debugout;
       unsigned char *pre_stroke;  /* pointer to keyboard pre strokes */

       /* Lock File business */
       char *tty_lockdir;	/* The Lock directory  */
       char *tty_lockfile;	/* Lock file pretext ie LCK.. */
       boolean tty_lockbinary;	/* Binary lock files ? */

       /* Sound emulation */
       __u16 sb_base;
       __u8 sb_dma;
       __u8 sb_irq;
       char *sb_dsp;
       char *sb_mixer;
       __u16 mpu401_base;
     }

config_t;


#define SPKR_OFF	0
#define SPKR_NATIVE	1
#define SPKR_EMULATED	2

#define KEYB_FINNISH           0
#define KEYB_FINNISH_LATIN1    1
#define KEYB_US                2
#define KEYB_UK                3
#define KEYB_DE                4
#define KEYB_DE_LATIN1         5
#define KEYB_FR                6
#define KEYB_FR_LATIN1         7
#define KEYB_DK                8
#define KEYB_DK_LATIN1         9
#define KEYB_DVORAK           10
#define KEYB_SG               11
#define KEYB_SG_LATIN1        12
#define KEYB_NO               13
#define KEYB_NO_LATIN1        14
#define KEYB_SF               15
#define KEYB_SF_LATIN1        16
#define KEYB_ES               17
#define KEYB_ES_LATIN1        18
#define KEYB_BE               19
#define KEYB_PO               20
#define KEYB_IT               21
#define KEYB_SW		      22
#define KEYB_HU		      23
#define KEYB_HU_CWI	      24
#define KEYB_HU_LATIN2	      25
/*
 * Right now, dosemu only supports two serial ports.
 */
#define SIG_SER		SIGTTIN

#define SIG_TIME	SIGALRM
#define TIMER_TIME	ITIMER_REAL

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
EXTERN __inline__ void run_int(int);
EXTERN int mfs_redirector(void);
EXTERN void int10(void);
EXTERN void int13(u_char);
EXTERN void int14(u_char);
EXTERN void int17(u_char);
EXTERN void io_select(fd_set);
EXTERN int pd_receive_packet(void);
EXTERN int printer_tick(u_long);
EXTERN int printer_tick(u_long);
EXTERN void floppy_tick(void);
EXTERN void open_kmem(void);
EXTERN void close_kmem(void);
EXTERN void CloseNetworkLink(int);
EXTERN int parse_config(char *);
EXTERN void disk_init(void);
EXTERN void serial_init(void);
EXTERN void close_all_printers(void);
EXTERN void release_ports(void);
EXTERN void serial_close(void);
EXTERN void disk_close_all(void);
EXTERN void init_all_printers(void);
EXTERN int mfs_inte6(void);
EXTERN void pkt_helper(void);
EXTERN short pop_word(struct vm86_regs *);
EXTERN void ems_init(void);
EXTERN int GetDebugFlagsHelper(char *);
EXTERN int SetDebugFlagsHelper(char *);
EXTERN void leavedos(int) NORETURN;
EXTERN void add_to_io_select(int, unsigned char);
EXTERN void remove_from_io_select(int, unsigned char);
EXTERN void sigquit(int);
#ifdef __linux__
EXTERN void sigalrm(int, struct sigcontext_struct);
EXTERN void sigio(int, struct sigcontext_struct);
#endif
#ifdef __NetBSD__
EXTERN void sigalrm(int, int, struct sigcontext *);
EXTERN void sigio(int, int, struct sigcontext *);
#endif

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
 * functions should add themselves to the SIGNALS_THAT_QUEUE define and
 * use SETQSIG(). To that end they will also need to be set up in an
 * order such as SIGIO.
 *
 * DANG_END_FUNCTION
 *
 */
#define SIGNALS_THAT_QUEUE SIGIO|SIGALRM|SIG_RELEASE|SIG_ACQUIRE

#ifdef __NetBSD__
#define SignalHandler sig_t
#define __sighandler_t sig_t

#define NEWSETQSIG(sig, fun)	sa.sa_handler = (__sighandler_t)fun; \
					sa.sa_flags = SA_RESTART|SA_ONSTACK ; \
					sigemptyset(&sa.sa_mask); \
					sigaddset(&sa.sa_mask, SIGNALS_THAT_QUEUE); \
					sigaction(sig, &sa, NULL);

#define SETSIG(sig, fun)	sa.sa_handler = (__sighandler_t)fun; \
					sa.sa_flags = SA_RESTART; \
					sigemptyset(&sa.sa_mask); \
					sigaddset(&sa.sa_mask, SIG_TIME); \
					sigaction(sig, &sa, NULL);

#define NEWSETSIG(sig, fun) \
			sa.sa_handler = (__sighandler_t) fun; \
			sa.sa_flags = SA_RESTART|SA_ONSTACK; \
			sigemptyset(&sa.sa_mask); \
			sigaddset(&sa.sa_mask, SIG_TIME); \
			sigaction(sig, &sa, NULL);
#endif
#ifdef __linux__
#ifdef __GLIBC__
#define SignalHandler __sighandler_t
#endif
#define NEWSETQSIG(sig, fun)	sa.sa_handler = (__sighandler_t)fun; \
			/* Point to the top of the stack, minus 4 \
			   just in case, and make it aligned  */ \
			sa.sa_restorer = \
			(void (*)()) (((unsigned int)(cstack) + sizeof(cstack) - 4) & ~3); \
					sa.sa_flags = SA_RESTART ; \
					sigemptyset(&sa.sa_mask); \
					sigaddset(&sa.sa_mask, SIGNALS_THAT_QUEUE); \
					dosemu_sigaction(sig, &sa, NULL);

#define SETSIG(sig, fun)	sa.sa_handler = (SignalHandler)fun; \
					sa.sa_flags = SA_RESTART; \
					sigemptyset(&sa.sa_mask); \
					sigaddset(&sa.sa_mask, SIG_TIME); \
					sigaction(sig, &sa, NULL);

#define NEWSETSIG(sig, fun) \
			sa.sa_handler = (__sighandler_t) fun; \
			/* Point to the top of the stack, minus 4 \
			   just in case, and make it aligned  */ \
			sa.sa_restorer = \
			(void (*)()) (((unsigned int)(cstack) + sizeof(cstack) - 4) & ~3); \
			sa.sa_flags = SA_RESTART; \
			sigemptyset(&sa.sa_mask); \
			sigaddset(&sa.sa_mask, SIG_TIME); \
			dosemu_sigaction(sig, &sa, NULL);
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

#ifdef __NetBSD__
#define iopl(value)			{ EXTERN int errno; if (i386_iopl(value) == -1) { g_printf("iopl failed: %s\n", strerror(errno)); leavedos(4);}}
#endif

EXTERN void signal_init(void);
EXTERN void device_init(void);
EXTERN void hardware_setup(void);
EXTERN void memory_init(void);
EXTERN void timer_interrupt_init(void);
EXTERN void keyboard_flags_init(void);
EXTERN void video_config_init(void);
EXTERN void printer_init(void);
EXTERN void video_close(void);
EXTERN void hma_exit(void);
EXTERN void ems_helper(void);
EXTERN boolean_t ems_fn(struct vm86_regs *);
EXTERN void serial_helper(void);
EXTERN void mouse_helper(void);
EXTERN void cdrom_helper(void);
EXTERN void boot(void);
EXTERN int pkt_int(void);
EXTERN void read_next_scancode_from_queue (void);
EXTERN unsigned short detach (void);
EXTERN void HMA_init(void);

#endif /* EMU_H */
