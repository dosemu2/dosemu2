/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DOSEMU debugger,  1995 Max Parke <mhp@light.lightlink.com>
 *
 * This is file mhpdbg.c
 *
 * changes: ( for details see top of file mhpdbgc.c )
 *
 *   07Jul96 Hans Lermen <lermen@elserv.ffm.fgan.de>
 *   19May96 Max Parke <mhp@lightlink.com>
 *   16Sep95 Hans Lermen <lermen@elserv.ffm.fgan.de>
 *   08Jan98 Hans Lermen <lermen@elserv.ffm.fgan.de>
 */

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "bitops.h"
#include "emu.h"
#include "cpu.h"
#include "bios.h"
#include "dpmi.h"
#include "timers.h"

#define MHP_PRIVATE
#include "mhpdbg.h"

#if 0
/* NOTE: the below is already defined with #include "emu.h"
 *       Must NOT redefine it, else vm86plus won't work !!!
 */
extern struct vm86_struct vm86s;
#endif

static void vmhp_printf(const char *fmt, va_list args);
static void mhp_poll(void);
static void mhp_puts(char*);
void mhp_putc(char);

static char mhp_banner[] = {
  "\nDOSEMU Debugger V0.6 connected\n"
  "- type ? to get help on commands -\n"
};
struct mhpdbgc mhpdbgc ={0};

/********/
/* CODE */
/********/

static void mhp_puts(char* s)

{
   for (;;){
	   if (*s == 0x00)
		   break;
	   mhp_putc (*s++);
   }
}

void mhp_putc(char c1)

{

#if 0
   if (c1 == '\n') {
      mhpdbg.sendbuf[mhpdbg.sendptr] = '\r';
      if (mhpdbg.sendptr < SRSIZE-1)
	 mhpdbg.sendptr++;
   }
#endif
   mhpdbg.sendbuf[mhpdbg.sendptr] = c1;
   if (mhpdbg.sendptr < SRSIZE-1)
      mhpdbg.sendptr++;

}

void mhp_send(void)
{
   if ((mhpdbg.sendptr) && (mhpdbg.fdout == -1)) {
      mhpdbg.sendptr = 0;
      return;
   }
   if ((mhpdbg.sendptr) && (mhpdbg.fdout != -1)) {
      write (mhpdbg.fdout, mhpdbg.sendbuf, mhpdbg.sendptr);
      mhpdbg.sendptr = 0;
   }
}

static  char *pipename_in, *pipename_out;

void mhp_close(void)
{
   if (mhpdbg.fdin == -1) return;
   if (mhpdbg.active) {
     mhp_putc(1); /* tell debugger terminal to also quit */
     mhp_send();
   }
   unlink(pipename_in);
   free(pipename_in);
   unlink(pipename_out);   
   free(pipename_out);
   mhpdbg.fdin = mhpdbg.fdout = -1;
   mhpdbg.active = 0;
   vm86s.vm86plus.vm86dbg_active = 0;
}

static int wait_for_debug_terminal = 0;

int vmhp_log_intercept(int flg, const char *fmt, va_list args)
{
  if (mhpdbg.active <= 1) return 0;
  if (flg) {
    if (dosdebug_flags & DBGF_LOG_TO_DOSDEBUG) {
      vmhp_printf(fmt, args);
      mhp_send();
    }
    if (dosdebug_flags & DBGF_LOG_TO_BREAK){
      mhp_regex(fmt, args);
    }
  }
  return 0;
}

