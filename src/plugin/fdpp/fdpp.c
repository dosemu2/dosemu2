#include <assert.h>
#include <stdio.h>
#include <fdpp/thunks.h>
#include "emu.h"
#include "init.h"
#include "utilities.h"

static uintptr_t fdpp_call(uint16_t seg, uint16_t off, uint8_t *sp,
	uint8_t len)
{
    error("Not supported yet\n");
    leavedos(3);
    return 0;
}

static void fdpp_symtab(void *calltab, int clen, void *symtab, int slen)
{
    int err;

    FdppSetAsmCalls(calltab, clen);
    err = FdppSetAsmThunks(symtab, slen);
    assert(!err);
}

static struct dl_ops ops = {
    .set_symtab = fdpp_symtab,
    .ccall = FdppThunkCall,
};

static void fdpp_abort(const char *file, int line)
{
    dosemu_error("fdpp: abort at %s:%i\n", file, line);
    leavedos(3);
}

static void fdpp_print(const char *format, va_list ap)
{
    vprintf(format, ap);
}

static uint8_t *fdpp_mbase(void)
{
    return lowmem_base;
}

static struct fdpp_api api = {
    .mem_base = fdpp_mbase,
    .abort_handler = fdpp_abort,
    .print_handler = fdpp_print,
    .asm_call = fdpp_call,
};

CONSTRUCTOR(static void init(void))
{
    FdppInit(&api);
    register_dl_ops(&ops);
}
