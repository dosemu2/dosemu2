/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * native DPMI backend
 *
 * Author: Stas Sergeev
 */
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/ptrace.h>
#include <sys/user.h>
#endif
#include <sys/syscall.h>
#include <Asm/ldt.h>
#include "init.h"
#include "libpcl/pcl.h"
#include "cpu.h"
#include "dosemu_debug.h"
#include "utilities.h"
#include "bitops.h"
#include "mapping.h"
#include "emudpmi.h"
#include "dnative.h"
#include "dnpriv.h"

#define USE_CPIO 0

#define EMU_X86_FXSR_MAGIC	0x0000
static coroutine_t dpmi_tid;
static cohandle_t co_handle;
static int dpmi_ret_val;
static sigcontext_t emu_stack_frame;
static int in_dpmi_thr;
static int dpmi_thr_running;
static cpuctx_t *dpmi_scp;
static uint8_t _ldt_buffer[LDT_ENTRIES * LDT_ENTRY_SIZE];
static char *xstorage;
static uint8_t cpio_bm[65536 / 8];

static void copy_context(sigcontext_t *d, sigcontext_t *s)
{
#ifdef __linux__
  /* keep pointer to FPU state the same */
  fpregset_t fptr = d->fpregs;
#endif
  *d = *s;
#ifdef __linux__
  d->fpregs = fptr;
#endif
}

#ifdef __i386__
/* On i386 only, if SSE is available (i.e. since the Pentium III),
   the kernel will use FXSAVE to save FPU state, then put the following
   on the signal stack:
   * FXSAVE format converted to FSAVE format (108 bytes)
   * status and magic field where magic == X86_FXSR_MAGIC (4 bytes)
   * FXSAVE format (512 bytes), which can be used directly by our loadfpstate
   However, when restoring FPU state it will only use the mxcsr and xmm
   fields from the FXSAVE format, and take everything else from the FSAVE
   format, so we must "undo" the kernel logic and put those fields into the
   FSAVE region.
   see also arch/x86/kernel/fpu/regset.c in Linux kernel */
static void convert_from_fxsr(fpregset_t fptr,
			      const struct emu_fpxstate *fxsave)
{
  static_assert(sizeof(*fptr) == sizeof(struct emu_fsave),
		  "size mismatch");
  fxsave_to_fsave(fxsave, (struct emu_fsave *)fptr);
}
#endif

static void copy_to_dpmi(sigcontext_t *scp, cpuctx_t *s)
{
#ifdef __x86_64__
  /* needs to clear high part of RIP and RSP because AMD CPUs
   * check whole 64bit regs against 32bit CS and SS limits on iret */
  _scp_rip = 0;
  _scp_rsp = 0;
#endif
#define _C(x) _scp_##x = get_##x(s)
  _C(es);
  _C(ds);
  _C(ss);
  _C(cs);
  _C(fs);
  _C(gs);
  _C(eax);
  _C(ebx);
  _C(ecx);
  _C(edx);
  _C(esi);
  _C(edi);
  _C(ebp);
  _C(esp);
  _C(eip);
  _C(eflags);
  _C(trapno);
  _C(err);
  _scp_cr2 = (uintptr_t)MEM_BASE32(get_cr2(s));

  if (scp->fpregs) {
    void *fpregs = scp->fpregs;
#ifdef __x86_64__
    static_assert(sizeof(*scp->fpregs) == sizeof(vm86_fpu_state),
		  "size mismatch");
#else
    /* i386: convert fxsave state to fsave state */
    convert_from_fxsr(scp->fpregs, &vm86_fpu_state);
    if ((scp->fpregs->status >> 16) != EMU_X86_FXSR_MAGIC)
      return;
    fpregs = &scp->fpregs->status + 1;
#endif
    memcpy(fpregs, &vm86_fpu_state, sizeof(vm86_fpu_state));
  }
}

