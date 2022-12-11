#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H

//#include "ffs.h"
#include <strings.h> // for ffs
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

#endif				/* _ASM_BITOPS_H */
