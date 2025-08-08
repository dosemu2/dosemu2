#ifndef DOSEMU_CPUEMU_H
#define DOSEMU_CPUEMU_H

#include <signal.h>
#include "bitops.h"
#include "cpu.h"

extern void e_priv_iopl(int);
#define test_ioperm(a)	(test_bit((a),emu_io_bitmap))

/* ------------------------------------------------------------------------
 * if this is defined, cpuemu will only start when explicitly activated
 * by int0xe6. Technically: if we want to trace DOS boot we undefine this
 * and config.cpuemu will be set at 3 from the start - else it is set at 1
 */
#if 0
#define DONT_START_EMU
#endif

/* define this to skip emulation through video BIOS. This speeds up a lot
 * the cpuemu and avoids possible time-dependent code in the VBIOS. It has
 * a drawback: when we jump into REAL vm86 in the VBIOS, we never know
 * where we will exit back to the cpuemu. Oh, well, speed is what matters...
 */
#if 0
#define	SKIP_EMU_VBIOS
#endif

/* if defined, trace instructions (with debug_level('e')>3) only in protected
 * mode code. This is useful to skip timer interrupts and/or better
 * follow the instruction flow */
#if 0
#define SKIP_VM86_TRACE
#endif

/* If you set this to 1, some I/O instructions will be compiled in,
 * otherwise all I/O will be interpreted.
 * Because of fault overhead, only instructions which access an
 * untrapped port will be allowed to compile. This is not 100% safe
 * since DX can dynamically change.
 */
#if 0
#define CPUEMU_DIRECT_IO
#endif

#if defined(__x86_64__) || defined(__i386__)
#define HOST_ARCH_X86
#endif

#ifdef X86_JIT
#define IS_EMU_JIT() (IS_EMU() && !config.cpusim)
#else
#define IS_EMU_JIT() (0)
#endif

/* ----------------------------------------------------------------------- */

/* Cpuemu status register - pack as much info as possible here, so to
 * use a single test to check if we have to go further or not */
#define CeS_SIGPEND	0x01	/* signal pending mask */
#define CeS_SIGACT	0x02	/* signal active mask */
#define CeS_RPIC	0x04	/* pic asks for interruption */
#define CeS_STI		0x08	/* IF active was popped */
#define CeS_MOVSS	0x10	/* mov ss or pop ss interpreted */
#define CeS_INHI	0x800	/* inhibit interrupts(pop ss; pop sp et sim.) */
#define CeS_TRAP	0x1000	/* INT01 Sstep active */
#define CeS_DRTRAP	0x2000	/* Debug Registers active */
#define CeS_INSTREMU_RM	0x4000
#define CeS_INSTREMU_PM	0x8000
#define CeS_INSTREMU	(CeS_INSTREMU_RM | CeS_INSTREMU_PM) /* behave like former instr_emu, with counter for VGAEMU faults */
#define CeS_PREJIT_RM	0x10000
#define CeS_PREJIT_PM	0x20000

void leave_cpu_emu(void);
void avltr_destroy(void);
int e_vm86(struct vm86_struct *info);

/* called from dpmi.c */
void emu_mhp_SetTypebyte (unsigned short selector, int typebyte);
unsigned short emu_do_LAR (unsigned short selector);
char *e_scp_disasm(cpuctx_t *scp, int pmode);

/* called from mfs.c, fatfs.c and some places that memcpy */
void e_invalidate(unsigned data, int cnt);
void e_invalidate_full(unsigned data, int cnt);
void e_invalidate_full_pa(unsigned data, int cnt);
int e_invalidate_page_full(unsigned data);
void e_invalidate_pa(unsigned data, int cnt);
void e_invalidate_dirty(unsigned int addr, unsigned int aend);
void e_invalidate_page_dirty(unsigned int addr);
void e_invalidate_dirty_full(void);
int e_querymprot(dosaddr_t addr);

/* called from cpu.c */
void init_emu_cpu(int cpu_type);
void reset_emu_cpu (void);

/* called/used from dpmi.c */
int e_dpmi(cpuctx_t *scp);
void e_dpmi_b0x(int op,cpuctx_t *scp);

/* called/used from vgaemu.c */
void instr_emu_sim(cpuctx_t *scp, int pmode);
void instr_emu_sim_reset_count(void);

void cpuemu_enter(int pm);
void cpuemu_leave(int pm);
void cpuemu_update_fpu(void);

/* called from emu-ldt.c */
void InvalidateSegs(void);

#ifdef X86_JIT
/* called from sigsegv.c */
int e_emu_fault(sigcontext_t *scp, int in_vm86);
#else
#define e_emu_fault(scp, in_vm86) 0
#endif

#ifdef X86_EMULATOR
/* called from signal.c */
int e_in_compiled_code(void);
void e_gen_sigalrm(void);
void e_gen_sigalrm_from_thread(void);
int EMU_V86(void);
int EMU_DPMI(void);
int _CPU_VM(void);
int _CPU_VM_DPMI(void);
#else
#define e_gen_sigalrm()
#define e_gen_sigalrm_from_thread()
#define e_in_compiled_code() 0
#define EMU_V86() 0
#define EMU_DPMI() 0
#define _CPU_VM() config.cpu_vm
#define _CPU_VM_DPMI() config.cpu_vm_dpmi
#endif

/* called from dos2linux.c */

int in_emu_cpu(void);

#endif	/*DOSEMU_CPUEMU_H*/