static void copy_to_emu(cpuctx_t *d, sigcontext_t *scp)
{
#define _D(x) get_##x(d) = _scp_##x
  _D(es);
  _D(ds);
  _D(ss);
  _D(cs);
  _D(fs);
  _D(gs);
  _D(eax);
  _D(ebx);
  _D(ecx);
  _D(edx);
  _D(esi);
  _D(edi);
  _D(ebp);
  _D(esp);
  _D(eip);
  _D(eflags);
  _D(trapno);
  _D(err);
  get_cr2(d) = DOSADDR_REL(LINP(_scp_cr2));
  if (scp->fpregs) {
    void *fpregs = scp->fpregs;
#ifdef __x86_64__
    static_assert(sizeof(*scp->fpregs) == sizeof(vm86_fpu_state),
		"size mismatch");
#else
    if ((scp->fpregs->status >> 16) == EMU_X86_FXSR_MAGIC)
      fpregs = &scp->fpregs->status + 1;
    else {
      fsave_to_fxsave(fpregs, &vm86_fpu_state);
      return;
    }
#endif
    memcpy(&vm86_fpu_state, fpregs, sizeof(vm86_fpu_state));
  }
}

static void _set_cpio(int base, int size)
{
  int i;

  assert(base + size <= 65536);
  for (i = base; i < base + size; i++)
    set_bit(i, cpio_bm);
}

static void dpmi_thr(void *arg);

/* ======================================================================== */
/*
 * DANG_BEGIN_FUNCTION native_dpmi_control
 *
 * This function is similar to the vm86() syscall in the kernel and
 * switches to dpmi code.
 *
 * DANG_END_FUNCTION
 */

static int _control(cpuctx_t *scp, char *storage, int *r_size)
{
    unsigned saved_IF = (_eflags & IF);

    dpmi_scp = scp;
    xstorage = storage;
    *r_size = 0;

    _eflags = get_EFLAGS(_eflags);
    if (in_dpmi_thr) {
        /* if we are going directly to a sighandler, mask async signals. */
        signal_restore_async_sigs();
        signal_switch_to_dpmi();
    } else {
        dpmi_tid = co_create(co_handle, dpmi_thr, NULL, NULL, SIGSTACK_SIZE);
    }
    dpmi_thr_running++;
    co_call(dpmi_tid);
    dpmi_thr_running--;
    if (in_dpmi_thr)
        signal_switch_to_dosemu();
    assert(_eflags & IF);
    if (!saved_IF)
        _eflags &= ~IF;
    _eflags &= ~VIF;

    signal_unblock_async_sigs();
    return dpmi_ret_val;
}

static void _dpmi_exit(cpuctx_t *scp)
{
    assert(in_dpmi_thr);
    D_printf("DPMI: leaving\n");
    dpmi_ret_val = DPMI_RET_EXIT;
}

void dpmi_return(sigcontext_t *scp, int retcode)
{
    /* only used for CPUVM_NATIVE (from sigsegv.c: dosemu_fault1()) */
    if (!DPMIValidSelector(_scp_cs)) {
        dosemu_error("Return to dosemu requested within dosemu context\n");
        return;
    }
    dpmi_ret_val = retcode;
    if (retcode == DPMI_RET_EXIT) {
        copy_context(scp, &emu_stack_frame);
        return;
    }
    copy_to_emu(dpmi_scp, scp);
    /* signal handlers start with clean FPU state, but we unmask
       overflow/division by zero in main code */
    fesetenv(&dosemu_fenv);
    signal_return_to_dosemu();
    co_resume(co_handle);
    signal_return_to_dpmi();
    if (dpmi_ret_val == DPMI_RET_EXIT)
        copy_context(scp, &emu_stack_frame);
    else
        copy_to_dpmi(scp, dpmi_scp);
}

void dpmi_switch_sa(int sig, siginfo_t * inf, void *uc)
{
    ucontext_t *uct = uc;
    sigcontext_t *scp = &uct->uc_mcontext;
    copy_context(&emu_stack_frame, scp);
    copy_to_dpmi(scp, dpmi_scp);
    unsetsig(DPMI_TMP_SIG);
    deinit_handler(scp, &uct->uc_flags);
}

#if USE_CPIO
__attribute__((warn_unused_result))
static int port_outb(ioport_t port, Bit8u byte)
{
    if (!test_bit(port, cpio_bm))
	return -1;
  //TODO
    return 0;
}

__attribute__((warn_unused_result))
static int port_outw(ioport_t port, Bit16u word)
{
    int i;

    for (i = port; i < port + sizeof(word); i++) {
	if (!test_bit(i, cpio_bm))
	    return -1;
    }
  //TODO
    return 0;
}