static void mhp_init(void)
{
  int retval;

  mhpdbg.fdin = mhpdbg.fdout = -1;
  mhpdbg.active = 0;
  mhpdbg.sendptr = 0;

  vm86s.vm86plus.vm86dbg_active = 0;
  vm86s.vm86plus.vm86dbg_TFpendig = 0;
  memset(&vm86s.vm86plus.vm86dbg_intxxtab, 0, sizeof(vm86s.vm86plus.vm86dbg_intxxtab));

  asprintf(&pipename_in, "%s/dosemu.dbgin.%d", RUNDIR, getpid());
  retval = mkfifo(pipename_in, S_IFIFO | 0600);
  if (!retval) {
    asprintf(&pipename_out, "%s/dosemu.dbgout.%d", RUNDIR, getpid());
    retval = mkfifo(pipename_out, S_IFIFO | 0600);
    if (!retval) {
      mhpdbg.fdin = open(pipename_in, O_RDONLY | O_NONBLOCK);
      if (mhpdbg.fdin != -1) {
        /* NOTE: need to open read/write else O_NONBLOCK would fail to open */
        mhpdbg.fdout = open(pipename_out, O_RDWR | O_NONBLOCK);
        if (mhpdbg.fdout != -1) {
          add_to_io_select(mhpdbg.fdin, 0, mhp_input);
        }
        else {
          close(mhpdbg.fdin);
          mhpdbg.fdin = -1;
        }
      }
    }
  }
  else {
    fprintf(stderr, "Can't create debugger pipes, dosdebug not available\n");
  }
  if (mhpdbg.fdin == -1) {
    unlink(pipename_in);
    free(pipename_in);
    unlink(pipename_out);
    free(pipename_out);
  }
  else {
    if (dosdebug_flags) {
      /* don't fiddle with select, just poll until the terminal
       * comes up to send the first input
       */
       mhpdbg.nbytes = -1;
       mhpdbgc.stopped = 1;
       wait_for_debug_terminal = 1;
       mhp_input();
    }
  }
}

void mhp_input()
{
  if (mhpdbg.fdin == -1) return;
  mhpdbg.nbytes = read(mhpdbg.fdin, mhpdbg.recvbuf, SRSIZE);
  if (mhpdbg.nbytes == -1) return;
  if (mhpdbg.nbytes == 0 && !wait_for_debug_terminal) {
    if (mhpdbgc.stopped) {
      mhp_cmd("g");
      mhp_send();
    }
    mhpdbg.active = 0;
    return;
  }
  if (!mhpdbg.active) {
    mhpdbg.active = 1; /* 1 = new session */
  }
}

static void mhp_poll_loop(void)
{
   char *ptr, *ptr1;
   for (;;) {
      handle_signals();
      /* NOTE: if there is input on mhpdbg.fdin, as result of handle_signals
       *       io_select() is called and this then calls mhp_input.
       *       ( all clear ? )
       */
      if (mhpdbg.nbytes <= 0) {
         if (traceloop && mhpdbgc.stopped) {
           strcpy(mhpdbg.recvbuf,loopbuf);
           mhpdbg.nbytes=strlen(loopbuf);
         }
         else {
          if (mhpdbgc.stopped) {
            usleep(JIFFIE_TIME/10);
            continue;
          }
          else break;
        }
      }
      else {
        if (traceloop) { traceloop=loopbuf[0]=0; }
      }
      if ((mhpdbg.recvbuf[0] == 'q') && (mhpdbg.recvbuf[1] <= ' ')) {
	 if (mhpdbgc.stopped) {
	   mhp_cmd("g");
	   mhp_send();
	 }
	 mhpdbg.active = 0;
	 vm86s.vm86plus.vm86dbg_active = 0;
	 mhpdbg.sendptr = 0;
         mhpdbg.nbytes = 0;
         return;
      }
      mhpdbg.recvbuf[mhpdbg.nbytes] = 0x00;
      ptr = mhpdbg.recvbuf;
      while (ptr && *ptr) {
	ptr1 = strsep(&ptr, "\r\n");
	if (!ptr1)
	  ptr1 = ptr;
	if (!ptr1)
	  break;
        mhp_cmd(ptr1);
        mhp_send();
      }
      mhpdbg.nbytes = 0;
   }
}

