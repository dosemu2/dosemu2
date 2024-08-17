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
#include <limits.h>
#include <sys/time.h>
#include <errno.h>
#include <fenv.h>

#include "dosemu_config.h"
#include "memory.h"
#include "emu.h"
#ifdef __linux__
#include "sys_vm86.h"
#endif
#include "port.h"
#include "int.h"
#include "emudpmi.h"
#include "priv.h"
#include "instremu.h"
#include "kvm.h"

#ifdef X86_EMULATOR
#include "simx86/syncpu.h"
#include "cpu-emu.h"
#define CRs	TheCPU.cr
#define DRs	TheCPU.dr
#define TRs	TheCPU.tr
#else
/* Extra CPU registers. Note that GDTR,LDTR,IDTR are internal
 * to the cpuemu and are defined in emu-ldt.c:
 */
static unsigned int CRs[5] =
{
	0x00000013,	/* valid bits: 0xe005003f */
	0x00000000,	/* invalid */
	0x00000000,
	0x00000000,
	0x00000000
};

/*
 * DR0-3 = linear address of breakpoint 0-3
 * DR4=5 = reserved
 * DR6	b0-b3 = BP active
 *	b13   = BD
 *	b14   = BS
 *	b15   = BT
 * DR7	b0-b1 = G:L bp#0
 *	b2-b3 = G:L bp#1
 *	b4-b5 = G:L bp#2
 *	b6-b7 = G:L bp#3
 *	b8-b9 = GE:LE
 *	b13   = GD
 *	b16-19= LLRW bp#0	LL=00(1),01(2),11(4)
 *	b20-23= LLRW bp#1	RW=00(x),01(w),11(rw)
 *	b24-27= LLRW bp#2
 *	b28-31= LLRW bp#3
 */
static unsigned int DRs[8] =
{
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xffff1ff0,
	0x00000400,
	0xffff1ff0,
	0x00000400
};

static unsigned int TRs[2] =
{
	0x00000000,
	0x00000000
};
#endif

/* fpu_state needs to be paragraph aligned for fxrstor/fxsave */
emu_fpstate vm86_fpu_state __attribute__((aligned(16)));
fenv_t dosemu_fenv;
static void fpu_reset(void);

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
int cpu_trap_0f (unsigned char *csp, cpuctx_t *scp)
{
	int increment_ip = 0;
	uint16_t *reg16[8] = { &LWORD(eax), &LWORD(ecx), &LWORD(edx), &LWORD(ebx),
		&LWORD(esp), &LWORD(ebp), &LWORD(esi), &LWORD(edi) };
	g_printf("CPU: TRAP op 0F %02x %02x\n",csp[1],csp[2]);

	if (csp[1] == 0x06) {
		/* CLTS - ignore */
		increment_ip = 2;
	}
	else if (csp[1] == 0x31) {
		/* ref: Van Gilluwe, "The Undocumented PC". The program
		 * 'cpurdtsc.exe' traps here */
		{
		  struct timeval tv;
		  unsigned long long t;

		  /* try to do the best you can to return something
		   * meaningful, assume clk=100MHz */
		  gettimeofday(&tv, NULL);
		  t = (unsigned long long)tv.tv_sec*100000000 +
		      (unsigned long long)tv.tv_usec*100;
		  REG(eax)=t & 0xffffffff;
		  REG(edx)=t >> 32;
		}
		increment_ip = 2;
	}
	else if ((((csp[1] & 0xfc)==0x20)||(csp[1]==0x24)||(csp[1]==0x26)) &&
		((csp[2] & 0xc0) == 0xc0)) {
		unsigned int *cdt;
		unsigned int *srg;
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
			case 0: srg = (scp ? &_eax:&_EAX);
				break;
			case 1: srg = (scp ? &_ecx:&_ECX);
				break;
			case 2: srg = (scp ? &_edx:&_EDX);
				break;
			case 3: srg = (scp ? &_ebx:&_EBX);
				break;
			case 6: srg = (scp ? &_esi:&_ESI);
				break;
			case 7: srg = (scp ? &_edi:&_EDI);
				break;
			default:	/* ESP(4),EBP(5) */
				return 0;
		}
		if (rnotw) {
		  *srg = cdt[idx];
		  g_printf("CPU: read =%08x\n",*srg);
		} else {
		  g_printf("CPU: write=%08x\n",*srg);
		  if (cdt==CRs) {
		    /* special cases... they are too many, I hope this
		     * will suffice for all */
		    if (idx==0) cdt[idx] = (*srg&0xe005002f)|0x10;
		     else if ((idx==1)||(idx==4)) return 0;
		  }
		  else cdt[idx] = *srg;
		}
		increment_ip = 3;
	} else if (csp[1] == 0) {  // SLDT, STR, ...
		switch (csp[2] & 0xc0) {
		case 0xc0: // register dest
			*reg16[csp[2] & 7] = 0;
			increment_ip = 3;
			break;
		default:
			error("unsupported SLDT dest %x\n", csp[2]);
			increment_ip = instr_len(csp, 0);
			break;
		}
	} else if (csp[1] == 1) {  // SGDT, SIDT, SMSW ...
		switch (csp[2] & 0xc0) {
		case 0xc0: // register dest
			/* some meaningful value for smsw, 0 for rest */
			*reg16[csp[2] & 7] = ((csp[2] & 0x28) == 0x20) ? 0x31 : 0;
			increment_ip = 3;
			break;
		default:
			error("unsupported SMSW dest %x\n", csp[2]);
			increment_ip = instr_len(csp, 0);
			break;
		}
	}
	if (increment_ip) {
		if (scp)
			_eip += increment_ip;
		else
			LWORD(eip) += increment_ip;
		return 1;
	}
	/* all other trapped combinations:
	 *	0f 0a, 0c..0f, 27, 29, 2b..2f, c2..c6, d0..fe, etc.
	 */
	return 0;
}

