#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H
/*
 * This is a hacked-up version of Linus' bitops.h file. I've added a
 * function and removed the "extern"s (in the i386 part) for convenience.
 * If I can figure out how to use Linus' bitops.h directly, I'll reduce
 * this file to an #include and just my added function. For now, this
 * works. -JLS $Id: bitops.h,v 1.1 1995/04/08 22:35:19 root Exp $
 * 
 */

/*
 * Copyright 1992, Linus Torvalds.
 */

/*
 * find_bit() Copyright 1994, J. Lawrence Stephan.
 */

/* function definitions */


#ifndef  C_RUN_IRQS
#define _INLINE_ extern __inline__
#define _STATIC_
#else
#define _INLINE_ static __inline__
#define _STATIC_ static
#endif

_STATIC_ int find_bit(void * addr);
_STATIC_ long atomic_inc(long * addr);
_STATIC_ long atomic_dec(long * addr);
_STATIC_ int set_bit(int nr, void * addr);
_STATIC_ int clear_bit(int nr, void * addr);
_STATIC_ int test_bit(int nr, void * addr);
_STATIC_ int pic0_to_emu(char flags);

/*
 * These have to be done with inline assembly: that way the bit-setting is
 * guaranteed to be atomic. All bit operations return 0 if the bit was
 * cleared before the operation and != 0 if it was not.
 * 
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy {
    unsigned long   a[100];
};
#define ADDR (*(struct __dummy *) addr)

/* JLS's stuff */
/* find_bit returns the bit number of the lowest bit that's set */
/* it doesn't need to be atomic, but it's much faster than using c */

_INLINE_ int
find_bit(void *addr)
{
    int             bitno;

    __asm__         __volatile__("bsfl %1,%0"
				 :"=r"           (bitno):"m"(ADDR));
    return bitno;
}
/* atomic increment flag and decrement flag operations */
_INLINE_ long int
atomic_inc(long *addr)
{
    __asm__         __volatile__("incl %1"
				 :"=m"           (ADDR):"0"(ADDR));
    return *addr;
}
_INLINE_ long int
atomic_dec(long *addr)
{
    __asm__         __volatile__("decl %1"
				 :"=m"           (ADDR):"0"(ADDR));
    return *addr;
}

_INLINE_ int
pic0_to_emu(char flags)
{
    /* This function maps pic0 bits to their positions in priority order */
    /*
     * It makes room for the pic1 bits in between.  This could be done in
     * c, but it would be messier, and might get clobbered by optimization
     */

    /* move bits xxxx xxxx 7654 3210 to 7654 3222 2222 210o             */
    /* where 76543210 are original 8 bits, x = don't care, and o = zero */
    /* bit 2 (cascade int) is used to mask/unmask pic1 (Larry)          */

    long            result;
    __asm__         __volatile__("movzbl %1,%0\n\t
				 shll $13, %0\n\t
				 sarw $7, %0\n\t
				 shrl $5, %0 " 
				 :"=r"           (result):"r"(flags));
    return result;
}
_INLINE_ long
emu_to_pic0(long flags)
{
    /*
     * This function takes the pic0 bits from the overall pic bit field
     * and concatenates them into a single byte.  This could be done in c,
     * but it would be messier and might get clobbered by optimization.
     */

    /* move bits 7654 3xxx xxxx 210x to xxxx xxxx 7654 3210          */
    /* where 76543210 are final 8 bits and x = don't care            */

    __asm__         __volatile__("shll $6,%0\n\t
				 shlw $7, %0\n\t
				 shrl $14, %0 " 
				 :"=r"           (flags):"0"(flags));
    return flags;
}

/*
 * Linus' stuff follows - except each __inline__ had an extern in front of
 * it
 */
_INLINE_ int
set_bit(int nr, void *addr)
{
    int             oldbit;

    __asm__         __volatile__("btsl %2,%1\n\tsbbl %0,%0"
				 :"=r"           (oldbit), "=m"(ADDR)
				 :"r"            (nr));
    return oldbit;
}

_INLINE_ int
clear_bit(int nr, void *addr)
{
    int             oldbit;

    __asm__         __volatile__("btrl %2,%1\n\tsbbl %0,%0"
				 :"=r"           (oldbit), "=m"(ADDR)
				 :"r"            (nr));
    return oldbit;
}

/*
 * This routine doesn't need to be atomic, but it's faster to code it this
 * way.
 */
_INLINE_ int
test_bit(int nr, void *addr)
{
    int             oldbit;

    __asm__         __volatile__("btl %2,%1\n\tsbbl %0,%0"
				 :"=r"           (oldbit)
				 :"m"            (ADDR), "r"(nr));
    return oldbit;
}

#endif				/* _ASM_BITOPS_H */
