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

#include "config.h"
#include "dosemu_config.h"
#include "memory.h"
#include "emu.h"
#ifdef __linux__
#include "sys_vm86.h"
#endif
#include "port.h"
#include "int.h"
#include "dpmi.h"
#include "priv.h"
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
struct _fpstate vm86_fpu_state __attribute__ ((aligned(16)));

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
int cpu_trap_0f (unsigned char *csp, struct sigcontext *scp)
{
	int increment_ip = 0;
	g_printf("CPU: TRAP op 0F %02x %02x\n",csp[1],csp[2]);

	if (csp[1] == 0x06) {
		/* CLTS - ignore */
		increment_ip = 2;
	}
	else if (csp[1] == 0x31) {
		/* ref: Van Gilluwe, "The Undocumented PC". The program
		 * 'cpurdtsc.exe' traps here */
#ifdef X86_EMULATOR
		if (config.cpuemu>1 && config.rdtsc) {
		  REG(eax) = (unsigned long)TheCPU.EMUtime;
		  REG(edx) = (unsigned long)(TheCPU.EMUtime>>32);
		} else
#endif
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
  reset_emu_cpu();
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
#ifdef __x86_64__
  vm86_fpu_state.cwd = 0x0040;
  vm86_fpu_state.swd = 0;
  vm86_fpu_state.ftw = 0x5555;       //bochs
#else
  vm86_fpu_state.cw = 0x40;
  vm86_fpu_state.sw = 0;
  vm86_fpu_state.tag = 0x5555;       // bochs
#endif
}

static Bit8u fpu_io_read(ioport_t port)
{
  return 0xff;
}

static void fpu_io_write(ioport_t port, Bit8u val)
{
  switch (port) {
  case 0xf0:
    /* not sure about this */
#ifdef __x86_64__
    vm86_fpu_state.swd &= ~0x8000;
#else
    vm86_fpu_state.sw &= ~0x8000;
#endif
    break;
  case 0xf1:
    fpu_reset();
    break;
  }
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
  emu_iodev_t io_dev;
  int orig_cpu_vm = config.cpu_vm;
  io_dev.read_portb = fpu_io_read;
  io_dev.write_portb = fpu_io_write;
  io_dev.read_portw = NULL;
  io_dev.write_portw = NULL;
  io_dev.read_portd = NULL;
  io_dev.write_portd = NULL;
  io_dev.irq = PIC_IRQ13;
  io_dev.fd = -1;
  io_dev.start_addr = 0xf0;
  io_dev.end_addr = 0xff;
  io_dev.handler_name = "Math Coprocessor";
  port_register_handler(io_dev, 0);

  int_vector_setup();

  cpu_reset();
  savefpstate(vm86_fpu_state);
  fpu_reset();

  if (config.cpu_vm == -1) {
    if (config.cpuemu)
      config.cpu_vm = CPUVM_EMU;
    else
      config.cpu_vm =
#ifdef __x86_64__
	CPUVM_KVM
#else
	CPUVM_VM86
#endif
	;
  }

  if (config.cpu_vm == CPUVM_KVM && !init_kvm_cpu()) {
    if (orig_cpu_vm == -1) {
      warn("KVM not available: %s\n", strerror(errno));
    } else {
      error("KVM not available: %s\n", strerror(errno));
    }
    config.cpu_vm = CPUVM_EMU;
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
      if (orig_cpu_vm == CPUVM_VM86) {
        error("vm86 service not available in your kernel, %s\n", strerror(errno));
      }
#ifdef X86_EMULATOR
      config.cpu_vm = CPUVM_EMU;
#else
      exit(1);
#endif
    }
  }
#endif

#ifdef X86_EMULATOR
  if (config.cpu_vm == CPUVM_EMU) {
    if (orig_cpu_vm != CPUVM_EMU) {
      config.cpuemu = 3;
      if (orig_cpu_vm == -1)
	warn("using CPU emulation for vm86()\n");
      else
	error("using CPU emulation for vm86()\n");
    }
    init_emu_cpu();
  }
#endif
}
