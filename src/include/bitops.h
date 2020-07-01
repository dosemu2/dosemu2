#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H
/*
 * This is a hacked-up version of Linus' bitops.h file. I've added a
 * function and removed the "extern"s (in the i386 part) for convenience.
 * If I can figure out how to use Linus' bitops.h directly, I'll reduce
 * this file to an #include and just my added function. For now, this
 * works. -JLS $Id$
 *
 */

/*
 * Copyright 1992, Linus Torvalds.
 */

/*
 * find_bit() Copyright 1994, J. Lawrence Stephan.
 */

/* function definitions */


static int find_bit(unsigned int word);
static int find_bit_r(unsigned int word);
static int set_bit(int nr, void * addr);
static int clear_bit(int nr, void * addr);
static int change_bit(int nr, void * addr);
static int test_bit(int nr, void * addr);
static int pic0_to_emu(char flags);

/*
 * These have to be done with inline assembly: that way the bit-setting is
 * guaranteed to be atomic. All bit operations return 0 if the bit was
 * cleared before the operation and != 0 if it was not.
 *
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */

#define ADDR (*(volatile long *) addr)

/* JLS's stuff */
/*
 * find_bit returns the bit number of the lowest bit that's set
 * Returns -1 if no one exists.
 */
static __inline__ int
find_bit(unsigned int word)
{
       int result = -1; /* value to return on error */
       __asm__("bsfl %2,%0"
               :"=r" (result) /* output */
               :"0" (result), "r" (word)); /* input */
       return result;
}

/*
 * find_bit_r returns the bit number of the highest bit that's set
 * Returns -1 if no one exists.
 */
static __inline__ int
find_bit_r(unsigned int word)
{
       int result = -1; /* value to return on error */
       __asm__("bsrl %2,%0"
               :"=r" (result) /* output */
               :"0" (result), "r" (word)); /* input */
       return result;
}

static __inline__ int
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

    int             result;
    __asm__         __volatile__("movzbl %1,%0\n\t"
				 "shll $13, %0\n\t"
				 "sarw $7, %w0\n\t"
				 "shrl $5, %0 " \
				 :"=r"(result):"q"(flags));
    return result;
}

/*
 * Linus' stuff follows - except each __inline__ had an extern in front of
 * it
 */
static __inline__ int
change_bit(int nr, void *addr)
{
    int             oldbit;

    __asm__         __volatile__("btcl %2,%1\n\tsbbl %0,%0"
				 :"=r"(oldbit), "=m"(ADDR)
				 :"r"(nr));
    return oldbit;
}

static __inline__ int
set_bit(int nr, void *addr)
{
    int             oldbit;

    __asm__         __volatile__("btsl %2,%1\n\tsbbl %0,%0"
				 :"=r"(oldbit), "=m"(ADDR)
				 :"r"(nr));
    return oldbit;
}

static __inline__ int
clear_bit(int nr, void *addr)
{
    int             oldbit;

    __asm__         __volatile__("btrl %2,%1\n\tsbbl %0,%0"
				 :"=r"(oldbit), "=m"(ADDR)
				 :"r"(nr));
    return oldbit;
}

/*
 * This routine doesn't need to be atomic, but it's faster to code it this
 * way.
 */
static __inline__ int
test_bit(int nr, void *addr)
{
    int             oldbit;

    __asm__         __volatile__("btl %2,%1\n\tsbbl %0,%0"
				 :"=r"(oldbit)
				 :"m"(ADDR), "r"(nr));
    return oldbit;
}

#endif				/* _ASM_BITOPS_H */
