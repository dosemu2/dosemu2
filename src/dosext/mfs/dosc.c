/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "config.h"
#include "emu.h"

#define DOSC_GENERIC	1
#define DOSC_NOTIFY	0xdc
#define DOSC_RUNNING    0xdd

#define E6_INT2f11	0x2f11


struct dosc_iregs {
  Bit16u ax, bx, cx, dx, si, di, bp;
  Bit16u ds, es;
  Bit16u ip, cs, flags;
} __attribute__ ((packed));

static void dosc_iregs_to_vm86regs(const struct dosc_iregs *r)
{
  _AX = r->ax;
  _BX = r->bx;
  _CX = r->cx;
  _DX = r->dx;
  _SI = r->si;
  _DI = r->di;
  _BP = r->bp;
  _DS = r->ds;
  _ES = r->es;
  _IP = r->ip;
  _CS = r->cs;
  _FLAGS = r->flags;
}

static int pass_to_int2f(Bit16u *ssp)
{
  struct vm86_regs saved_regs = REGS; /* first save the regs */
  /* expected stack layout pointed to by ssp:
   *
   * word 0	return IP (from near call)
   * word 1	dosemu_generic mode
   * word 2	AX for INT2f
   * word 3	DOS far ptr to dosc_iregs
   */
  const struct dosc_iregs *r = (void *)SEGOFF2LINEAR(ssp[4], ssp[3]);
  int ret;

  if (running_DosC && running_DosC <= 1937) {
     d_printf("Redirector: INT2f, AX=%04x unsupported by DosC build %d\n",
		running_DosC, r->ax);
     return 1;
  }

  dosc_iregs_to_vm86regs(r);

  if (ssp[2] == 0x111e) {
    /* expected stack layout pointed to by ssp:
     * word 5	orig AX to be placed on stack
     */
    /* NOTE: SS:SP are _not_ changed, we jus adjust SP to point to
     * word 5 on the stack (original AX)
     */
    _SP += 5*2;
  }
  ret = mfs_redirector();
  REGS = saved_regs;	/* we restore all regs, as we return to DosC' C-code */
  return ret;
}


int dosc_interface(void)
{
  switch (HI(ax)) {
    case DOSC_NOTIFY: {  /* install check and notify */
      running_DosC = LWORD(ebx);
      LWORD(eax) = (Bit16u)DOSEMU_VERSION_CODE;   
      LWORD(edx) = DOSEMU_VERSION_CODE >> 16;
      c_printf("Booted DosC kernel build %d\n", running_DosC);
      break;
    }
    case DOSC_RUNNING: {
      LWORD(eax) = 0;
      LWORD(ebx) = running_DosC;
      break;
    }
    case DOSC_GENERIC: {
      Bit16u *ssp = SEG_ADR((Bit16u *), ss, sp);
      int mode = ssp[1];
      switch (mode) {
        case E6_INT2f11: return pass_to_int2f(ssp);
      }
    }
  }
  return 1;
}
