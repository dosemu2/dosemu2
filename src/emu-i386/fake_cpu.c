/* 
 * DANG_BEGIN_MODULE
 * 
 * Contains the functions to fake the stack push's & pop's
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * $Date: $
 * $Source: $
 * $Revision: $
 * $State: $
 *
 * $Log: $
 *
 * DANG_END_CHANGELOG
 */

#include "emu.h"
#include "config.h"

/*
 * nearly directly stolen from Linus : linux/kernel/vm86.c
 *
 * Boy are these ugly, but we need to do the correct 16-bit arithmetic.
 * Gcc makes a mess of it, so we do it inline and use non-obvious calling
 * conventions..
 */
#define pushb(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"movb %2,(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define pushw(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"movb %h2,(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define pushl(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb %h2,(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb %h2,(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#ifdef 0
/* this is done in include/cpu.h too, but implemented other way  */
#define popb(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb (%1,%0),%b2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })
#endif

#define popw(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb (%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb (%1,%0),%h2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#define popl(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb (%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb (%1,%0),%h2\n\t" \
	"incw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb (%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb (%1,%0),%h2\n\t" \
	"incw %w0\n\t" \
	"rorl $16,%2" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base)); \
__res; })

void _pusha(void)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  pushw(ssp, sp, _AX);
  pushw(ssp, sp, _CX);
  pushw(ssp, sp, _DX);
  pushw(ssp, sp, _BX);
  pushw(ssp, sp, _SP);
  pushw(ssp, sp, _BP);
  pushw(ssp, sp, _SI);
  pushw(ssp, sp, _DI);
  _SP -= 16;
}

void _pushad(void)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  pushl(ssp, sp, _EAX);
  pushl(ssp, sp, _ECX);
  pushl(ssp, sp, _EDX);
  pushl(ssp, sp, _EBX);
  pushl(ssp, sp, _ESP);
  pushl(ssp, sp, _EBP);
  pushl(ssp, sp, _ESI);
  pushl(ssp, sp, _EDI);
  _SP -= 32;
}

void _popa(void)
{
  /* WARNING -- this routine is UNTESTED */
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  _SP += 16;
  _DI = popw(ssp, sp);
  _SI = popw(ssp, sp);
  _BP = popw(ssp, sp);
  _SP = popw(ssp, sp);
  _BX = popw(ssp, sp);
  _DX = popw(ssp, sp);
  _CX = popw(ssp, sp);
  _AX = popw(ssp, sp);
}

void _popad(void)
{
  /* WARNING -- this routine is UNTESTED */
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  _SP += 32;
  _EDI = popl(ssp, sp);
  _ESI = popl(ssp, sp);
  _EBP = popl(ssp, sp);
  _ESP = popl(ssp, sp);
  _EBX = popl(ssp, sp);
  _EDX = popl(ssp, sp);
  _ECX = popl(ssp, sp);
  _EAX = popl(ssp, sp);
}

void _pushf(void)
{
  /* WARNING -- this routine is UNTESTED */
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  pushw(ssp, sp, _EFLAGS);
  _SP -= 2;
}

void _pushfd(void)
{
  /* WARNING -- this routine is UNTESTED */
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  pushl(ssp, sp, _EFLAGS);
  _SP -= 4;
}

void _pushb(Bit8u byte)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  pushb(ssp, sp, byte);
  _SP -= 1;
}

void _pushw(Bit16u word)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  pushw(ssp, sp, word);
  _SP -= 2;
}

void _pushd(Bit32u dword)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  pushl(ssp, sp, dword);
  _SP -= 4;
}

Bit8u _popb(void)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  _SP += 1;
  return popb(ssp, sp);
}

Bit16u _popw(void)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  _SP += 2;
  return popw(ssp, sp);
}

Bit32u _popd(void)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  _SP += 4;
  return popl(ssp, sp);
}

void _callf(Bit16u seg, Bit16u ofs)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  pushw(ssp, sp, _CS);
  pushw(ssp, sp, _IP);
  _SP -= 4;
  _CS = seg;
  _IP = ofs;
}

void _calli(Bit16u seg, Bit16u ofs)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  pushw(ssp, sp, vflags);
  pushw(ssp, sp, _CS);
  pushw(ssp, sp, _IP);
  _SP -= 6;
  _CS = seg;
  _IP = ofs;
}

void _iret(void)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  _SP += 6;
  _IP = popw(ssp, sp);
  _CS = popw(ssp, sp);
  WRITE_FLAGS(popw(ssp, sp));  /* I'm a bit worried about this one... */
}

void _retf(Bit16u pop_count)
{
  Bit8u  *ssp;
  Bit32u  sp;

  ssp = (Bit8u *)(((Bit32u)_SS)<<4);
  sp  = (Bit32u)_SP;

  _IP = popw(ssp, sp);
  _CS = popw(ssp, sp);
  _SP += 4;              /* don't merge these two lines, gcc generates */
  _SP += 2 * pop_count;  /* incorrect assembler code, last I checked   */
}
