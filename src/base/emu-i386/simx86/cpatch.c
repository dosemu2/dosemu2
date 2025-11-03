/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
 *
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originally was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#include "msdoshlp.h"
#include "emu86.h"
#include "trees.h"
#include "codegen-arch.h"
#include "cpatch.h"
#include "vgaemu.h"

#define UNCPATCH_WRITES 0

#if PROFILE
int CpatchTotal;
int UncpatchTotal;
int CpatchReps;
int CpatchWrites;
int CpatchStkWrites;
int CpatchInvalidates;
#endif

/*
 * Return address of the stub function is passed into eip
 */
void m_munprotect(unsigned int addr, unsigned int len, unsigned char *eip)
{
	/* Shut down prejitter before invalidating, or it may crash.
	 * Also since mprot API currently lacks mutex, shut down prejitter
	 * before querying the code marks. */
	prejit_sync();
	if (debug_level('e')>1) {
		if (debug_level('e')>3)
			e_printf("\tM_MUNPROT %08x:%p\n", addr,eip);
		if (e_querymark(addr, len))
			e_printf("CODE %08x hit in DATA %p patch\n",addr,eip);
	}
	/* if only data in aliased low memory is hit, nothing to do */
	if (!e_querymark(addr, len))
		return;
	// no need to invalidate the whole page here,
	// as the page does not need to be unprotected
	InvalidateNodeRange(addr, len, eip);
#if PROFILE
	CpatchInvalidates++;
#endif
}

#define repmovs(std,letter,cld)			       \
	asm volatile(#std" ; rep ; movs"#letter ";" #cld"\n\t" \
		     : "=&c" (ecx), "=&D" (edi), "=&S" (esi)   \
		     : "0" (ecx), "1" (edi), "2" (esi) \
		     : "memory")

#define repstos(std,letter,cld)			       \
	asm volatile(#std" ; rep ; stos"#letter ";" #cld"\n\t" \
		     : "=&c" (ecx), "=&D" (edi) \
		     : "a" (eax), "0" (ecx), "1" (edi) \
		     : "memory")

struct rep_stack {
	unsigned char *esi, *edi;
	unsigned long ecx, eflags, edx, eax;
	unsigned char *eip;
	unsigned long cpatch_op;
} __attribute__((packed));