__attribute__((warn_unused_result))
static int port_outd(ioport_t port, Bit32u dword)
{
    int i;

    for (i = port; i < port + sizeof(dword); i++) {
	if (!test_bit(i, cpio_bm))
	    return -1;
    }
  //TODO
    return 0;
}

static int port_rep_outb(ioport_t port, Bit8u *base, int df, Bit32u count)
{
	int i;
	int incr = df? -1: 1;
	Bit8u *dest = base;

	if (count==0) return 0;
	for (i = port; i < port + count; i++) {
	    if (!test_bit(i, cpio_bm))
		return -1;
	}
	i_printf("Doing REP outsb(%#x) %d bytes at %p, DF %d\n", port,
		count, base, df);
	while (count--) {
	    int rc = port_outb(port, *dest);
	    assert(rc != -1);
	    dest += incr;
	}
	return dest-base;
}

static int port_rep_outw(ioport_t port, Bit16u *base, int df, Bit32u count)
{
	int i;
	int incr = df? -1: 1;
	Bit16u *dest = base;

	if (count==0) return 0;
	for (i = port; i < port + count; i++) {
	    if (!test_bit(i, cpio_bm))
		return -1;
	}
	i_printf("Doing REP outsw(%#x) %d words at %p, DF %d\n", port,
		count, base, df);
	while (count--) {
	    int rc = port_outw(port, *dest);
	    assert(rc != -1);
	    dest += incr;
	}
	return (Bit8u *)dest-(Bit8u *)base;
}

static int port_rep_outd(ioport_t port, Bit32u *base, int df, Bit32u count)
{
	int i;
	int incr = df? -1: 1;
	Bit32u *dest = base;

	if (count==0) return 0;
	for (i = port; i < port + count; i++) {
	    if (!test_bit(i, cpio_bm))
		return -1;
	}
	while (count--) {
	    int rc = port_outd(port, *dest);
	    assert(rc != -1);
	    dest += incr;
	}
	return (Bit8u *)dest-(Bit8u *)base;
}
#endif

static uint32_t client_esp(sigcontext_t *scp)
{
    if(_Segments(_ldt_buffer, _scp_ss >> 3).is_32)
	return _scp_esp;
    else
	return (_scp_esp)&0xffff;
}

static uint32_t client_eip(sigcontext_t *scp)
{
    if(_Segments(_ldt_buffer, _scp_cs >> 3).is_32)
	return _scp_eip;
    else
	return (_scp_eip)&0xffff;
}

