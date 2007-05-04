/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef DOSEMU_DEBUG_H
#define DOSEMU_DEBUG_H

#include <stdio.h>
#include <stdarg.h>
#include "config.h"
#include "extern.h"
#include "machcompat.h"

/* Some older gccs accept the format attribute too,
 * but not for the function pointers. :( */
#if GCC_VERSION_CODE >= 3002
# define FORMAT(T,A,B)  __attribute__((format(T,A,B)))
#else
# define FORMAT(T,A,B)
#endif

#if GCC_VERSION_CODE >= 2005
# define NORETURN	__attribute__((noreturn))
#else
# define NORETURN
#endif

/*  DONT_DEBUG_BOOT - if you set that, debug messages are
 *	skipped until the cpuemu is activated by int0xe6, so you can
 *	avoid the long DOS intro when using high debug values.
 *	You have to define it if you want to trace into the DOS boot
 *	phase (i.e. from the loading at 0x7c00 on) */
#if 0
#define DONT_DEBUG_BOOT
#endif

/*  CIRCULAR_LOGBUFFER (utilities.c) - another way to reduce the size of
 *	the debug logfile by keeping only the last n lines (n=~20000)
 */
#if 0
#define	CIRCULAR_LOGBUFFER
#endif

/*
 * The 'debug' array defines which features may print discrete debug
 * messages. If you want to add one, also adapt the following places in the
 * DOSEMU code:
 *
 *  src/base/init/config.c, parse_debugflags(), allopts[]
 *  src/include/emu.h, 'EXTERN struct debug_flags d INIT({0..})'
 *  src/base/async/dyndeb.c, GetDebugFlagsHelper()
 *
 * Here is an overview of which flags are used and which not, please keep
 * this comment in sync with the reality:
 *
 *   used: aA  cCdDeE  g h iIj k   mMn   pP QrRsStTu v wW X   Z#
 *   free:   bB      fF G H   J KlL   NoO  q        U V  x yYz
 */
 
#define DEBUG_CLASSES 128
struct debug_class
{
	void (*change_level)(int level);
	char *help_text;
	unsigned char level, letter;
};

#ifdef DONT_DEBUG_BOOT
EXTERN struct debug_class debug_save[DEBUG_CLASSES];
#endif
EXTERN struct debug_class debug[DEBUG_CLASSES];

int log_printf(int, const char *,...) FORMAT(printf, 2, 3);
int vlog_printf(int, const char *,va_list);

int p_dos_str(const char *,...) FORMAT(printf, 1, 2);

#if 0  /* set this to 1, if you want dosemu to honor the -D flags */
 #define NO_DEBUGPRINT_AT_ALL
#endif
#undef DEBUG_LITE

EXTERN int shut_debug INIT(0);

#ifndef NO_DEBUGPRINT_AT_ALL
# ifdef DEBUG_LITE
#  define ifprintf(flg,fmt,a...)	do{ if (flg) log_printf(0x10000|__LINE__,fmt,##a); }while(0)
# else
#  define ifprintf(flg,fmt,a...)	do{ if (flg) log_printf(flg,fmt,##a); }while(0)
# endif
#else
# define ifprintf(flg,fmt,a...) 
#endif

/* unconditional message into debug log */
#define dbug_printf(f,a...)	ifprintf(10,f,##a)

/* unconditional message into debug log and stderr */
void error(const char *fmt, ...);
void verror(const char *fmt, va_list args);

#define flush_log()		{ if (dbg_fd) log_printf(-1, "\n"); }

/* "dRWDCvXkiTsm#pgcwhIExMnPrS" */
#define b_printf(f,a...)	ifprintf(debug_level('b'),f,##a)
#define c_printf(f,a...)	ifprintf(debug_level('c'),f,##a)
#define d_printf(f,a...)	ifprintf(debug_level('d'),f,##a)
#define e_printf(f,a...)	ifprintf(debug_level('e'),f,##a)
#define f_printf(f,a...)	ifprintf(debug_level('f'),f,##a)
#define g_printf(f,a...)	ifprintf(debug_level('g'),f,##a)
#define h_printf(f,a...)	ifprintf(debug_level('h'),f,##a)
#define i_printf(f,a...)	ifprintf(debug_level('i'),f,##a)
#define j_printf(f,a...)	ifprintf(debug_level('j'),f,##a)
#define k_printf(f,a...)	ifprintf(debug_level('k'),f,##a)
#define l_printf(f,a...)	ifprintf(debug_level('l'),f,##a)
#define m_printf(f,a...)	ifprintf(debug_level('m'),f,##a)
#define n_printf(f,a...)	ifprintf(debug_level('n'),f,##a)
#define o_printf(f,a...)	ifprintf(debug_level('o'),f,##a)
#define p_printf(f,a...)	ifprintf(debug_level('p'),f,##a)
#define q_printf(f,a...)	ifprintf(debug_level('q'),f,##a)
#define r_printf(f,a...)	ifprintf(debug_level('r'),f,##a)
#define s_printf(f,a...)	ifprintf(debug_level('s'),f,##a)
#define t_printf(f,a...)	ifprintf(debug_level('t'),f,##a)
#define u_printf(f,a...)	ifprintf(debug_level('u'),f,##a)
#define v_printf(f,a...)	ifprintf(debug_level('v'),f,##a)
#define w_printf(f,a...)	ifprintf(debug_level('w'),f,##a)
#define x_printf(f,a...)	ifprintf(debug_level('x'),f,##a)
#define y_printf(f,a...)	ifprintf(debug_level('y'),f,##a)
#define z_printf(f,a...)	ifprintf(debug_level('z'),f,##a)