void cpu_reset(void)
{
#ifdef X86_EMULATOR
  reset_emu_cpu();
#endif
  /* ax,bx,cx,dx,si,di,bp,fs,gs can probably can be anything */
  REG(eax) = 0;
  REG(ebx) = 0;
  REG(ecx) = 0;
  REG(edx) = 0;
  REG(esi) = 0;
  REG(edi) = 0;
  REG(ebp) = 0;
  REG(eip) = 0;
  SREG(cs) = 0xffff;
  REG(esp) = 0xfffe;
  SREG(ss) = 0;		/* This is the standard pc bios stack */
  SREG(es) = 0;			/* standard pc es */
  SREG(ds) = 0x40;		/* standard pc ds */
  SREG(fs) = 0;
  SREG(gs) = 0;
  REG(eflags) = 0;
  fpu_reset();
}

#if 0
static void fpu_init(void)
{
#ifdef __x86_64__
  vm86_fpu_state.cwd = 0x037F;
  vm86_fpu_state.swd = 0;
  vm86_fpu_state.ftw = 0xFFFF;       // bochs
#else
  vm86_fpu_state.cw = 0x40;
  vm86_fpu_state.sw = 0;
  vm86_fpu_state.tag = 0xFFFF;       // bochs
#endif
}
#endif

static void fpu_reset(void)
{
  vm86_fpu_state.cwd = 0x0040;
  vm86_fpu_state.swd = 0;
//  vm86_fpu_state.ftw = 0x5555;       //bochs
  if (config.cpu_vm == CPUVM_KVM || config.cpu_vm_dpmi == CPUVM_KVM)
    kvm_update_fpu();
}

static Bit8u fpu_io_read(ioport_t port, void *arg)
{
  return 0xff;
}

static void fpu_io_write(ioport_t port, Bit8u val, void *arg)
{
  switch (port) {
  case 0xf0:
    /* We need to check if the FPU exception is pending, and set IGNNE
     * if it is, before untriggering an IRQ. But we don't (and probably
     * can't) emulate IGNNE properly. So the trick is to do fnclex in
     * bios.S, then untrigger IRQ unconditionally. */
    pic_untrigger(13); /* done by default via int75 handler in bios.S */
    break;
  case 0xf1:
    fpu_reset();
    break;
  }
}

static int fpu_is_masked(void)
{
    uint8_t imr[2] = { [0] = port_inb(0x21), [1] = port_inb(0xa1) };
    uint16_t real_imr = (imr[1] << 8) | imr[0];
    return ((imr[0] & 4) || !!(real_imr & (1 << 13)));
}