static void mhp_poll(void)
{

   if (!mhpdbg.active && !wait_for_debug_terminal) {
     mhpdbg.nbytes = 0;
     return;
   }

   if (mhpdbg.active == 1) {
      /* new session has started */
      mhpdbg.active++;
      vm86s.vm86plus.vm86dbg_active = 1;
      
      mhp_printf ("%s", mhp_banner);
      mhp_cmd("rmapfile");
      mhp_send();
      if (wait_for_debug_terminal) wait_for_debug_terminal =0;
   }

   if (mhpdbgc.want_to_stop) {
      mhpdbgc.stopped = 1;
      mhpdbgc.want_to_stop = 0;
   }
   if (mhpdbgc.stopped) {
      if (dosdebug_flags & DBGF_LOG_TEMPORARY) {
         dosdebug_flags &= ~DBGF_LOG_TEMPORARY;
	 mhp_cmd("log off");
      }
      mhp_cmd("r0");
      mhp_send();
   }
   mhp_poll_loop();
}

void mhp_intercept_log(char *flags, int temporary)
{
   char buf[255];
   sprintf(buf, "log %s", flags);
   mhp_cmd(buf);
   mhp_cmd("log on");
   if (temporary)
      dosdebug_flags |= DBGF_LOG_TEMPORARY;
}

void mhp_intercept(char *msg, char *logflags)
{
   if (!mhpdbg.active || (mhpdbg.fdin == -1)) return;
   mhpdbgc.stopped = 1;
   mhpdbgc.want_to_stop = 0;
   traceloop = 0;
   mhp_printf(msg);
   mhp_cmd("r0");
   mhp_send();
   if (!(dosdebug_flags & DBGF_IN_LEAVEDOS)) {
     if (in_dpmi)
       dpmi_return_request();
     if (logflags)
       mhp_intercept_log(logflags, 1);
     return;
   }
   mhp_poll_loop();
}

void mhp_exit_intercept(int errcode)
{
   char buf[255];
   if (!errcode || !mhpdbg.active || (mhpdbg.fdin == -1) ) return;

   sprintf(buf, "\n****\nleavedos(%d) called, at termination point of DOSEMU\n****\n\n", errcode);
   dosdebug_flags |= DBGF_IN_LEAVEDOS;
   mhp_intercept(buf, NULL);
}

