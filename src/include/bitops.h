#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H

//#include "ffs.h"
#include <strings.h> // for ffs
#include <stdint.h>
#include "fls.h"
#include "generic-non-atomic.h"

#define find_bit(w) (ffs(w) - 1)
#define find_bit_r(w) (fls(w) - 1)
#define set_bit(nr, addr) generic___set_bit(nr, (unsigned long *)(addr))
#define clear_bit(nr, addr) generic___clear_bit(nr, (unsigned long *)(addr))
#define change_bit(nr, addr) generic___change_bit(nr, (unsigned long *)(addr))
#define test_bit(nr, addr) generic_test_bit(nr, (const unsigned long *)(addr))
#define test_and_set_bit(nr, addr) generic___test_and_set_bit(nr, (unsigned long *)(addr))
#define test_and_clear_bit(nr, addr) generic___test_and_clear_bit(nr, (unsigned long *)(addr))
#define test_and_change_bit(nr, addr) generic___test_and_change_bit(nr, (unsigned long *)(addr))

#ifdef __ILP64__
#define BITS_PER_INT 64
#else
#define BITS_PER_INT 32
#endif

static inline unsigned int find_bit64(uint64_t word)
{
	if (!word)
		return -1;
#if BITS_PER_INT == 32
	if (((uint32_t)word) == 0UL)
		return find_bit((uint32_t)(word >> 32)) + 32;
#elif BITS_PER_INT != 64
#error BITS_PER_INT not 32 or 64
#endif
	return find_bit((unsigned int)word);
}

static inline unsigned int find_bit64_from(uint64_t word, int idx)
{
	uint64_t w;

	if (idx >= 64)
		return -1;
	w = word >> idx;
	if (!w)
		return -1;
	return find_bit64(w) + idx;
}

#endif				/* _ASM_BITOPS_H */
