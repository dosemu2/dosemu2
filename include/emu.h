/* dos emulator, Matthias Lautner 
 * Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1995/02/25 22:38:41 $
 * $Source: /home/src/dosemu0.60/include/RCS/emu.h,v $
 * $Revision: 2.25 $
 * $State: Exp $
 *
 * $Log: emu.h,v $
 * Revision 2.25  1995/02/25  22:38:41  root
 * *** empty log message ***
 *
 * Revision 2.24  1995/02/25  21:54:46  root
 * *** empty log message ***
 *
 * Revision 2.23  1995/02/05  16:54:16  root
 * Prep for Scotts patches.
 *
 * Revision 2.22  1995/01/14  15:31:55  root
 * New Year checkin.
 *
 * Revision 2.21  1994/11/13  00:40:45  root
 * Prep for Hans's latest.
 *
 * Revision 2.20  1994/11/03  11:43:26  root
 * Checkin Prior to Jochen's Latest.
 *
 * Revision 2.19  1994/10/14  17:58:38  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.18  1994/10/03  00:24:25  root
 * Checkin prior to pre53_25.tgz
 *
 * Revision 2.17  1994/09/26  23:10:13  root
 * Prep for pre53_22.
 *
 *
 */

#ifndef EMU_H
#define EMU_H

#include <sys/types.h>
#include <signal.h> 
#include "machcompat.h"
#include "cpu.h"

#include "extern.h"

#if 1 /* Set to 1 to use Silly Interrupt generator */
#define SIG 1
typedef struct { int fd; int irq; } SillyG_t;
#endif

#if 0 /* NOTE: This has been moved to the main Makefile */
#if 0 
  /* Set this to 1, if you you need the emumodules special features
   * You also must load the modules as follows:
   *   login in as root
   *   cd /usr/src/dosemuXXX/syscallmgr
   *   ./insmod syscallmgr.o
   *   ./insmod -m ../emumod/emumodule.o
   */
  #define REQUIRES_EMUMODULE
#else 
  #undef REQUIRES_EMUMODULE
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
EXTERN struct vm86_struct vm86s;
EXTERN fd_set fds_sigio, fds_no_sigio;
EXTERN unsigned int use_sigio INIT(0);
EXTERN unsigned int not_use_sigio INIT(0);
EXTERN int terminal_pipe;
EXTERN int terminal_fd INIT(-1);

/* set to one if runing setuid as root */
EXTERN int i_am_root INIT(0);

EXTERN char *cstack[16384];

/* this is DEBUGGING code! */
EXTERN int sizes INIT(0);

EXTERN char *tmpdir;
extern int screen, screen_mode;

/* number of highest vid page - 1 */
EXTERN int max_page INIT(7);


extern char *cl,		/* clear screen */
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
EXTERN int kbd_fd INIT(-1);
/* the file descriptor for /dev/mem when mmap'ing the video mem */
EXTERN int mem_fd INIT(-1);
extern int in_readkeyboard;

/* X-pipes */
EXTERN int keypipe;
EXTERN int mousepipe;

EXTERN int in_vm86 INIT(0);

extern int li, co;	/* lines, columns */
extern int scanseq;
extern int cursor_row;
extern int cursor_col;

void dos_ctrlc(void), dos_ctrl_alt_del(void);
int ext_fs(int, char *, char *, int);
#if 0 /* Not used anymore */
int outch(int c);
#endif
extern void run_vm86(void);

#define NOWAIT  0
#define WAIT    1
#define TEST    2
#define POLL    3

void getKeys(void);



struct debug_flags {
  unsigned char
   disk,		/* disk msgs,        "d" */
   read,		/* disk read         "R" */
   write,		/* disk write        "W" */
   dos,			/* unparsed int 21h  "D" */
   video,		/* video,            "v" */
   X,			/* X support,        "X" */
   keyb,		/* keyboard,         "k" */
   io,			/* port I/O,         "i" */
   serial,		/* serial,           "s" */
   mouse,		/* mouse,            "m" */
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
   pd;			/* Packet driver     "P" */
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

extern void saytime(char *m_str);

int
ifprintf(unsigned char, const char *,...) FORMAT(printf, 2, 3);
     void p_dos_str(char *,...) FORMAT(printf, 1, 2);

#if 1

#define dbug_printf(f,a...)	ifprintf(2,f,##a)
#define k_printf(f,a...) 	(d.keyb?ifprintf(d.keyb,f,##a):0)
#define h_printf(f,a...) 	ifprintf(d.hardware,f,##a)
#define v_printf(f,a...) 	ifprintf(d.video,f,##a)
#define X_printf(f,a...)        ifprintf(d.X,f,##a)
#define s_printf(f,a...) 	ifprintf(d.serial,f,##a)
#define p_printf(f,a...) 	ifprintf(d.printer,f,##a)
#define d_printf(f,a...) 	ifprintf(d.disk,f,##a)
#define i_printf(f,a...) 	ifprintf(d.io,f,##a)
#define R_printf(f,a...) 	ifprintf(d.read,f,##a)
#define W_printf(f,a...) 	ifprintf(d.write,f,##a)
#define warn(f,a...)     	ifprintf(d.warning,f,##a)
#define g_printf(f,a...)	ifprintf(d.general,f,##a)
#define x_printf(f,a...)	ifprintf(d.xms,f,##a)
#define D_printf(f,a...)	ifprintf(d.dpmi,f,##a)
#define m_printf(f,a...)	ifprintf(d.mouse,f,##a)
#define I_printf(f,a...) 	ifprintf(d.IPC,f,##a)
#define E_printf(f,a...) 	ifprintf(d.EMS,f,##a)
#define c_printf(f,a...) 	ifprintf(d.config,f,##a)
#define e_printf(f,a...) 	ifprintf(1,f,##a)
#define n_printf(f,a...)        ifprintf(d.network,f,##a)	/* TRB */
#define pd_printf(f,a...)       ifprintf(d.pd,f,##a)	/* pktdrvr  */
#define error(f,a...)	 	fprintf(stderr, f, ##a)
#define hard_error(f, a...)	fprintf(stderr, f, ##a) 

#else
#define dbug_printf(f,a...)	ifprintf(2,f,##a)
#define k_printf(f,a...)
#define h_printf(f,a...)
#define v_printf(f,a...)
#define s_printf(f,a...)
#define p_printf(f,a...)
#define d_printf(f,a...)
#define i_printf(f,a...)
#define R_printf(f,a...)
#define W_printf(f,a...)
#define warn(f,a...)
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
#define error(f,a...)

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

