/*
 * Declarations shared between the C and the asm halves of the loader.
 * This is the PDHDR that thunk_gen reads, so keep it to declarations.
 *
 * Free software, GPL v2 or later.
 */
#ifndef RUN286_ASM_H
#define RUN286_ASM_H

/* one stub per interrupt vector the program hooks, with a 16bit stack to
 * run its handler on, as a 286 extender would have given it */
#define INT_SLOTS 12
#define INT_SLOT_SIZE 8
/*
 * Each vector gets a piece of stack per interrupt of it in flight, not
 * one piece for the vector: a handler that enables interrupts, as a timer
 * handler that chains to the one before it does, can be interrupted by
 * its own vector again, and by then it has moved to a stack of its own,
 * so there is nothing in SS to say that ours is still in use.
 */
#define INT_FRAME_LEN 0x1000		/* what one handler gets */
#define INT_FRAMES 4			/* how deeply one vector may nest */
#define INT_STACK_LEN (INT_FRAME_LEN * INT_FRAMES)
#define INT_STACK_HDR 0x40		/* the free pointers live here, in
					 * the stack segment, so that the
					 * 16bit return stub can reach them */

/* one stub per processor exception, and a stack to report one on */
#define EXC_SLOTS 0x20
#define EXC_SLOT_SIZE 8
#define EXC_STACK_LEN 0x2000

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

/* Called from _exc_common when the program, or we, take a processor
 * exception. Says what happened and does not come back. */
void ASMCFUNC run286_exception(void);

#else

/* one piece per call in flight: the program's interrupt handlers call
 * imports too, and one can land while another call is still being
 * served */
#define GATE_FRAME_LEN 0x1000
#define GATE_STACK_LEN (GATE_FRAME_LEN * 8)

#endif

#endif
