#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H
/* This is a hacked-up version of Linus' bitops.h file.
   I've added a function and removed the "extern"s (in the i386 part) for 
   convenience.  If I can figure out how to use Linus' bitops.h directly, I'll 
   reduce this file to an #include and just my added function.
   For now, this works. -JLS
   */

/*
 * Copyright 1992, Linus Torvalds.
 */
 
/*
 * find_bit() Copyright 1994, J. Lawrence Stephan.
 */

/* function definitions */

extern int find_bit(void * addr);
extern long atomic_inc(long * addr);
extern long atomic_dec(long * addr);
extern int set_bit(int nr, void * addr);
extern int clear_bit(int nr, void * addr);
extern int test_bit(int nr, void * addr);
extern int pic0_to_emu(char flags);

#if (defined(INCLUDE_INLINE_FUNCS) || !defined(NO_INLINE_FUNCS))
#ifdef INCLUDE_INLINE_FUNCS
#define _INLINE_ extern
#else
#define _INLINE_ extern __inline__
#endif

#ifdef i386
/*
 * These have to be done with inline assembly: that way the bit-setting
 * is guaranteed to be atomic. All bit operations return 0 if the bit
 * was cleared before the operation and != 0 if it was not.
 *
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy { unsigned long a[100]; };
#define ADDR (*(struct __dummy *) addr)

/* JLS's stuff */
/* find_bit returns the bit number of the lowest bit that's set */
/* it doesn't need to be atomic, but it's much faster than using c */

_INLINE_ int find_bit(void * addr)
{
	int bitno;

	__asm__ __volatile__("bsfl %1,%0"
		:"=r" (bitno):"m" (ADDR));
	return bitno;
}
/* atomic increment flag and decrement flag operations */
_INLINE_ long int atomic_inc(long * addr)
{        
        __asm__ __volatile__("incl %1"
                :"=m" (ADDR):"0" (ADDR));
        return *addr;
 }
_INLINE_ long int atomic_dec(long * addr)
{        
        __asm__ __volatile__("decl %1"
                :"=m" (ADDR):"0" (ADDR));
        return *addr;
 }
 
_INLINE_ int pic0_to_emu(char flags)
{
   /* This function maps pic0 bits to their positions in priority order */
   /* It makes room for the pic1 bits in between.  This could be done in c,
      but it would be messier, and might get clobbered by optimization */

   /* move bits xxxx xxxx 7654 3210 to 7654 3ooo oooo 210o             */
   /* where 76543210 are original 8 bits, x = don't care, and o = zero */

        long result;    
        __asm__ __volatile__("movzbl %1,%0\n\t
                              shll $13,%0\n\t
                              sarw $7,%0\n\t
                              shrl $5,%0" 
                :"=r" (result):"r" (flags));
        return result;
 }
_INLINE_ long emu_to_pic0(long flags)
{
   /* This function takes the pic0 bits from the overall pic bit field
      and concatenates them into a single byte.  This could be done in c,
      but it would be messier and might get clobbered by optimization.  */
      
   /* move bits 7654 3xxx xxxx 210x to xxxx xxxx 7654 3210          */
   /* where 76543210 are final 8 bits and x = don't care            */

        __asm__ __volatile__("shll $6,%0\n\t
                              shlw $7,%0\n\t
                              shrl $14,%0" 
                :"=r" (flags):"0" (flags));
        return flags;
 }
                              
/* Linus' stuff follows - except each __inline__ had an extern in front of it*/
_INLINE_ int set_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"r" (nr));
	return oldbit;
}

_INLINE_ int clear_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"r" (nr));
	return oldbit;
}

/*
 * This routine doesn't need to be atomic, but it's faster to code it
 * this way.
 */
_INLINE_ int test_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit)
		:"m" (ADDR),"r" (nr));
	return oldbit;
}

#else
/*
 * For the benefit of those who are trying to port Linux to another
 * architecture, here are some C-language equivalents.  You should
 * recode these in the native assmebly language, if at all possible.
 * To guarantee atomicity, these routines call cli() and sti() to
 * disable interrupts while they operate.  (You have to provide inline
 * routines to cli() and sti().)
 *
 * Also note, these routines assume that you have 32 bit integers.
 * You will have to change this if you are trying to port Linux to the
 * Alpha architecture or to a Cray.  :-)
 *
 * find_bit, atomic_inc, and atomic_dec  equivalents by 
 * J. L. Stephan 3/6/94, other 
 * C language equivalents written by Theodore Ts'o, 9/26/92
 */
 
_INLINE_ int emu_to_pic0(long flags)
{
       return ( ( (flags>>1) & 0x07) | ( (flags>>8) & 0xf8) );
}
_INLINE_ int pic0_to_emu(char flags)
{
       return ( ( (flags&0x07) << 1 ) | ( (flags&0xf8) << 8) );       
  
_INLINE_ find_bit(unsigned long * addr)
{
       int bitno;
       unsigned long bitmask;
       bitno = 0;
       bitmask = 1;
       while( (bitmask & *addr)==0 && bitno < 32)
   {   bitmask += bitmask;
       ++bitno;
    }
/* in i386 asm routine, if no bits are set, result is undefined */
       return bitno;
}   

_INLINE_ atomic_inc(long * addr)
{
 ++(*addr);
}

_INLINE_ atomic_dec(long * addr)
{
 --(*addr);
}
_INLINE_ int set_bit(int nr,int * addr)
{
	int	mask, retval;

	addr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	cli();
	retval = (mask & *addr) != 0;
	*addr |= mask;
	sti();
	return retval;
}

_INLINE_ int clear_bit(int nr, int * addr)
{
	int	mask, retval;

	addr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	cli();
	retval = (mask & *addr) == 0;
	*addr &= ~mask;
	sti();
	return retval;
}

_INLINE_ int test_bit(int nr, int * addr)
{
	int	mask;

	addr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	return ((mask & *addr) != 0);
}
#endif	/* i386 */
#undef _INLINE_
#endif /* ??_INLINE_FUNCS */
#endif /* _ASM_BITOPS_H */