     extern struct debug_flags d;
     extern u_char in_sighandler, in_ioctl;
/* one-entry queue ;-( for ioctl's */
EXTERN struct ioctlq iq INIT({0, 0, 0, 0}); 
EXTERN u_char in_ioctl INIT(0);
EXTERN struct ioctlq curi INIT({0, 0, 0, 0});



     /* int 11h config single bit tests
 */
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

     typedef unsigned char boolean;

/* would like to have this in memory.h (but emu.h is included before memory.h !) */
#define HARDWARE_RAM_START 0xc8000
#define HARDWARE_RAM_STOP  0xf0000


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
       u_short gfxmemsize;		/* for SVGA card, in K */
       u_short term_method;		/* Terminal method: ANSI or NCURSES */
       u_short term_color;		/* Terminal color support on or off */
       u_short term_updatelines;	/* Amount to update at a time */
       u_short term_updatefreq;		/* Terminal update frequency */
       u_short term_charset;		/* Terminal Character set */
       u_short term_corner;             /* Update char at lower-right corner */
       u_short X_updatelines;           /* Amount to update at a time */
       u_short X_updatefreq;            /* X update frequency */
       char    *X_display;              /* X server to use (":0") */
       char    *X_title;                /* X window title */
       char    *X_icon_name;
       char    *X_font;
       int     X_blinkrate;
       boolean fullrestore;
       int     dualmon;

       u_short usesX;  /* !=0 if dosemu owns an X window */

       boolean console_keyb;
       boolean kbd_tty;
       boolean X_keycode;	/* use keycode field of event structure */
       boolean exitearly;
       boolean mathco;
       boolean ipxsup;
       boolean keybint;
       boolean dosbanner;
       boolean allowvideoportaccess;
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

       u_short speaker;		/* 0 off, 1 native, 2 emulated */
       u_short fdisks, hdisks;
       u_short num_lpt;
       u_short num_ser;
       u_short num_mice;

       int pktflags;		/* global flags for packet driver */

       unsigned int update, freq;	/* temp timer magic */

       unsigned int hogthreshold;

       int mem_size, xms_size, ems_size, dpmi, max_umb;
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

       /* Lock File business */
       char *tty_lockdir;	/* The Lock directory  */
       char *tty_lockfile;	/* Lock file pretext ie LCK.. */
       boolean tty_lockbinary;	/* Binary lock files ? */

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
#define KEYB_SF               15
#define KEYB_SF_LATIN1        16
#define KEYB_ES               17
#define KEYB_ES_LATIN1        18
#define KEYB_BE               19
#define KEYB_PO               20
#define KEYB_IT               21
#define KEYB_SW		      22
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
extern void cli(void);
extern void sti(void);
extern int port_readable(int);
extern int port_writeable(int);
extern int read_port(int);
extern int write_port(int, int);
extern __inline__ void parent_nextscan(void);
extern __inline__ void disk_close(void);
extern void cpu_setup(void);
extern __inline__ void run_int(int);
extern int mfs_redirector(void);
extern void int10(void);
extern void int13(u_char);
extern void int14(u_char);
extern void int17(u_char);
extern void io_select(fd_set);
extern int pd_receive_packet(void);
extern int printer_tick(u_long);
extern int printer_tick(u_long);
extern void floppy_tick(void);
extern void open_kmem(void);
extern void close_kmem(void);
extern void CloseNetworkLink(int);
extern int parse_config(char *);
#ifdef __NetBSD__
extern int priv_on(void);
extern int priv_off(void);
#endif
#ifdef __linux__
extern int exchange_uids(void);
#define priv_on exchange_uids
#define priv_off exchange_uids
#endif
extern void disk_init(void);
extern void serial_init(void);
extern void close_all_printers(void);
extern void serial_close(void);
extern void disk_close_all(void);
extern void init_all_printers(void);
extern int mfs_inte6(void);
extern void pkt_helper(void);
extern short pop_word(struct vm86_regs *);
extern void ems_init(void);
extern int GetDebugFlagsHelper(char *);
extern int SetDebugFlagsHelper(char *);
extern void leavedos(int) NORETURN;
extern void add_to_io_select(int, unsigned char);
extern void remove_from_io_select(int, unsigned char);
extern void sigio(int, struct sigcontext_struct);
extern void sigquit(int);
extern void sigalrm(int, struct sigcontext_struct);

/* signals for Linux's process control of consoles */
#define SIG_RELEASE     SIGWINCH
#define SIG_ACQUIRE     SIGUSR1

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

extern inline void SIGNAL_save( void (*signal_call)(void) );
extern inline void handle_signals(void);


#ifdef REQUIRES_EMUMODULE
  #include "emusys.h"
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
EXTERN int fatalerr;


#endif /* EMU_H */
