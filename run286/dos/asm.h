/*
 * Declarations shared between the C and the asm halves of the loader.
 * This is the PDHDR that thunk_gen reads, so keep it to declarations.
 *
 * Free software, GPL v2 or later.
 */
#ifndef RUN286_ASM_H
#define RUN286_ASM_H

#ifndef __ASSEMBLER__
#ifdef DJ64
#include <dj64/asm_inc.h>
#else
#include "asm_inc.h"
#endif

#define ASMFUNC
#define ASMCFUNC

/* Switch to the program's 16bit stack and jump to its entry point. Returns
 * only once the program asks to terminate, with its exit code. */
int ASMFUNC desc_probe(void);
int ASMFUNC ne_enter(int cs, int ip, int ss, int sp, int ds, int es, int ax, int cx);

/* Called from the int 0x66 handler when the program calls an import. The
 * index is in run286_import_idx; a nonzero return unwinds ne_enter(). */
int ASMCFUNC run286_import(void);

#else

#define GATE_STACK_LEN 0x4000

#endif

#endif
