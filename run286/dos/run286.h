/*
 * The API the program sees: PHAPI and the OS/2 1.x DOSCALLS subset that
 * the Phar Lap bound programs actually import.
 *
 * Free software, GPL v2 or later.
 */
#ifndef RUN286_H
#define RUN286_H

#include <stdint.h>

/*
 * One call from the program. gate_entry() has pushed, on the program's own
 * 16bit stack and in this order, PUSHAD, DS, ES; below that sits the iret
 * frame the DPMI host made, then the near return into the stub, then the
 * arguments. c->sp points at the saved ES.
 */
#define CALL_EAX	36		/* the saved EAX, for the result */
#define CALL_ARGS	56		/* the first byte of the arguments */

struct call {
    uint16_t ss;
    uint32_t sp;
};

/* An API entry. args is the number of argument bytes the callee has to pop
 * on the way out, which for the Pascal convention is all of them. */
struct api_fn {
    const char *name;			/* for imports by name */
    uint16_t ord;			/* for imports by ordinal */
    uint16_t args;
    uint16_t (*fn)(struct call *c);
};

const struct api_fn *phapi_lookup(const char *name);
const struct api_fn *doscalls_lookup(uint16_t ord);

uint16_t call_argw(struct call *c, unsigned off);
uint32_t call_argd(struct call *c, unsigned off);
void call_setw(uint32_t fp, uint16_t val);
void call_setd(uint32_t fp, uint32_t val);

#endif