static char *_show_state(sigcontext_t *scp)
{
    static char buf[4096];
    int pos = 0;
    unsigned char *csp2, *ssp2;
    dosaddr_t daddr, saddr;
    pos += sprintf(buf + pos, "eip: 0x%08x  esp: 0x%08x  eflags: 0x%08x\n"
	     "\ttrapno: 0x%02x  errorcode: 0x%08x  cr2: 0x%08"PRI_RG"\n"
	     "\tcs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x  fs: 0x%04x  gs: 0x%04x\n",
	     _scp_eip, _scp_esp, _scp_eflags, _scp_trapno, _scp_err,
	     _scp_cr2, _scp_cs, _scp_ds, _scp_es, _scp_ss, _scp_fs, _scp_gs);
    pos += sprintf(buf + pos, "EAX: %08x  EBX: %08x  ECX: %08x  EDX: %08x\n",
	     _scp_eax, _scp_ebx, _scp_ecx, _scp_edx);
    pos += sprintf(buf + pos, "ESI: %08x  EDI: %08x  EBP: %08x\n",
	     _scp_esi, _scp_edi, _scp_ebp);
    /* display the 10 bytes before and after CS:EIP.  the -> points
     * to the byte at address CS:EIP
     */
    if (!((_scp_cs) & 0x0004)) {
      /* GTD */
#if 0
      csp2 = (unsigned char *) _scp_rip;
      daddr = 0;
#else
      return buf;
#endif
    }
    else {
      /* LDT */
      csp2 = SEL_ADR(_scp_cs, _scp_eip);
      daddr = GetSegmentBase(_scp_cs) + client_eip(scp);
    }
    /* We have a problem here, if we get a page fault or any kind of
     * 'not present' error and then we try accessing the code/stack
     * area, we fall into another fault which likely terminates dosemu.
     */
    {
      int i;
      #define CSPP (csp2 - 10)
      pos += sprintf(buf + pos, "OPS  : ");
      if ((CSPP >= &mem_base[0] && CSPP + 10 < &mem_base[0x110000]) ||
	  ((mapping_find_hole((uintptr_t)CSPP, (uintptr_t)CSPP + 10, 1) == MAP_FAILED) &&
	   dpmi_is_valid_range(daddr - 10, 10))) {
	for (i = 0; i < 10; i++)
	  pos += sprintf(buf + pos, "%02x ", CSPP[i]);
      } else {
	pos += sprintf(buf + pos, "<invalid memory> ");
      }
      if ((csp2 >= &mem_base[0] && csp2 + 10 < &mem_base[0x110000]) ||
	  ((mapping_find_hole((uintptr_t)csp2, (uintptr_t)csp2 + 10, 1) == MAP_FAILED) &&
	   dpmi_is_valid_range(daddr, 10))) {
	pos += sprintf(buf + pos, "-> ");
	for (i = 0; i < 10; i++)
	  pos += sprintf(buf + pos, "%02x ", *csp2++);
	pos += sprintf(buf + pos, "\n");
      } else {
	pos += sprintf(buf + pos, "CS:EIP points to invalid memory\n");
      }
      if (!((_scp_ss) & 0x0004)) {
        /* GDT */
#if 0
        ssp2 = (unsigned char *) _ecp_rsp;
        saddr = 0;
#else
        return buf;
#endif
      }
      else {
        /* LDT */
	ssp2 = SEL_ADR(_scp_ss, _scp_esp);
	saddr = GetSegmentBase(_scp_ss) + client_esp(scp);
      }
      #define SSPP (ssp2 - 10)
      pos += sprintf(buf + pos, "STACK: ");
      if ((SSPP >= &mem_base[0] && SSPP + 10 < &mem_base[0x110000]) ||
	  ((mapping_find_hole((uintptr_t)SSPP, (uintptr_t)SSPP + 10, 1) == MAP_FAILED) &&
	   dpmi_is_valid_range(saddr - 10, 10))) {
	for (i = 0; i < 10; i++)
	  pos += sprintf(buf + pos, "%02x ", SSPP[i]);
      } else {
	pos += sprintf(buf + pos, "<invalid memory> ");
      }
      if ((ssp2 >= &mem_base[0] && ssp2 + 10 < &mem_base[0x110000]) ||
	  ((mapping_find_hole((uintptr_t)ssp2, (uintptr_t)ssp2 + 10, 1) == MAP_FAILED) &&
	   dpmi_is_valid_range(saddr, 10))) {
	pos += sprintf(buf + pos, "-> ");
	for (i = 0; i < 10; i++)
	  pos += sprintf(buf + pos, "%02x ", *ssp2++);
	pos += sprintf(buf + pos, "\n");
      } else {
	pos += sprintf(buf + pos, "SS:ESP points to invalid memory\n");
      }
    }

    return buf;
}

