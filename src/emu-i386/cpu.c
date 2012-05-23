/* 
 * DANG_BEGIN_MODULE
 * 
 * REMARK
 * CPU/V86 support for dosemu
 * /REMARK
 *
 * DANG_END_MODULE
 *
 * much of this code originally written by Matthias Lautner
 * taken over by:
 *          Robert Sanders, gt8134b@prism.gatech.edu
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>

#include "config.h"
#include "memory.h"
#include "termio.h"
#include "emu.h"
#include "port.h"
#include "int.h"

#include "vc.h"

#include "dpmi.h"
#include "priv.h"

extern void xms_control(void);

static unsigned long CRs[5] =
{
	0x00000010,	/*0x8003003f?*/
	0x00000000,	/* invalid */
	0x00000000,
	0x00000000,
	0x00000000
};

static unsigned long DRs[8] =
{
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xffff0ff0,
	0x00000400,
	0xffff0ff0,
	0x00000400
};

static unsigned long TRs[2] =
{
	0x00000000,
	0x00000000
};

/* 
 * DANG_BEGIN_FUNCTION cpu_trap_0f
 *
 * process opcodes 0F xx xx trapped by GP_fault
 * returns 1 if handled, 0 otherwise
 * Main difference with previous version: bits in our pseudo-control
 * regs can now be written. This should make CPU detection pgms happy.
 *
 * DANG_END_FUNCTION
 *
 */
int cpu_trap_0f (unsigned char *csp, struct sigcontext_struct *scp)
{
  	if (in_dpmi && (scp==NULL)) return 0; /* for safety */
	/* moved access to "csp" after safety check,
	 *                          -- 980627 Andreas Kirschbaum
	 */
	g_printf("CPU: TRAP op 0F %02x %02x\n",csp[1],csp[2]);

	if (csp[1] == 0x06) {
		(in_dpmi? scp->eip:LWORD(eip)) += 2;  /* CLTS - ignore */
		return 1;
	}
	else if (csp[1] == 0x31) {
		/* ref: Van Gilluwe, "The Undocumented PC". The program
		 * 'cpurdtsc.exe' traps here */
		if (vm86s.cpu_type >= CPU_586) {
		  __asm__ __volatile__ ("rdtsc" \
			:"=a" (REG(eax)),  \
			 "=d" (REG(edx)));
		}
		else {
		  struct timeval tv;
		  unsigned long long t;

		  /* try to do the best you can to return something
		   * meaningful, assume clk=100MHz */
		  gettimeofday(&tv, NULL);
		  t = (unsigned long long)tv.tv_sec*100000000 +
		      (unsigned long long)tv.tv_usec*100;
		  REG(eax)=((int *)&t)[0];
		  REG(edx)=((int *)&t)[1];
		}
		(in_dpmi? scp->eip:LWORD(eip)) += 2;
		return 1;
	}
	else if ((((csp[1] & 0xfc)==0x20)||(csp[1]==0x24)||(csp[1]==0x26)) &&
		((csp[2] & 0xc0) == 0xc0)) {
		unsigned long *cdt, *srg;
		int idx;
		boolean rnotw;

		rnotw = ((csp[1]&2)==0);
		idx = (csp[2]>>3)&7;
		switch (csp[1]&7) {
			case 0: case 2: cdt=CRs;
					if (idx>4) return 0;
					break;
			case 1: case 3: cdt=DRs; break;
			case 4: case 6: cdt=TRs;
					if (idx<6) return 0;
					idx -= 6;
					break;
			default: return 0;
		}

		switch (csp[2]&7) {
			case 0: srg = (in_dpmi? &(scp->eax):(unsigned long *)&(REGS.eax));
				break;
			case 1: srg = (in_dpmi? &(scp->ecx):(unsigned long *)&(REGS.ecx));
				break;
			case 2: srg = (in_dpmi? &(scp->edx):(unsigned long *)&(REGS.edx));
				break;
			case 3: srg = (in_dpmi? &(scp->ebx):(unsigned long *)&(REGS.ebx));
				break;
			case 6: srg = (in_dpmi? &(scp->esi):(unsigned long *)&(REGS.esi));
				break;
			case 7: srg = (in_dpmi? &(scp->edi):(unsigned long *)&(REGS.edi));
				break;
			default:	/* ESP(4),EBP(5) */
				return 0;
		}
		if (rnotw) {
		  *srg = cdt[idx];
		  g_printf("CPU: read =%08lx\n",*srg);
		} else {
		  g_printf("CPU: write=%08lx\n",*srg);
		  if (cdt==CRs) {
		    /* special cases... they are too many, I hope this
		     * will suffice for all */
		    if (idx==0) cdt[idx] = (*srg&0xe005002f)|0x10;
		     else if ((idx==1)||(idx==4)) return 0;
		  }
		  else cdt[idx] = *srg;
		}
		(in_dpmi? scp->eip:LWORD(eip)) += 3;
		return 1;
	}
	/* all other trapped combinations:
	 *	0f 0a, 0c..0f, 27, 29, 2b..2f, c2..c6, d0..fe, etc.
	 */
	return 0;
}

/* 
 * DANG_BEGIN_FUNCTION cpu_setup
 *
 * description:
 *  Setup initial interrupts which can be revectored so that the kernel
 * does not need to return to DOSEMU if such an interrupt occurs.
 *
 * DANG_END_FUNCTION
 *
 */
void cpu_setup(void)
{
  extern void int_vector_setup(void);
  extern void init_cpu (void);

  int_vector_setup();

  /* ax,bx,cx,dx,si,di,bp,fs,gs can probably can be anything */
  REG(eax) = 0;
  REG(ebx) = 0;
  REG(ecx) = 0;
  REG(edx) = 0;
  REG(esi) = 0;
  REG(edi) = 0;
  REG(ebp) = 0;
  REG(eip) = 0x7c00;
  REG(cs) = 0;			/* Some boot sectors require cs=0 */
  REG(esp) = 0x100;
  REG(ss) = 0x30;		/* This is the standard pc bios stack */
  REG(es) = 0;			/* standard pc es */
  REG(ds) = 0x40;		/* standard pc ds */
  REG(fs) = 0;
  REG(gs) = 0;
#if 0
  REG(eflags) |= (IF | VIF | VIP);
#else
  REG(eflags) |= (VIF | VIP);
#endif

#ifdef X86_EMULATOR
  if (config.cpuemu) {
    init_cpu();
  }
#endif
}



#ifdef USE_INT_QUEUE
int
do_hard_int(int intno)
{
  queue_hard_int(intno, NULL, NULL);
  return (1);
}
#endif

int
do_soft_int(int intno)
{
  do_int(intno);
  return 1;
}

