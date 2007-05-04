/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DOSEMU debugger,  1995 Max Parke <mhp@light.lightlink.com>
 *
 * This is file mhpdbg.h
 *
 * changes:
 *
 *   16Sep95 Hans Lermen <lermen@elserv.ffm.fgan.de>
 */

#ifndef MHPDBG_H
#define MHPDBG_H

#include <stdarg.h>
#include "extern.h"

#if 0  /* now defined in include/vm86plus */
#define VM86_TRAP 4	  /* (vm86 return) TRAP */
#endif

#define DBG_INIT 0
#define DBG_INTx 1
#define DBG_TRAP 2
#define DBG_POLL 3
#define DBG_GPF  4
#define DBG_INTxDPMI 5

EXTERN unsigned long dosdebug_flags INIT(0);
#define DBGF_WAIT_ON_STARTUP		0x001
#define DBGF_INTERCEPT_LOG		0x002
#define DBGF_DISABLE_LOG_TO_FILE	0x004
#define DBGF_LOG_TO_DOSDEBUG		0x100
#define DBGF_LOG_TO_BREAK		0x200
#define DBGF_LOG_TEMPORARY		0x400
#define DBGF_IN_LEAVEDOS	   0x40000000


unsigned int mhp_debug(unsigned int, unsigned int, unsigned int);
void mhp_send(void);
void mhp_input(void);
void mhp_close(void);
void mhp_printf(const char *,...);
int mhp_getaxlist_value(int v, int mask);
int mhp_getcsip_value(void);
void mhp_modify_eip(int delta);
void mhp_intercept_log(char *flags, int temporary);
void mhp_intercept(char *msg, char *logflags);
void mhp_exit_intercept(int errcode);

void DBGload(void);
void DBGload_CSIP(void);
void DBGload_parblock(void);

int vmhp_log_intercept(int flg, const char *fmt, va_list args);
 
#define MHP_BUFFERSIZE 8192
struct mhpdbg
{
   unsigned char sendbuf[MHP_BUFFERSIZE];
   unsigned char recvbuf[MHP_BUFFERSIZE];
   int sendptr;
   int nbytes;
   int active;
   int flags;
   
   int fdin,fdout;
};

EXTERN struct mhpdbg mhpdbg;

#ifdef MHP_PRIVATE

#define DBG_TYPE(val)	       ((val) & 0xff)
#define DBG_ARG(val)	       ((val) >> 8)
#define SRSIZE MHP_BUFFERSIZE
#define MYPORT 3456
#define IBUFS 100
#define MAXARG 16
#define MAXBP 64
#define MAXSYM 10000

void mhp_cmd(const char *);
void mhp_bpset(void);
void mhp_bpclr(void);
int  mhp_bpchk(unsigned char *);
int mhp_setbp(unsigned long seekval);
int mhp_clearbp(unsigned long seekval);
void mhp_regex(const char *fmt, va_list args);

struct brkentry {
   unsigned char * brkaddr;
   unsigned char opcode;
   char is_dpmi;
};

struct segoff {
  unsigned short off,seg;
};

struct mhpdbg_4bpar
{
  unsigned short env;
  struct segoff
    commandline_ptr,
    fcb1_ptr,
    fcb2_ptr,
    sssp,
    csip;
};

#define PAR4b_addr(x) ((char *)( ((long)mhpdbgc.bpload_par->x.seg << 4) \
                        + mhpdbgc.bpload_par->x.off ))

struct mhpdbgc
{
   int stopped;
   int want_to_stop;
   int currcode;
   int trapcmd;
   int bpload;
   int bpload_bp;
   int int21_count;
   struct mhpdbg_4bpar *bpload_par;
   char bpload_cmd[128];
   char bpload_cmdline[132];
   char intxxtab[256];
   struct brkentry brktab[MAXBP];
};

#if 0
extern int stop_cputime (int);
extern int restart_cputime (int);
#define MHP_STOP	{stop_cputime(0); mhpdbgc.stopped = 1;}
#define MHP_UNSTOP	{mhpdbgc.stopped = 0; restart_cputime(0);}
#else
#define MHP_STOP	mhpdbgc.stopped = 1
#define MHP_UNSTOP	mhpdbgc.stopped = 0
#endif

struct symbol_entry {
   unsigned long addr;
   unsigned char type;
   unsigned char name[49];
};

struct symbl2_entry {
   unsigned short seg;
   unsigned short off;
   unsigned char type;
   unsigned char name[49];
};

extern int traceloop;
extern char loopbuf[4];
extern struct mhpdbgc mhpdbgc;

#endif	 /* MHP_PRIVATE */

#endif	 /* MHPDBG_H */