#define A_printf(f,a...)	ifprintf(debug_level('A'),f,##a)
#define B_printf(f,a...)	ifprintf(debug_level('B'),f,##a)
#define C_printf(f,a...)	ifprintf(debug_level('C'),f,##a)
#define D_printf(f,a...)	ifprintf(debug_level('D'),f,##a)
#define E_printf(f,a...)	ifprintf(debug_level('E'),f,##a)
#define F_printf(f,a...)	ifprintf(debug_level('F'),f,##a)
#define G_printf(f,a...)	ifprintf(debug_level('G'),f,##a)
#define H_printf(f,a...)	ifprintf(debug_level('H'),f,##a)
#define I_printf(f,a...)	ifprintf(debug_level('I'),f,##a)
#define J_printf(f,a...)	ifprintf(debug_level('J'),f,##a)
#define K_printf(f,a...)	ifprintf(debug_level('K'),f,##a)
#define L_printf(f,a...)	ifprintf(debug_level('L'),f,##a)
#define M_printf(f,a...)	ifprintf(debug_level('M'),f,##a)
#define N_printf(f,a...)	ifprintf(debug_level('N'),f,##a)
#define O_printf(f,a...)	ifprintf(debug_level('O'),f,##a)
#define P_printf(f,a...)	ifprintf(debug_level('P'),f,##a)
#define Q_printf(f,a...)	ifprintf(debug_level('Q'),f,##a)
#define R_printf(f,a...)	ifprintf(debug_level('R'),f,##a)
#define S_printf(f,a...)	ifprintf(debug_level('S'),f,##a)
#define T_printf(f,a...)	ifprintf(debug_level('T'),f,##a)
#define U_printf(f,a...)	ifprintf(debug_level('U'),f,##a)
#define V_printf(f,a...)	ifprintf(debug_level('V'),f,##a)
#define W_printf(f,a...)	ifprintf(debug_level('W'),f,##a)
#define X_printf(f,a...)	ifprintf(debug_level('X'),f,##a)
#define Y_printf(f,a...)	ifprintf(debug_level('Y'),f,##a)
#define Z_printf(f,a...)	ifprintf(debug_level('Z'),f,##a)

#define di_printf(f,a...)	ifprintf(debug_level('#'),f,##a)

/* Handle the weird leftovers */
#undef D_printf
#undef M_printf
#undef w_printf
#undef P_printf
#define ds_printf(f,a...)	ifprintf(debug_level('D'),f,##a)
#define D_printf(f,a...)	ifprintf(debug_level('M'),f,##a)
#define warn(f,a...)		ifprintf(debug_level('w'),f,##a)
#define pd_printf(f,a...)	ifprintf(debug_level('P'),f,##a)

#ifndef NO_DEBUGPRINT_AT_ALL

extern int parse_debugflags(const char *s, unsigned char flag);
extern int SetDebugFlagsHelper(char *debugStr);
extern int GetDebugFlagsHelper(char *debugStr, int print);
extern int register_debug_class(
	int letter, void (*change_level)(int level), char *help_text);
extern int unregister_debug_class(int letter);
extern void print_debug_usage(FILE *stream);
extern int set_debug_level(int letter, int level);
extern inline int debug_level(int letter);
extern inline int debug_level(int letter)
{
	if (letter >= DEBUG_CLASSES) {
		return -1;
	}
	return debug[letter].level;

}

#else


extern inline int parse_debugflags(const char *s, unsigned char flag) { return 0; }
extern inline int SetDebugFlagsHelper(char *debugStr) { return 0; }
extern inline int GetDebugFlagsHelper(char *debugStr) { debugStr[0] = '\0'; return 0; }
extern inline int register_debug_class(
	int letter, void (*change_level)(int level), char *help_text)
{
	return 0;
}
extern inline int unregister_debug_class(int letter) { return 0; }
extern inline void print_debug_usage(FILE *stream) { return; }
extern inline int set_debug_level(int letter, int level) { return 0; }
extern inline int debug_level(int letter) { return 0; }
#endif

#endif /* DOSEMU_DEBUG_H */