unsigned int mhp_debug(unsigned int code, unsigned int parm1, unsigned int parm2)
{
  int rtncd = 0;
#if 0
  return rtncd;
#endif
  mhpdbgc.currcode = code;
  mhp_bpclr();
  switch (DBG_TYPE(mhpdbgc.currcode)) {
  case DBG_INIT:
	  mhp_init();
	  break;
  case DBG_INTx:
	  if (!mhpdbg.active)
	     break;
	  if (test_bit(DBG_ARG(mhpdbgc.currcode), vm86s.vm86plus.vm86dbg_intxxtab)) {
	    if ((mhpdbgc.bpload==1) && (DBG_ARG(mhpdbgc.currcode) == 0x21) && (LWORD(eax) == 0x4b00) ) {
	      mhpdbgc.bpload_bp=((long)LWORD(cs) << 4) +LWORD(eip);
	      if (mhp_setbp(mhpdbgc.bpload_bp)) {
		mhpdbgc.bpload++;
		mhpdbgc.bpload_par=(struct mhpdbg_4bpar *)(((long)DBGload_parblock-(long)bios_f000)+(BIOSSEG << 4));
		memcpy((char *)mhpdbgc.bpload_par, MK_FP32(LWORD(es),LWORD(ebx)), 14);
                memcpy(mhpdbgc.bpload_cmdline, PAR4b_addr(commandline_ptr), 128);
                memcpy(mhpdbgc.bpload_cmd, SEG_ADR((char *), ds, dx), 128);
		LWORD(es)=BIOSSEG;
		LWORD(ebx)=(long)mhpdbgc.bpload_par - (BIOSSEG << 4);
		LWORD(eax)=0x4b01; /* load, but don't execute */
	      }
	      else {
	        mhpdbgc.bpload_bp=0;
	        mhpdbgc.bpload=0;
	      }
	      if (!--mhpdbgc.int21_count) {
	        volatile register int i=0x21; /* beware, set_bit-macro has wrong constraints */
	        clear_bit(i, vm86s.vm86plus.vm86dbg_intxxtab);
	      }
	    }
	    else {
	      if ((DBG_ARG(mhpdbgc.currcode) != 0x21) || !mhpdbgc.bpload ) {
	        mhpdbgc.stopped = 1;
		vm86s.vm86plus.vm86dbg_active = 1;
		vm86s.vm86plus.vm86dbg_TFpendig = 1;
	        mhp_poll();
	      }
	    }
	  }
	  break;
  case DBG_INTxDPMI:
	  if (!mhpdbg.active) break;
          mhpdbgc.stopped = 1;
          mhp_poll();
          dpmi_mhp_intxxtab[DBG_ARG(mhpdbgc.currcode) & 0xff] &= ~2;
	  break;
  case DBG_TRAP:
	  if (!mhpdbg.active)
	     break;
	  if (DBG_ARG(mhpdbgc.currcode) == 1) { /* single step */
		  if (mhpdbgc.trapcmd) {
			  mhpdbgc.trapcmd = 0;
			  rtncd = 1;
			  mhpdbgc.stopped = 1;
			  mhp_poll();
		  }
	  }
	  if (DBG_ARG(mhpdbgc.currcode) == 3) { /* int3 (0xCC) */
		  int ok=0;
		  uintptr_t csip=mhp_getcsip_value() - 1;
		  if (mhpdbgc.bpload_bp == csip ) {
		    mhp_clearbp(mhpdbgc.bpload_bp);
		    LWORD(eip)--;
		    if (mhpdbgc.bpload == 2) {
#if 0
		      mhpdbgc.bpload_bp=(mhpdbgc.bpload_par->csip.seg << 4)+mhpdbgc.bpload_par->csip.off;
#endif
		      mhpdbgc.bpload_bp=(long)PAR4b_addr(csip);
		      mhp_setbp(mhpdbgc.bpload_bp);
		      LWORD(cs)=BIOSSEG;
		      LWORD(eip)=(long)DBGload-(long)bios_f000;
		      mhpdbgc.bpload=0;
		    }
		    else {
		      ok=1;
		      mhpdbgc.bpload_bp=3;
		    }
		  }
		  else {
		    if ((ok=mhp_bpchk( (unsigned char *) csip))) {
			  mhp_modify_eip(-1);
		    }
		    else {
		      if ((ok=test_bit(3, vm86s.vm86plus.vm86dbg_intxxtab))) {
		        /* software programmed INT3 */
		        mhp_modify_eip(-1);
		        mhp_cmd("r");
		        mhp_modify_eip(+1);
		      }
		    }
		  }
		  if (ok) {
		    mhpdbgc.trapcmd = 0;
		    rtncd = 1;
		    mhpdbgc.stopped = 1;
		    mhp_poll();
		  }
	  }
	  break;
  case DBG_POLL:
	  mhp_poll();
	  break;
  case DBG_GPF:
	  if (!mhpdbg.active)
	     break;
	  mhpdbgc.stopped = 1;
	  mhp_poll();
	  break;
  default:
	  break;
  }
  if (mhpdbg.active) mhp_bpset();
  return rtncd;
}

static void vmhp_printf(const char *fmt, va_list args)
{
  char frmtbuf[SRSIZE];

  vsprintf(frmtbuf, fmt, args);

  mhp_puts(frmtbuf);
}

void mhp_printf(const char *fmt,...)
{
  va_list args;

  va_start(args, fmt);
  vmhp_printf(fmt, args);
  va_end(args);
}