int dpmi_fault(sigcontext_t *scp)
{
#define LWORD32(x,y) {if (_Segments(_ldt_buffer, _scp_cs >> 3).is_32) _scp_##x y; else _scp_LWORD(x) y;}
#define ASIZE_IS_32 (_Segments(_ldt_buffer, _scp_cs >> 3).is_32 ^ prefix67)
#define OSIZE_IS_32 (_Segments(_ldt_buffer, _scp_cs >> 3).is_32 ^ prefix66)
#define _LWECX (ASIZE_IS_32 ? _scp_ecx : _scp_LWORD(ecx))
#define set_LWECX(x) {if (ASIZE_IS_32) _scp_ecx=(x); else _scp_LWORD(ecx) = (x);}
  int ret = DPMI_RET_FAULT;
  /* If this is an exception 0x11, we have to ignore it. The reason is that
   * under real DOS the AM bit of CR0 is not set.
   * Also clear the AC flag to prevent it from re-occuring.
   */
  if (_scp_trapno == 0x11) {
    g_printf("Exception 0x11 occurred, clearing AC\n");
    _scp_eflags &= ~AC;
    ret = DPMI_RET_CLIENT;
  }
#if USE_CPIO
  if (_scp_trapno == 13) {
    Bit32u org_eip;
    int pref_seg;
    int done,is_rep,prefix66,prefix67;
    unsigned char *csp = (unsigned char *) SEL_ADR(_scp_cs, _scp_eip);
    unsigned char *lina = csp;

    /* DANG_BEGIN_REMARK
     * Here we handle all prefixes prior switching to the appropriate routines
     * The exception CS:EIP will point to the first prefix that effects the
     * the faulting instruction, hence, 0x65 0x66 is same as 0x66 0x65.
     * So we collect all prefixes and remember them.
     * - Hans Lermen
     * DANG_END_REMARK
     */

    done=0;
    is_rep=0;
    prefix66=prefix67=0;
    pref_seg=-1;

    do {
      switch (*(csp++)) {
         case 0x66:      /* operand prefix */  prefix66=1; break;
         case 0x67:      /* address prefix */  prefix67=1; break;
         case 0x2e:      /* CS */              pref_seg=_scp_cs; break;
         case 0x3e:      /* DS */              pref_seg=_scp_ds; break;
         case 0x26:      /* ES */              pref_seg=_scp_es; break;
         case 0x36:      /* SS */              pref_seg=_scp_ss; break;
         case 0x65:      /* GS */              pref_seg=_scp_gs; break;
         case 0x64:      /* FS */              pref_seg=_scp_fs; break;
         case 0xf2:      /* repnz */
         case 0xf3:      /* rep */             is_rep=1; break;
         default: done=1;
      }
    } while (!done);
    csp--;
    org_eip = _scp_eip;
    _scp_eip += (csp-lina);

    switch (*csp++) {

    case 0x6e:			/* [rep] outsb */
      if (debug_level('M')>=9)
        D_printf("DPMI: outsb\n");
      if (pref_seg < 0) pref_seg = _scp_ds;
      /* WARNING: no test for (E)SI wrapping! */
      if (ASIZE_IS_32) {		/* a32 outsb */
	int rc = port_rep_outb(_scp_LWORD(edx), (Bit8u *)SEL_ADR(pref_seg,_scp_esi),
	        _scp_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	if (rc == -1)
	  break;
	_scp_esi += rc;
      } else {			/* a16 outsb */
	int rc = port_rep_outb(_scp_LWORD(edx), (Bit8u *)SEL_ADR(pref_seg,_scp_LWORD(esi)),
	        _scp_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	if (rc == -1)
	  break;
	_scp_LWORD(esi) += rc;
      }
      if (is_rep) set_LWECX(0);
      LWORD32(eip,++);
      ret = DPMI_RET_CLIENT;
      break;

    case 0x6f:			/* [rep] outsw/d */
      if (debug_level('M')>=9)
        D_printf("DPMI: outs%s\n", OSIZE_IS_32 ? "d" : "w");
      if (pref_seg < 0) pref_seg = _scp_ds;
      /* WARNING: no test for (E)SI wrapping! */
      if (OSIZE_IS_32) {	/* outsd */
        if (ASIZE_IS_32) {	/* a32 outsd */
	  int rc = port_rep_outd(_scp_LWORD(edx), (Bit32u *)SEL_ADR(pref_seg,_scp_esi),
		_scp_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	  if (rc == -1)
	    break;
	  _scp_esi += rc;
        } else {			/* a16 outsd */
	  int rc = port_rep_outd(_scp_LWORD(edx), (Bit32u *)SEL_ADR(pref_seg,_scp_LWORD(esi)),
		_scp_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	  if (rc == -1)
	    break;
	  _scp_LWORD(esi) += rc;
	}
      }
      else {			/* outsw */
        if (ASIZE_IS_32) {	/* a32 outsw */
	  int rc = port_rep_outw(_scp_LWORD(edx), (Bit16u *)SEL_ADR(pref_seg,_scp_esi),
		_scp_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	  if (rc == -1)
	    break;
	  _scp_esi += rc;
        } else {			/* a16 outsw */
	  int rc = port_rep_outw(_scp_LWORD(edx), (Bit16u *)SEL_ADR(pref_seg,_scp_LWORD(esi)),
		_scp_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	  if (rc == -1)
	    break;
	  _scp_LWORD(esi) += rc;
	}
      }
      if (is_rep) set_LWECX(0);
      LWORD32(eip,++);
      ret = DPMI_RET_CLIENT;
      break;

    case 0xe7: {			/* outw xx */
      int rc;
      if (debug_level('M')>=9)
        D_printf("DPMI: out%s xx\n", OSIZE_IS_32 ? "d" : "w");
      if (OSIZE_IS_32) rc = port_outd((int)csp[0], _scp_eax);
      else rc = port_outw((int)csp[0], _scp_LWORD(eax));
      if (rc == -1)
        break;
      LWORD32(eip, += 2);
      ret = DPMI_RET_CLIENT;
      break;
    }
    case 0xe6: {			/* outb xx */
      int rc;
      if (debug_level('M')>=9)
        D_printf("DPMI: outb xx\n");
      rc = port_outb((int) csp[0], _scp_LO(ax));
      if (rc == -1)
        break;
      LWORD32(eip, += 2);
      ret = DPMI_RET_CLIENT;
      break;
    }
    case 0xef: {			/* outw dx */
      int rc;
      if (debug_level('M')>=9)
        D_printf("DPMI: out%s dx\n", OSIZE_IS_32 ? "d" : "w");
      if (OSIZE_IS_32) rc = port_outd(_scp_LWORD(edx), _scp_eax);
      else rc = port_outw(_scp_LWORD(edx), _scp_LWORD(eax));
      if (rc == -1)
        break;
      LWORD32(eip, += 1);
      ret = DPMI_RET_CLIENT;
      break;
    }
    case 0xee: {			/* outb dx */
      int rc;
      if (debug_level('M')>=9)
        D_printf("DPMI: outb dx\n");
      rc = port_outb(_scp_LWORD(edx), _scp_LO(ax));
      if (rc == -1)
        break;
      LWORD32(eip, += 1);
      ret = DPMI_RET_CLIENT;
      break;
    }
    default:
      _scp_eip = org_eip;
      break;
    } /* switch */
  } /* _trapno==13 */
#endif

  return ret;
}

static void indirect_dpmi_transfer(void)
{
    signative_start();
    signal_set_altstack(1);
    /* for some absolutely unclear reason neither pthread_self() nor
     * pthread_kill() are the memory barriers. */
    asm volatile ("":::"memory");
    pthread_kill(pthread_self(), DPMI_TMP_SIG);
    /* and we are back */
    signal_set_altstack(0);
    signative_stop();
    /* we inherited FPU state from DPMI, so put back to DOSEMU state */
    fesetenv(&dosemu_fenv);
}

static void dpmi_thr(void *arg)
{
    in_dpmi_thr++;
    indirect_dpmi_transfer();
    in_dpmi_thr--;
}

static int _setup(void)
{
    signative_init();
    co_handle = co_thread_init(PCL_C_MC);
    return 0;
}

static void _done(void)
{
    if (in_dpmi_thr && !dpmi_thr_running)
        co_delete(dpmi_tid);
    co_thread_cleanup(co_handle);
}

static int _read_ldt(void *ptr, int bytecount)
{
  int ret = syscall(SYS_modify_ldt, LDT_READ, _ldt_buffer, bytecount);
  if (ret < 0)
    return ret;
  memcpy(ptr, _ldt_buffer, bytecount);
  return ret;
}

static int _write_ldt(const void *ptr, int bytecount, uint64_t base)
{
  struct user_desc ldt_info;
  int offs;

  memcpy(&ldt_info, ptr, sizeof(ldt_info));
  offs  = ldt_info.entry_number * LDT_ENTRY_SIZE;
  assert(bytecount == sizeof(ldt_info) &&
      offs + bytecount <= sizeof(_ldt_buffer));
  memcpy(_ldt_buffer + offs, ptr, bytecount);

  /* NOTE: the real LDT in kernel space uses the real addresses, but
     the LDT we emulate, and DOS applications work with,
     has all base addresses with respect to mem_base */
  if (!ldt_info.seg_not_present)
    ldt_info.base_addr += base;
  return syscall(SYS_modify_ldt, LDT_WRITE, &ldt_info, sizeof(ldt_info));
}

static int _check_verr(unsigned short selector)
{
  int ret = 0;
  asm volatile(
    "verrw %%ax\n"
    "jz 1f\n"
    "xorl %%eax, %%eax\n"
    "1:\n"
    : "=a"(ret)
    : "a"(selector));
  return ret;
}

static int get_dr(pid_t pid, int i, unsigned int *dri)
{
  *dri = ptrace(PTRACE_PEEKUSER, pid,
		(void *)(offsetof(struct user, u_debugreg) + sizeof(int) * i), 0);
  D_printf("DPMI: ptrace peek user dr%d=%x\n", i, *dri);
  return *dri != -1 || errno == 0;
}

static int set_dr(pid_t pid, int i, unsigned long dri)
{
  int r = ptrace(PTRACE_POKEUSER, pid,
		 (void *)(offsetof(struct user, u_debugreg) + sizeof(int) * i), (void *)dri);
  D_printf("DPMI: ptrace poke user r=%d dr%d=%lx\n", r, i, dri);
  return r == 0;
}

static int _debug_breakpoint(int op, cpuctx_t *scp, int err)
{
  pid_t pid, vpid;
  int r, status;

  pid = getpid();
  vpid = fork();
  if (vpid == (pid_t)-1)
    return err;
  if (vpid == 0) {
    unsigned int dr6, dr7;
    /* child ptraces parent */
    r = ptrace(PTRACE_ATTACH, pid, 0, 0);
    D_printf("DPMI: ptrace attach %d op=%d\n", r, op);
    if (r == -1)
      _exit(err);
    do {
      r = waitpid(pid, &status, 0);
    } while (r == pid && !WIFSTOPPED(status));
    if (r == pid) switch (op) {
      case 0: {   /* set */
	int i;
	if(get_dr(pid, 7, &dr7)) for (i=0; i<4; i++) {
	  if ((~dr7 >> (i*2)) & 3) {
	    unsigned mask;
	    if (!set_dr(pid, i, (_LWORD_(ebx) << 16) | _LWORD_(ecx))) {
	      err = 0x25;
	      break;
	    }
	    dr7 |= (3 << (i*2));
	    mask = _HI(dx) & 3; if (mask==2) mask++;
	    mask |= ((_LO(dx)-1) << 2) & 0x0c;
	    dr7 |= mask << (i*4 + 16);
	    if (set_dr(pid, 7, dr7))
	      err = i;
	    break;
	  }
	}
	break;
      }
      case 1:   /* clear */
	if(get_dr(pid, 6, &dr6) && get_dr(pid, 7, &dr7)) {
	  int i = _LWORD(ebx);
	  dr6 &= ~(1 << i);
	  dr7 &= ~(3 << (i*2));
	  dr7 &= ~(15 << (i*4+16));
	  if (set_dr(pid, 6, dr6) && set_dr(pid, 7, dr7))
	    err = 0;
	  break;
	}
      case 2:   /* get */
	if(get_dr(pid, 6, &dr6))
	  err = (dr6 >> _LWORD(ebx)) & 1;
        break;
      case 3:   /* reset */
	if(get_dr(pid, 6, &dr6)) {
          dr6 &= ~(1 << _LWORD(ebx));
          if (set_dr(pid, 6, dr6))
	    err = 0;
	}
        break;
    }
    ptrace(PTRACE_DETACH, pid, 0, 0);
    D_printf("DPMI: ptrace detach\n");
    _exit(err);
  }
  D_printf("DPMI: waitpid start\n");
  r = waitpid(vpid, &status, 0);
  if (r != vpid || !WIFEXITED(status))
    return err;
  err = WEXITSTATUS(status);
  if (err >= 0 && err < 4) {
    if (op == 0)
      _LWORD(ebx) = err;
    else if (op == 2)
      _LWORD(eax) = err;
    err = 0;
  }
  D_printf("DPMI: waitpid end, err=%#x, op=%d\n", err, op);
  return err;
}

static const struct dnative_ops ops = {
  .control = _control,
  .exit = _dpmi_exit,
  .setup = _setup,
  .done = _done,
  .set_cpio = _set_cpio,
  .read_ldt = _read_ldt,
  .write_ldt = _write_ldt,
  .check_verr = _check_verr,
  .debug_breakpoint = _debug_breakpoint,
};

CONSTRUCTOR(static void init(void))
{
  register_dnative_ops(&ops);
}
