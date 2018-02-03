#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fdpp/thunks.h>
#include "emu.h"
#include "init.h"
#include "utilities.h"

static uintptr_t fdpp_call(uint16_t seg, uint16_t off, uint8_t *sp,
	uint8_t len)
{
    uint8_t *stk;

    LWORD(esp) -= len;
    stk = SEG_ADR((uint8_t *), ss, sp);
    memcpy(stk, sp, len);
    do_call_back(seg, off);
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

static void fdpp_sti(void)
{
    set_IF();
}

static void fdpp_cli(void)
{
    clear_IF();
}

static uint16_t fdpp_cs(void)
{
    return SREG(cs);
}

static void fdpp_set_ds(uint16_t ds)
{
    SREG(ds) = ds;
}

static void fdpp_set_es(uint16_t es)
{
    SREG(es) = es;
}

static struct fdpp_api api = {
    .mem_base = fdpp_mbase,
    .abort_handler = fdpp_abort,
    .print_handler = fdpp_print,
    .asm_call = fdpp_call,
    .thunks = {
        .enable = fdpp_sti,
        .disable = fdpp_cli,
        .getCS = fdpp_cs,
        .setDS = fdpp_set_ds,
        .setES = fdpp_set_es,
    },
};

CONSTRUCTOR(static void init(void))
{
    FdppInit(&api);
    register_dl_ops(&ops);
}
