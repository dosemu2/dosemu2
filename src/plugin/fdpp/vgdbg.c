#include <stdint.h>
#include <valgrind/memcheck.h>
#include <fdpp/memtype.h>
#include "memory.h"
#include "cpu.h"
#include "dosemu_debug.h"
#include "vgdbg.h"

void fdpp_mark_mem(uint16_t seg, uint16_t off, uint16_t size, int type)
{
    void *ptr = MEM_BASE32(SEGOFF2LINEAR(seg, off));
    switch (type) {
    case FD_MEM_READONLY:
	/* oops, no readonly support in valgrind */
	VALGRIND_MAKE_MEM_NOACCESS(ptr, size);
	break;
    case FD_MEM_UNINITIALIZED:
	VALGRIND_MAKE_MEM_UNDEFINED(ptr, size);
	break;
    }
}
