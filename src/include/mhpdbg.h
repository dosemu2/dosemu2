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
#include <stdint.h>

#include "dosemu_debug.h"

// There is also an argument field shifted 8 bits left
enum dosdebug_event {
   DBG_INIT = 0,
   DBG_BOOT,
   DBG_INTx,
   DBG_TRAP,
   DBG_PRE_VM86,
   DBG_POLL,
   DBG_GPF,
   DBG_INTxDPMI,
};

extern unsigned long dosdebug_flags;
#define DBGF_WAIT_ON_STARTUP		0x001
#define DBGF_INTERCEPT_LOG		0x002
#define DBGF_DISABLE_LOG_TO_FILE	0x004
#define DBGF_ALLOW_BREAKPOINT_OVERWRITE 0x008
/* removed DBGF_LOG_TO_DOSDEBUG		0x100 */
#define DBGF_LOG_TO_BREAK		0x200
#define DBGF_LOG_TEMPORARY		0x400
#define DBGF_IN_LEAVEDOS	   0x40000000


unsigned int mhp_debug(enum dosdebug_event, unsigned int, unsigned int);
void mhp_send(void);
void mhp_input(void);
void mhp_close(void);
void mhp_printf(const char *,...) FORMAT(printf, 1, 2);
int mhp_getaxlist_value(int v, int mask);
int mhp_getcsip_value(void);
void mhp_modify_eip(int delta);
void mhp_intercept_log(const char *flags, int temporary);
void mhp_intercept(const char *msg, const char *logflags);
void mhp_exit_intercept(int errcode);
int mhpdbg_is_stopped(void);
int mhp_usermap_move_block(uint16_t oldseg, uint16_t newseg,
                           uint16_t startoff, uint32_t blklen);
int mhp_usermap_load_gnuld(const char *fname, uint16_t origin);
#ifdef USE_MHPDBG
int mhp_revectored(int inum);
#else
static inline int mhp_revectored(int inum)
{
    return 0;
}
#endif

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

   unsigned char intxxtab[32];
};

extern struct mhpdbg mhpdbg;

#ifdef MHP_PRIVATE

#define DBG_TYPE(val)	       ((val) & 0xff)
#define DBG_ARG(val)	       ((val) >> 8)
#define SRSIZE MHP_BUFFERSIZE
#define MYPORT 3456
#define IBUFS 100
#define MAXARG 16
#define MAXBP 64

void mhp_cmd(const char *);
void mhp_bpset(void);
void mhp_bpclr(void);
int  mhp_bpchk(unsigned int);
int mhp_setbp(unsigned int seekval);
int mhp_clearbp(unsigned int seekval);
void mhp_regex(const char *fmt, va_list args);

void mhpdbg_trace_init(void);

struct brkentry {
   unsigned int brkaddr;
   unsigned char opcode;
   char is_dpmi;
   char is_valid;
};

struct segoff {
  unsigned short off,seg;
};

/* parameters for DOS 4Bh (EXEC) call */
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

#define PAR4b_addr(x) SEGOFF2LINEAR(mhpdbgc.bpload_par->x.seg, \
				    mhpdbgc.bpload_par->x.off)

struct mhpdbgc
{
   int stopped;
   int want_to_stop;
   enum dosdebug_event currcode;
   enum { TRACE_NONE, TRACE_INTO, TRACE_OVER } trapcmd;
   int trapip; /* ip that we were on when we started the "tracei" command */
   int bpload;
   int bpload_bp;
   int int21_count;
   int int_handled;
   int saved_if;
   struct mhpdbg_4bpar *bpload_par;
   char bpload_cmd[128];
   char bpload_cmdline[132];
   unsigned char intxxalt[32];
   struct brkentry brktab[MAXBP];
   int bpcleared;
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

extern int traceloop;
extern char loopbuf[4];
extern struct mhpdbgc mhpdbgc;

#endif	 /* MHP_PRIVATE */

#endif	 /* MHPDBG_H */