void raise_fpu_irq(void)
{
  if (fpu_is_masked() || !isset_IF_async()) {
    error("FPU IRQ cannot be injected (%i %i), bye\n",
	fpu_is_masked(), isset_IF());
    leavedos(2);
  }
  pic_request(13);
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
  emu_iodev_t io_dev = {};
  int orig_vm, orig_vm_dpmi;

  io_dev.read_portb = fpu_io_read;
  io_dev.write_portb = fpu_io_write;
  io_dev.read_portw = NULL;
  io_dev.write_portw = NULL;
  io_dev.read_portd = NULL;
  io_dev.write_portd = NULL;
  io_dev.start_addr = 0xf0;
  io_dev.end_addr = 0xff;
  io_dev.handler_name = "Math Coprocessor";
  port_register_handler(io_dev, 0);

  savefpstate(vm86_fpu_state);
#ifdef FE_NOMASK_ENV
  feenableexcept(FE_DIVBYZERO | FE_OVERFLOW);
#endif
  fegetenv(&dosemu_fenv);

  orig_vm = config.cpu_vm;
  orig_vm_dpmi = config.cpu_vm_dpmi;
  if (config.cpu_vm == -1)
    config.cpu_vm =
#if defined(__x86_64__)
	CPUVM_KVM
#elif defined(__i386__)
	CPUVM_VM86
#else
	CPUVM_EMU
#endif
	;
  if (config.cpu_vm_dpmi == -1)
    config.cpu_vm_dpmi =
#if defined(__x86_64__) || defined(__i386__)
	CPUVM_KVM
#else
	CPUVM_EMU
#endif
	;

  if ((config.cpu_vm == CPUVM_KVM || config.cpu_vm_dpmi == CPUVM_KVM) &&
      !init_kvm_cpu()) {
    warn("KVM not available: %s\n", strerror(errno));
    /* if explicitly requested, don't continue */
    if (orig_vm == CPUVM_KVM || orig_vm_dpmi == CPUVM_KVM) {
      leavedos(21);
      return;
    }
    if (config.cpu_vm == CPUVM_KVM)
      config.cpu_vm = CPUVM_EMU;
    if (config.cpu_vm_dpmi == CPUVM_KVM) {
#ifdef X86_EMULATOR
      config.cpu_vm_dpmi = CPUVM_EMU;
#else
      config.cpu_vm_dpmi = CPUVM_NATIVE;
#endif
    }
  }

#ifdef __i386__
  if (config.cpu_vm == CPUVM_VM86) {
#if 0
    if (vm86((void *)VM86_PLUS_INSTALL_CHECK) != -1 ||
		errno != EFAULT)
#else
    if (vm86_plus(VM86_PLUS_INSTALL_CHECK, 0) != 0)
#endif
    {
      error("vm86 service not available in your kernel, %s\n", strerror(errno));
#ifdef X86_EMULATOR
      config.cpu_vm = CPUVM_EMU;
#else
      exit(1);
#endif
    }
  }
#endif

#ifdef X86_EMULATOR
  if (config.cpu_vm == CPUVM_EMU || config.cpu_vm_dpmi == CPUVM_EMU) {
    if (config.cpu_vm == CPUVM_EMU)
      dbug_printf("using CPU emulation for vm86()\n");
    if (config.cpu_vm_dpmi == CPUVM_EMU)
      dbug_printf("using CPU emulation for DPMI\n");
  }
  init_emu_cpu();
#endif
}


void fxsave_to_fsave(const struct emu_fpxstate *fxsave, struct emu_fsave *fptr)
{
  unsigned int tmp;
  int i;

  fptr->cw = fxsave->cwd | 0xffff0000u;
  fptr->sw = fxsave->swd | 0xffff0000u;
  /* Expand the valid bits */
  tmp = fxsave->ftw;			/* 00000000VVVVVVVV */
  tmp = (tmp | (tmp << 4)) & 0x0f0f;	/* 0000VVVV0000VVVV */
  tmp = (tmp | (tmp << 2)) & 0x3333;	/* 00VV00VV00VV00VV */
  tmp = (tmp | (tmp << 1)) & 0x5555;	/* 0V0V0V0V0V0V0V0V */
  /* Transform each bit from 1 to 00 (valid) or 0 to 11 (empty).
     The kernel will convert it back, so no need to worry about zero
     and special states 01 and 10. */
  tmp = ~(tmp | (tmp << 1));
  fptr->tag = tmp | 0xffff0000u;
  fptr->ipoff = fxsave->fip;
  fptr->cssel = (uint16_t)fxsave->fcs | ((uint32_t)fxsave->fop << 16);
  fptr->dataoff = fxsave->fdp;
  fptr->datasel = fxsave->fds;
  for (i = 0; i < 8; i++)
    memcpy(&fptr->st[i], &fxsave->st[i], sizeof(fptr->st[i]));
}

/*
 * FPU tag word conversions.
 */
static unsigned short twd_i387_to_fxsr(unsigned short twd)
{
	unsigned int tmp; /* to avoid 16 bit prefixes in the code */

	/* Transform each pair of bits into 01 (valid) or 00 (empty) */
	tmp = ~twd;
	tmp = (tmp | (tmp>>1)) & 0x5555; /* 0V0V0V0V0V0V0V0V */
	/* and move the valid bits to the lower byte. */
	tmp = (tmp | (tmp >> 1)) & 0x3333; /* 00VV00VV00VV00VV */
	tmp = (tmp | (tmp >> 2)) & 0x0f0f; /* 0000VVVV0000VVVV */
	tmp = (tmp | (tmp >> 4)) & 0x00ff; /* 00000000VVVVVVVV */

	return tmp;
}

/* NOTE: this function does NOT memset the "unused" fxsave fields.
 * We preserve the previous fxsave context. */
void fsave_to_fxsave(const struct emu_fsave *fptr, struct emu_fpxstate *fxsave)

{
	int i;

	fxsave->cwd = fptr->cw;
	fxsave->swd = fptr->sw;
	fxsave->ftw = twd_i387_to_fxsr(fptr->tag);
	fxsave->fop = (uint16_t) ((uint32_t) fptr->cssel >> 16);
	fxsave->fip = fptr->ipoff;
	fxsave->fcs = (fptr->cssel & 0xffff);
	fxsave->fdp = fptr->dataoff;
	fxsave->fds = fptr->datasel;

	for (i = 0; i < 8; ++i) {
		/* fxsave's regs are larger so memset first */
		memset(&fxsave->st[i], 0, sizeof(fxsave->st[0]));
		memcpy(&fxsave->st[i], &fptr->st[i], sizeof(fptr->st[0]));
	}
}