#define CMPFLAGS(asmcon,eflags,op1,op2) \
	asm volatile("cmp %1, %2; pushf; pop %0\n\t" \
		     : "=g" (eflags) : #asmcon (op1), #asmcon (op2))

#define SCASLOOP(bitsize,asmcon,eflags,addr,eax,df,ecx,repcond) \
	do { \
		CMPFLAGS(asmcon,eflags,read_##bitsize(addr),(uint##bitsize##_t)eax); \
		addr += df; \
	} while (--ecx && (eflags & X86_EFLAGS_ZF)==repcond)

#define CMPSLOOP(bitsize,asmcon,eflags,addr,source,df,ecx,repcond) \
	do { \
		CMPFLAGS(asmcon,eflags,read_##bitsize(addr),read_##bitsize(source)); \
		addr += df; source += df; \
	} while (--ecx && (eflags & X86_EFLAGS_ZF)==repcond)

// the rep stub gets passed an argument on the stack:
// the original op | 0x10 for operand override, | 0x40 for REPNE
void rep_movs_stos(struct rep_stack *stack)
{
	unsigned char *paddr = stack->edi;
	unsigned int ecx = stack->ecx;
	unsigned char *eip = GetGenCodeBuf(stack->eip);
	dosaddr_t addr;
	unsigned int len = ecx;
	unsigned char *edi;
	unsigned char op;
	unsigned int size;

	assert(InCompiledCode);
	InCompiledCode--;
	addr = EMUADDR_REL(paddr);
	op = stack->cpatch_op;
	size = 1;
	if (op & 0x10) {
		size = 2;
		op &= ~0x10;
	}
	else if (op & 1)
		size = 4;
	len *= size;
	m_munprotect(addr - ((EFLAGS & EFLAGS_DF) ? (len - size) : 0),
		     len, eip);
	edi = LINEAR2UNIX(addr);
	if ((op & 0xfe) == 0xa4) { /* movs */
		dosaddr_t source = EMUADDR_REL(stack->esi);
		unsigned char *esi;
		unsigned int v = vga_access(source, addr);
		esi = LINEAR2UNIX(source);
		if (v) {
			int df = ((EFLAGS & EFLAGS_DF) ? -size:size);
			e_VgaMovs(addr, source, ecx, df, v);
			ecx = 0;
		}
		else if (ecx == len) {
			if (EFLAGS & EFLAGS_DF) repmovs(std,b,cld);
			else repmovs(,b,);
		}
		else if (ecx*2 == len) {
			if (EFLAGS & EFLAGS_DF) repmovs(std,w,cld);
			else repmovs(,w,);
		}
		else {
			if (EFLAGS & EFLAGS_DF) repmovs(std,l,cld);
			else repmovs(,l,);
		}
		if (EFLAGS & EFLAGS_DF) {source -= len; addr -= len;}
		else {source += len; addr += len;}
		stack->esi = EMU_BASE32(source);
	}
	else if ((op & 0xfe) == 0xaa) { /* stos */
		unsigned int eax = stack->eax;
		if (ecx == len) {
			if (vga_write_access(addr)) {
				if (EFLAGS & EFLAGS_DF)
					vga_memset(addr - len + 1, eax, ecx);
				else
					vga_memset(addr, eax, ecx);
				ecx = 0;
			}
			else if (EFLAGS & EFLAGS_DF) repstos(std,b,cld);
			else repstos(,b,);
		}
		else if (ecx*2 == len) {
			if (vga_write_access(addr)) {
				if (EFLAGS & EFLAGS_DF)
					vga_memsetw(addr - len + 2, eax, ecx);
				else
					vga_memsetw(addr, eax, ecx);
				ecx = 0;
			}
			else if (EFLAGS & EFLAGS_DF) repstos(std,w,cld);
			else repstos(,w,);
		}
		else {
			if (vga_write_access(addr)) {
				if (EFLAGS & EFLAGS_DF)
					vga_memsetl(addr - len + 4, eax, ecx);
				else
					vga_memsetl(addr, eax, ecx);
				ecx = 0;
			}
			else if (EFLAGS & EFLAGS_DF) repstos(std,l,cld);
			else repstos(,l,);
		}
		if (EFLAGS & EFLAGS_DF) addr -= len;
		else addr += len;
	}
	else if ((op & 0xb6) == 0xa6 && ecx > 0) { /* cmps/scas */
		int df = size * (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		unsigned long repcond = (op & 0x40) ? 0 : X86_EFLAGS_ZF;
		unsigned long eflags;
		if ((op & 0xbe) == 0xa6) { /* cmps */
			dosaddr_t source = EMUADDR_REL(stack->esi);
			if (size == 1)
				CMPSLOOP(8,q,eflags,addr,source,df,ecx,repcond);
			else if (size == 2)
				CMPSLOOP(16,r,eflags,addr,source,df,ecx,repcond);
			else
				CMPSLOOP(32,r,eflags,addr,source,df,ecx,repcond);
			stack->esi = EMU_BASE32(source);
		}
		else { /* scas */
			unsigned int eax = stack->eax;
			if (size == 1)
				SCASLOOP(8,q,eflags,addr,eax,df,ecx,repcond);
			else if (size == 2)
				SCASLOOP(16,r,eflags,addr,eax,df,ecx,repcond);
			else
				SCASLOOP(32,r,eflags,addr,eax,df,ecx,repcond);
		}
		stack->eflags = (stack->eflags & ~EFLAGS_CC) | (eflags & EFLAGS_CC);
	}
	stack->edi = EMU_BASE32(addr);
	stack->ecx = ecx;
	InCompiledCode++;
#if PROFILE
	CpatchReps++;
#endif
}

/* ======================================================================= */

void stk_16(dosaddr_t addr, Bit16u value)
{
	e_invalidate(addr, 2);
	WRITE_WORD(addr, value);
#if PROFILE
	CpatchStkWrites++;
#endif
}

void stk_32(dosaddr_t addr, Bit32u value)
{
	e_invalidate(addr, 4);
	WRITE_DWORD(addr, value);
#if PROFILE
	CpatchStkWrites++;
#endif
}

static void wri8_slow(dosaddr_t addr, Bit8u value, unsigned char *eip)
{
	if (e_querymark(addr, 1)) {
		InvalidateNodeRange(addr, 1, eip);
#if PROFILE
		CpatchInvalidates++;
#endif
	}
	WRITE_BYTE(addr, value);
}

static void wri16_slow(dosaddr_t addr, Bit16u value, unsigned char *eip)
{
	if (e_querymark(addr, 2)) {
		InvalidateNodeRange(addr, 2, eip);
#if PROFILE
		CpatchInvalidates++;
#endif
	}
	WRITE_WORD(addr, value);
}

static void wri32_slow(dosaddr_t addr, Bit32u value, unsigned char *eip)
{
	if (e_querymark(addr, 4)) {
		InvalidateNodeRange(addr, 4, eip);
#if PROFILE
		CpatchInvalidates++;
#endif
	}
	WRITE_DWORD(addr, value);
}

#if UNCPATCH_WRITES
static void UnCpatch_wri8(unsigned char *eip)
{
    unsigned char *p = eip - 3;
#if PROFILE
    UncpatchTotal++;
#endif
    *((short *)p) = 0x0488; p[2] = 0x2f;
}

static void UnCpatch_wri16(unsigned char *eip)
{
    unsigned char *p = eip - 4;
#if PROFILE
    UncpatchTotal++;
#endif
    *p++ = 0x66; *((short *)p) = 0x0489; p[2] = 0x2f;
}

static void UnCpatch_wri32(unsigned char *eip)
{
    unsigned char *p = eip - 3;
#if PROFILE
    UncpatchTotal++;
#endif
    *((short *)p) = 0x0489; p[2] = 0x2f;
}
#endif

void wri_8(dosaddr_t addr, Bit8u value, unsigned char *rip)
{
	unsigned char *eip = GetGenCodeBuf(rip);
#if PROFILE
	CpatchWrites++;
#endif
	if (e_querymprot(addr)) {
		wri8_slow(addr, value, eip);
		return;
	}
	if (vga_write_access(addr)) {
		vga_write(addr, value);
		return;
	}
	if (__builtin_expect(msdos_ldt_access(addr), 0)) {
		emu_ldt_write(addr, value, 1);
		return;
	}
	if (__builtin_expect(memcheck_is_rom(addr), 0))
		return;
	/* bypass aliasmap lookup, as we know addr is unprotected */
	InCompiledCode--;
#if UNCPATCH_WRITES
	UnCpatch_wri8(eip);
#endif
	UNIX_WRITE_BYTE(EMU_BASE32(addr), value);
	InCompiledCode++;
}

void wri_16(dosaddr_t addr, Bit16u value, unsigned char *rip)
{
	unsigned char *eip = GetGenCodeBuf(rip);
#if PROFILE
	CpatchWrites++;
#endif
	if (e_querymprotrange(addr, 2)) {
		wri16_slow(addr, value, eip);
		return;
	}
	if (vga_write_access(addr)) {
		vga_write_word(addr, value);
		return;
	}
	if (__builtin_expect(msdos_ldt_access(addr), 0)) {
		emu_ldt_write(addr, value, 2);
		return;
	}
	if (__builtin_expect(memcheck_is_rom(addr), 0))
		return;
	/* bypass aliasmap lookup, as we know addr is unprotected */
	InCompiledCode--;
#if UNCPATCH_WRITES
	UnCpatch_wri16(eip);
#endif
	UNIX_WRITE_WORD(EMU_BASE32(addr), value);
	InCompiledCode++;
}

void wri_32(dosaddr_t addr, Bit32u value, unsigned char *rip)
{
	unsigned char *eip = GetGenCodeBuf(rip);
#if PROFILE
	CpatchWrites++;
#endif
	if (e_querymprotrange(addr, 4)) {
		wri32_slow(addr, value, eip);
		return;
	}
	if (vga_write_access(addr)) {
		vga_write_dword(addr, value);
		return;
	}
	if (__builtin_expect(msdos_ldt_access(addr), 0)) {
		emu_ldt_write(addr, value, 4);
		return;
	}
	if (__builtin_expect(memcheck_is_rom(addr), 0))
		return;
	/* bypass aliasmap lookup, as we know addr is unprotected */
	InCompiledCode--;
#if UNCPATCH_WRITES
	UnCpatch_wri32(eip);
#endif
	UNIX_WRITE_DWORD(EMU_BASE32(addr), value);
	InCompiledCode++;
}

Bit8u read_8(dosaddr_t addr)
{
	return vga_read_access(addr) ? vga_read(addr) : READ_BYTE(addr);
}

Bit16u read_16(dosaddr_t addr)
{
	return vga_read_access(addr) ? vga_read_word(addr) : READ_WORD(addr);
}

Bit32u read_32(dosaddr_t addr)
{
	return vga_read_access(addr) ? vga_read_dword(addr) : READ_DWORD(addr);
}

#ifdef __i386__

/*
 * stack on entry:
 *	esp+00	return address
 */

#define STUB_STK(cfunc) \
"		pushal\n \
		pushl	%eax\n \
		pushl	%edx\n \
		call	"#cfunc"\n \
		addl	$8,%esp\n \
		popal\n \
		ret\n"


#define STUB_WRI(cfunc) \
"		pushl	(%esp)\n"	/* return addr = patch point+3 */ \
"		pushl	%eax\n"		/* value to write */ \
"		pushl	%edi\n"		/* addr where to write */ \
"		call	"#cfunc"\n" \
"		popl	%edi\n"		/* restore addr */ \
"		popl	%eax\n"		/* restore eax */ \
"		addl	$4,%esp\n"	/* remove parameters */ \
"		ret\n"

#define STUB_READ(cfunc) \
"		pushl	%edi\n"		/* addr where to read */ \
"		call	"#cfunc"\n" \
"		addl	$4,%esp\n"	/* remove parameters */ \
"		ret\n"

asm (
".text\n.globl stub_rep__\n"
"stub_rep__:	jecxz	1f\n"		/* zero move, nothing to do */
"		pushl	%eax\n"		/* save regs */
"		pushl	%edx\n"		/* edx used in 16bit overrun emulation, save too */
"		pushfl\n"		/* push flags for DF */
"		pushl	%ecx\n"		/* push count */
"		pushl	%edi\n"		/* push dest address */
"		pushl	%esi\n"		/* push source address */
"		pushl	%esp\n"		/* push stack */
"		cld\n"
"		call	rep_movs_stos\n"
"		addl	$4,%esp\n"	/* remove stack parameter */
"		popl	%esi\n"		/* obtain changed source address */
"		popl	%edi\n"		/* obtain changed dest address */
"		popl	%ecx\n"		/* obtain changed count */
"		popfl\n"		/* real CPU flags back */
"		popl	%edx\n"
"		popl	%eax\n"
"1:		ret	$4\n"
);

/* ======================================================================= */

#else //__x86_64__

#define STUB_STK(cfunc) \
"		pushq	%rax\n"		/* save regs */ \
"		pushq	%rcx\n" \
"		pushq	%rdx\n" \
"		pushq	%rdi\n" \
"		pushq	%rsi\n" \
"		movl	%edx,%edi\n" \
"		movl	%eax,%esi\n" \
					/* pass base address in %rdi */ \
"		call	"#cfunc"\n" \
"		popq	%rsi\n"		/* restore regs */ \
"		popq	%rdi\n" \
"		popq	%rdx\n" \
"		popq	%rcx\n" \
"		popq	%rax\n"	\
"		ret\n"


#define STUB_WRI(cfunc) \
"		movq	(%rsp),%rdx\n"	/* return addr = patch point+3 */ \
"		pushq	%rdi\n"		/* save regs */ \
"		pushq	%rsi\n"	\
"		pushq	%rax\n"	\
"		movl	%eax,%esi\n"	/* value to write */ \
					/* pass addr where to write in %rdi */\
"		call	"#cfunc"\n" \
"		popq	%rax\n"		/* restore regs */ \
"		popq	%rsi\n" \
"		popq	%rdi\n"	\
"		ret\n"

#define STUB_READ(cfunc) \
"		pushq	%rdi\n"		/* save regs */ \
"		call	"#cfunc"\n" \
"		popq	%rdi\n"		/* restore regs */ \
"		ret\n"

// Note: 6 pushes + the argument push and call means stack stays 16-bit aligned
asm (
".text\n.globl stub_rep__\n"
"stub_rep__:	jrcxz	1f\n"		/* zero move, nothing to do */
"		pushq	%rax\n"		/* save regs */
"		pushq	%rdx\n"
"		pushfq\n"		/* push flags for DF */
"		pushq	%rcx\n"
"		pushq	%rdi\n"
"		pushq	%rsi\n"
"		movq	%rsp,%rdi\n"	/* pass stack address in %rdi */
"		cld\n"
"		call	rep_movs_stos\n"
"		popq	%rsi\n"		/* restore regs */
"		popq	%rdi\n"
"		popq	%rcx\n"
"		popfq\n"		/* real CPU flags back */
"		popq	%rdx\n"
"		popq	%rax\n"
"1:		ret	$8\n"
);

#endif

asm (
		".text\n"
"stub_stk_16__:.globl stub_stk_16__\n"STUB_STK(stk_16)
"stub_stk_32__:.globl stub_stk_32__\n"STUB_STK(stk_32)
"stub_wri_8__: .globl stub_wri_8__\n "STUB_WRI(wri_8)
"stub_wri_16__:.globl stub_wri_16__\n"STUB_WRI(wri_16)
"stub_wri_32__:.globl stub_wri_32__\n"STUB_WRI(wri_32)
"stub_read_8__: .globl stub_read_8__\n "STUB_READ(read_8)
"stub_read_16__:.globl stub_read_16__\n"STUB_READ(read_16)
"stub_read_32__:.globl stub_read_32__\n"STUB_READ(read_32)
);

enum {
	STUB_REP,
	STUB_STK_16,
	STUB_STK_32,
	STUB_WRI_8,
	STUB_WRI_16,
	STUB_WRI_32,
	STUB_READ_8,
	STUB_READ_16,
	STUB_READ_32,
	STUB_SETSEGPROT,
	STUB_SIMHELPER,
	STUBS_LEN
};
static_assert(STUBS_LEN <= STUBS_LEN_MAX);

// using negative byte offsets
#define Ofs_stub(x) (unsigned char)(((x) - STUBS_LEN_MAX) * \
				    sizeof(TheCPU_struct.stub_func[STUB_REP]))
#define Ofs_stub_rep Ofs_stub(STUB_REP)
#define Ofs_stub_wri_8 Ofs_stub(STUB_WRI_8)
#define Ofs_stub_wri_16 Ofs_stub(STUB_WRI_16)
#define Ofs_stub_wri_32 Ofs_stub(STUB_WRI_32)
#define Ofs_stub_stk_16 Ofs_stub(STUB_STK_16)
#define Ofs_stub_stk_32 Ofs_stub(STUB_STK_32)
#define Ofs_stub_read_8 Ofs_stub(STUB_READ_8)
#define Ofs_stub_read_16 Ofs_stub(STUB_READ_16)
#define Ofs_stub_read_32 Ofs_stub(STUB_READ_32)
typedef void (*stubfunc_t)(void);

int Ofs_SetSegProt(void)
{
	return Ofs_stub(STUB_SETSEGPROT);
}

int Ofs_SimHelper(void)
{
	return Ofs_stub(STUB_SIMHELPER);
}

/* call N(%ebx) */
#define JSRPATCH(p,N) *((short *)(p))=0x53ff;p[2]=N;

/*
 * enters here only from a fault
 */
int Cpatch(sigcontext_t *scp)
{
    unsigned char *p;
    int w16;
    unsigned int v;
    unsigned char *eip = (unsigned char *)_scp_rip;

#if PROFILE
    CpatchTotal++;
#endif
    p = GetGenCodeBuf(eip);
    if ((*p==0xf2 || *p==0xf3) && (p[1] == 0x66 || p[2] == 0x90) &&
	p[3] == 0x90 && p[4] == 0x90) {
	unsigned char op;

	// rep movs, rep stos, rep lods, rep scas, rep cmps
	// we have a sequence:	f2/f3 op 90 90 90
	//		or	f2/f3 66 op 90 90 (f2 for cmps/scas only)
	if (debug_level('e')>1) e_printf("### REP patch at %p\n",p);
	op = p[1];
	/* as all ops are between 0xa4 and 0xaf we can encode override
	   prefix as 0x10 and repne as 0x40 */
	if (op == OPERoverride)
	    op = p[2] | 0x10;
	if (p[0] == REPNE)
	    op |= 0x40;
	G2M(PUSHbi,op,p); /* push $op; call ofs(%ebx) */
	JSRPATCH(p,Ofs_stub_rep);
	return 1;
    }

    if (*p==0x66) w16=1,p++; else w16=0;
    v = *((int *)p) & 0xffffff;
    while (v==0x2a0489) {		// stack: never fail
	// mov %%{e}ax,(%%edx,%%ebp,1)
	// we have a sequence:	66 89 04 2a
	//		or	89 04 2a
	if (debug_level('e')>1) e_printf("### Stack patch at %p\n",p);
	if (w16) {
	    p[-1] = 0x90; JSRPATCH(p,Ofs_stub_stk_16);
	}
	else {
	    JSRPATCH(p,Ofs_stub_stk_32);
	}
	p+=3;
	/* check for optimized multiple register push */
#ifdef KEEP_ESP
	if (p[10]==0x89) return 1; //O_PUSH3
#else
	if (p[0]==0x89) return 1; //O_PUSH3
#endif
	p += 13;
	if (p[0]==0xff) return 1; // already JSRPATCH'ed
	if (*p==0x66) w16=1,p++; else w16=0;
	v = *((int *)p) & 0xffffff;
	/* extra check: should not fail */
	if (v!=0x2a0489) {
	    dbug_printf("CPUEMU: stack patch failure, fix source code! %x\n", v);
	    return 1;
	}
    }
    if (v==0x2f0488) {		// movb %%al,(%%edi,%%ebp,1)
	// we have a sequence:	88 04 2f
	if (debug_level('e')>1) e_printf("### Byte write patch at %p\n",p);
	JSRPATCH(p,Ofs_stub_wri_8);
	return 1;
    }
    if (v==0x2f0489) {		// mov %%{e}ax,(%%edi,%%ebp,1)
	// we have a sequence:	89 04 2f
	//		or	66 89 04 2f
	if (debug_level('e')>1) e_printf("### Word/Long write patch at %p\n",p);
	if (w16) {
	    p[-1] = 0x90; JSRPATCH(p,Ofs_stub_wri_16);;
	}
	else {
	    JSRPATCH(p,Ofs_stub_wri_32);
	}
	return 1;
    }
    if (v==0x2f048a) {		// movb (%%edi,%%ebp,1),%%al
	// we have a sequence:	8a 04 2f 90 90 90
	if (debug_level('e')>1) e_printf("### Byte read patch at %p\n",p);
	JSRPATCH(p,Ofs_stub_read_8);
	return 1;
    }
    if (v==0x2f048b) {		// mov (%%edi,%%ebp,1),%%{e}ax
	// we have a sequence:	8b 04 2f
	//		or	66 8b 04 2f
	if (debug_level('e')>1) e_printf("### Word/Long read patch at %p\n",p);
	if (w16) {
	    p[-1] = 0x90; JSRPATCH(p,Ofs_stub_read_16);
	}
	else {
	    JSRPATCH(p,Ofs_stub_read_32);
	}
	return 1;
    }
    if (debug_level('e')>1) e_printf("### Patch unimplemented: %08x\n",*((int *)p));
    return 0;
}

int UnCpatch(unsigned char *eip)
{
    unsigned char *p;
    p = GetGenCodeBuf(eip);

    if (*p != 0xff) return 1;
    if (debug_level('e')) {
	e_printf("UnCpatch   at %p was %02x%02x%02x%02x%02x\n",p,
	    p[0],p[1],p[2],p[3],p[4]);
    }
#if PROFILE
    UncpatchTotal++;
#endif
    if (p[1] == 0x53) {
	if ((unsigned char)p[2] == Ofs_stub_rep) {
	    unsigned char op = p[-1];
	    p -= 2;
	    G1((op&0x40)?REPNE:REP,p);
	    if (op & 0x10) {
		G2M(OPERoverride,op&~0x50,p);
	    }
	    else {
		G2M(op&~0x50,NOP,p);
	    }
	    G2M(NOP,NOP,p);
	}
	else if ((unsigned char)p[2] == Ofs_stub_wri_8) {
	    *((short *)p) = 0x0488; p[2] = 0x2f;
	}
	else if ((unsigned char)p[2] == Ofs_stub_wri_16) {
	    *p++ = 0x66; *((short *)p) = 0x0489; p[2] = 0x2f;
	}
	else if ((unsigned char)p[2] == Ofs_stub_wri_32) {
	    *((short *)p) = 0x0489; p[2] = 0x2f;
	}
	else if ((unsigned char)p[2] == Ofs_stub_stk_16) {
	    *((int *)p) = 0x2a048966;
	}
	else if ((unsigned char)p[2] == Ofs_stub_stk_32) {
	    *((short *)p) = 0x0489; p[2] = 0x2a;
	}
	else return 1;
    }
    else return 1;
    if (debug_level('e')) {
	e_printf("UnCpatched at %p  is %02x%02x%02x%02x%02x\n",p,
	    p[0],p[1],p[2],p[3],p[4]);
    }
    return 0;
}

void stub_rep(void) asm ("stub_rep__");
void stub_stk_16(void) asm ("stub_stk_16__");
void stub_stk_32(void) asm ("stub_stk_32__");
void stub_wri_8 (void) asm ("stub_wri_8__" );
void stub_wri_16(void) asm ("stub_wri_16__");
void stub_wri_32(void) asm ("stub_wri_32__");
void stub_read_8 (void) asm ("stub_read_8__" );
void stub_read_16(void) asm ("stub_read_16__");
void stub_read_32(void) asm ("stub_read_32__");

static unsigned int Sim_helper_jit(unsigned int mem_ref, unsigned int data,
				   int mode, uint32_t *flags, unsigned int opc,
				   unsigned int arg, unsigned char *rip)
{
    unsigned int ret;

    InCompiledCode--;
    ret = Sim_helper(mem_ref, data, mode, flags, opc, arg, GetGenCodeBuf(rip));
    InCompiledCode++;
    return ret;
}

void Cpatch_init(void)
{
    TheCPU_struct.stub_func[STUB_REP] = stub_rep;
    TheCPU_struct.stub_func[STUB_WRI_8] = stub_wri_8;
    TheCPU_struct.stub_func[STUB_WRI_16] = stub_wri_16;
    TheCPU_struct.stub_func[STUB_WRI_32] = stub_wri_32;
    TheCPU_struct.stub_func[STUB_STK_16] = stub_stk_16;
    TheCPU_struct.stub_func[STUB_STK_32] = stub_stk_32;
    TheCPU_struct.stub_func[STUB_READ_8] = stub_read_8;
    TheCPU_struct.stub_func[STUB_READ_16] = stub_read_16;
    TheCPU_struct.stub_func[STUB_READ_32] = stub_read_32;
    TheCPU_struct.stub_func[STUB_SETSEGPROT] = (stubfunc_t)SetSegProt_helper;
    TheCPU_struct.stub_func[STUB_SIMHELPER] = (stubfunc_t)Sim_helper_jit;
}

/* ======================================================================= */
