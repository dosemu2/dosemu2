/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ASM_GENERIC_BITOPS_GENERIC_NON_ATOMIC_H
#define __ASM_GENERIC_BITOPS_GENERIC_NON_ATOMIC_H

#include <stdbool.h>

#ifdef __x86_64__
#define BITS_PER_LONG 64
#else
#define BITS_PER_LONG 32
#endif
#define BIT_ULL(nr)		(ULL(1) << (nr))
#define BIT_MASK(nr)		(UL(1) << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BIT_ULL_MASK(nr)	(ULL(1) << ((nr) % BITS_PER_LONG_LONG))
#define BIT_ULL_WORD(nr)	((nr) / BITS_PER_LONG_LONG)
#define BITS_PER_BYTE		8
#define UL(x) ((unsigned long)(x))

/*
 * Generic definitions for bit operations, should not be used in regular code
 * directly.
 */

/**
 * generic___set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __always_inline void
generic___set_bit(unsigned long nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p  |= mask;
}

static __always_inline void
generic___clear_bit(unsigned long nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p &= ~mask;
}

/**
 * generic___change_bit - Toggle a bit in memory
 * @nr: the bit to change
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __always_inline void
generic___change_bit(unsigned long nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p ^= mask;
}

/**
 * generic___test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __always_inline bool
generic___test_and_set_bit(unsigned long nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old | mask;
	return (old & mask) != 0;
}

/**
 * generic___test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __always_inline bool
generic___test_and_clear_bit(unsigned long nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old & ~mask;
	return (old & mask) != 0;
}

/* WARNING: non atomic and it can be reordered! */
static __always_inline bool
generic___test_and_change_bit(unsigned long nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old ^ mask;
	return (old & mask) != 0;
}

/**
 * generic_test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 */
static __always_inline bool
generic_test_bit(unsigned long nr, const unsigned long *addr)
{
	/*
	 * Unlike the bitops with the '__' prefix above, this one *is* atomic,
	 * so `volatile` must always stay here with no cast-aways. See
	 * `Documentation/atomic_bitops.txt` for the details.
	 */
	return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
}

/*
 * const_*() definitions provide good compile-time optimizations when
 * the passed arguments can be resolved at compile time.
 */
#define const___set_bit			generic___set_bit
#define const___clear_bit		generic___clear_bit
#define const___change_bit		generic___change_bit
#define const___test_and_set_bit	generic___test_and_set_bit
#define const___test_and_clear_bit	generic___test_and_clear_bit
#define const___test_and_change_bit	generic___test_and_change_bit
#define const_test_bit_acquire		generic_test_bit_acquire

/**
 * const_test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 *
 * A version of generic_test_bit() which discards the `volatile` qualifier to
 * allow a compiler to optimize code harder. Non-atomic and to be called only
 * for testing compile-time constants, e.g. by the corresponding macros, not
 * directly from "regular" code.
 */
static __always_inline bool
const_test_bit(unsigned long nr, const unsigned long *addr)
{
	const unsigned long *p = (const unsigned long *)addr + BIT_WORD(nr);
	unsigned long mask = BIT_MASK(nr);
	unsigned long val = *p;

	return !!(val & mask);
}

#endif /* __ASM_GENERIC_BITOPS_GENERIC_NON_ATOMIC_H */
