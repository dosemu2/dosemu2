/*
 * PHAPI on top of DPMI.
 *
 * Every function here is entered from a 16bit stub through gate_entry(),
 * with the program's arguments still on its own stack. The Pascal calling
 * convention puts them there left to right, so the last one is nearest the
 * return address; the offsets below are counted from CALL_ARGS.
 *
 * Free software, GPL v2 or later.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include "run286.h"

/* the error codes these functions return in AX */
#define ERROR_INVALID_PARAMETER	87
#define ERROR_NOT_ENOUGH_MEMORY	8

/* access rights of a 16bit, DPL 3, present data segment */
#define AR_DATA16	0x00f2
/* the same, code, readable, and 16 or 32 bit */
#define AR_CODE16	0x00fa
#define AR_CODE32	0x40fa

uint16_t call_argw(struct call *c, unsigned off)
{
    return _farpeekw(c->ss, c->sp + CALL_ARGS + off);
}

uint32_t call_argd(struct call *c, unsigned off)
{
    return _farpeekl(c->ss, c->sp + CALL_ARGS + off);
}

/* A far pointer argument is a selector in the high half and an offset in
 * the low one, so it can be poked at directly. A null pointer means the
 * caller does not want the value back; BioForge passes one for the old
 * vectors it is never going to restore. */
void call_setw(uint32_t fp, uint16_t val)
{
    if (!(fp >> 16))
	return;
    _farpokew(fp >> 16, fp & 0xffff, val);
}

void call_setd(uint32_t fp, uint32_t val)
{
    if (!(fp >> 16))
	return;
    _farpokel(fp >> 16, fp & 0xffff, val);
}

/* Hands out one descriptor covering [base, base + size). A size of zero
 * means the full 64K a 16bit selector can reach: the programs get there by
 * pushing a 16bit count, so a 64K request arrives as 0. */
static uint16_t map_seg(uint32_t base, uint32_t size, uint32_t selp)
{
    uint16_t sel;

    if (!size)
	size = 0x10000;
    sel = __dpmi_allocate_ldt_descriptors(1);
    if (sel == (uint16_t)-1)
	return ERROR_NOT_ENOUGH_MEMORY;
    if (__dpmi_set_segment_base_address(sel, base) == -1 ||
	    __dpmi_set_segment_limit(sel, size - 1) == -1 ||
	    __dpmi_set_descriptor_access_rights(sel, AR_DATA16) == -1) {
	__dpmi_free_ldt_descriptor(sel);
	return ERROR_INVALID_PARAMETER;
    }
    call_setw(selp, sel);
    return 0;
}

/* USHORT DosCreateDSAlias(SEL sel, PSEL aselp) */
static uint16_t dos_create_ds_alias(struct call *c)
{
    uint32_t aselp = call_argd(c, 0);
    uint16_t sel = call_argw(c, 4);
    int alias = __dpmi_create_alias_descriptor(sel);

    if (alias == -1)
	return ERROR_INVALID_PARAMETER;
    call_setw(aselp, alias);
    return 0;
}

/* USHORT DosMapRealSeg(USHORT rm_para, ULONG size, PSEL selp) */
static uint16_t dos_map_real_seg(struct call *c)
{
    uint32_t selp = call_argd(c, 0);
    uint32_t size = call_argd(c, 4);
    uint16_t para = call_argw(c, 8);

    return map_seg((uint32_t)para << 4, size, selp);
}

uint32_t ldt_lin;
uint32_t ldt_size;
uint16_t ldt_sel_reg;

/*
 * Origin's wrapper looks for its descriptor table the way a 286 extender
 * would: sgdt for the GDT, sldt for the index of the LDT inside it, then
 * the base and limit out of that entry. dosemu2 tells a client that both
 * tables are at zero, so we hand the wrapper a page of our own with one
 * real entry in it: the LDT, as dosemu2 makes it reachable. Everything it
 * indexes after that lands in the LDT, where dosemu2 picks the writes up
 * on the alias page and applies them itself.
 */
static __dpmi_meminfo fake_gdt;
static uint16_t fake_gdt_sel;

static int fake_gdt_init(void)
{
    uint32_t off;

    if (fake_gdt.size)
	return 0;
    if (!ldt_lin || !ldt_size)
	return -1;
    fake_gdt.size = 0x10000;
    if (__dpmi_allocate_memory(&fake_gdt) == -1) {
	fake_gdt.size = 0;
	return -1;
    }
    fake_gdt_sel = __dpmi_allocate_ldt_descriptors(1);
    if (fake_gdt_sel == (uint16_t)-1 ||
	    __dpmi_set_segment_base_address(fake_gdt_sel,
		fake_gdt.address) == -1 ||
	    __dpmi_set_segment_limit(fake_gdt_sel, fake_gdt.size - 1) == -1 ||
	    __dpmi_set_descriptor_access_rights(fake_gdt_sel, AR_DATA16) == -1)
	return -1;
    for (off = 0; off < fake_gdt.size; off += 4)
	_farpokel(fake_gdt_sel, off, 0);

    off = ldt_sel_reg & 0xfff8;
    _farpokew(fake_gdt_sel, off + 0, (ldt_size - 1) & 0xffff);
    _farpokew(fake_gdt_sel, off + 2, ldt_lin & 0xffff);
    _farpokeb(fake_gdt_sel, off + 4, (ldt_lin >> 16) & 0xff);
    _farpokeb(fake_gdt_sel, off + 5, 0x82);	/* present, LDT */
    _farpokeb(fake_gdt_sel, off + 6, ((ldt_size - 1) >> 16) & 0x0f);
    _farpokeb(fake_gdt_sel, off + 7, (ldt_lin >> 24) & 0xff);
    trc("run286: descriptor table at %#lx, ldt entry %#x -> %#x/%#x\n",
	    (unsigned long)fake_gdt.address, off, ldt_lin, ldt_size);
    return 0;
}

/*
 * USHORT DosMapLinSeg(ULONG lin_addr, ULONG size, PSEL selp)
 *
 * Origin's wrapper runs sgdt and maps whatever it reports, so that it can
 * rewrite the descriptors of its own selectors by hand: it indexes the
 * mapping by (selector & 0xfff8) and writes the base into bytes 2, 4 and
 * 7. Under dosemu2 sgdt reads back as base 0, so what it asks to map is
 * the interrupt vector table and the whole of low memory, and letting it
 * write there takes the host down with it.
 *
 * Its selectors are the ones we handed out, which live in the LDT, so the
 * table it is really indexing is the LDT. Point the mapping there and its
 * writes land in the right place: dosemu2 catches them on the alias page
 * and applies them itself, in msdos_ldt.c.
 */
static uint16_t dos_map_lin_seg(struct call *c)
{
    uint32_t selp = call_argd(c, 0);
    uint32_t size = call_argd(c, 4);
    uint32_t lin = call_argd(c, 8);

    if (lin < 0x1000) {
	if (fake_gdt_init() != 0)
	    return ERROR_NOT_ENOUGH_MEMORY;
	return map_seg(fake_gdt.address + lin, size, selp);
    }
    return map_seg(lin, size, selp);
}

/* USHORT DosGetBIOSSeg(PSEL selp) - the BIOS data area at 0040:0000 */
static uint16_t dos_get_bios_seg(struct call *c)
{
    return map_seg(0x400, 0x100, call_argd(c, 0));
}

/* USHORT DosAllocRealSeg(ULONG size, PUSHORT parap, PSEL selp) */
static uint16_t dos_alloc_real_seg(struct call *c)
{
    uint32_t selp = call_argd(c, 0);
    uint32_t parap = call_argd(c, 4);
    uint32_t size = call_argd(c, 8);
    int sel, para;

    if (size > 0x100000)
	return ERROR_INVALID_PARAMETER;
    para = __dpmi_allocate_dos_memory((size + 15) >> 4, &sel);
    if (para == -1)
	return ERROR_NOT_ENOUGH_MEMORY;
    /* The host builds that descriptor for its client, and its client is
     * us, so it comes out 32bit. The program is 16bit and puts such a
     * segment in SS: with B set the machine would then push at ESP, whose
     * high half a 16bit program never writes. */
    if (__dpmi_set_descriptor_access_rights(sel, AR_DATA16) == -1) {
	__dpmi_free_dos_memory(sel);
	return ERROR_INVALID_PARAMETER;
    }
    call_setw(parap, para);
    call_setw(selp, sel);
    return 0;
}

/* DPMI frees a block by handle, PHAPI by linear address, so the handles
 * have to be kept around. */
#define MAX_LINMEM	1024
static __dpmi_meminfo linmem[MAX_LINMEM];

/*
 * The programs measure memory by asking for ever smaller blocks until one
 * is granted, BioForge in steps of one page from 8Mb down, so the answer
 * to a request that cannot be met has to be cheap. DPMI has no call for
 * how much linear memory is left, so find it once by halving, and keep
 * the number until something is allocated or freed.
 */
static uint32_t lin_avail;
static int lin_avail_known;

static int lin_alloc(__dpmi_meminfo *m, uint32_t size)
{
    m->size = size;
    m->address = 0;
    return __dpmi_allocate_linear_memory(m, 1);
}

static uint32_t lin_probe(void)
{
    __dpmi_meminfo m = {};
    uint32_t lo = 0, hi = 0x10000000;

    while (hi - lo > 0x1000) {
	uint32_t mid = lo + (hi - lo) / 2;

	if (lin_alloc(&m, mid & ~0xfff) == 0) {
	    __dpmi_free_memory(m.handle);
	    lo = mid;
	} else {
	    hi = mid;
	}
    }
    return lo & ~0xfff;
}

/* USHORT DosAllocLinMem(ULONG size, PULONG lin_addp) */
static uint16_t dos_alloc_lin_mem(struct call *c)
{
    uint32_t linp = call_argd(c, 0);
    uint32_t size = call_argd(c, 4);
    __dpmi_meminfo m = {};
    __dpmi_free_mem_info mi;
    int i;

    for (i = 0; i < MAX_LINMEM; i++) {
	if (!linmem[i].size)
	    break;
    }
    if (i == MAX_LINMEM)
	return ERROR_NOT_ENOUGH_MEMORY;
    /*
     * The name is not decoration: the programs care where a block lands.
     * BioForge builds its arena by asking for 8Mb at a time and throws away
     * everything that ends above linear 30Mb, so blocks out of the ordinary
     * DPMI pool, which dosemu2 keeps above $_dpmi_base, leave it with an
     * arena of nothing. DPMI 1.0 has a second pool for exactly this, the
     * linear one below $_dpmi_base, so take the memory from there.
     */
    if (!lin_avail_known) {
	lin_avail = lin_probe();
	lin_avail_known = 1;
    }
    if (size <= lin_avail && lin_alloc(&m, size) == 0) {
	lin_avail_known = 0;
	linmem[i] = m;
	call_setd(linp, m.address);
	if (run286_trace)
	    trc("run286:   linmem[%d] %u bytes at %#lx, low pool\n", i, size,
		    (unsigned long)m.address);
	return 0;
    }
    /* Nothing left down there. The programs measure memory by asking for
     * ever smaller blocks until one is granted, so a host that hands out
     * more than it has turns that into a very long loop: answer the rest
     * from what DPMI says is left in the ordinary pool. */
    if (__dpmi_get_free_memory_information(&mi) == 0 &&
	    size > mi.largest_available_free_block_in_bytes)
	return ERROR_NOT_ENOUGH_MEMORY;
    m.size = size;
    m.address = 0;
    if (__dpmi_allocate_memory(&m) == -1)
	return ERROR_NOT_ENOUGH_MEMORY;
    linmem[i] = m;
    call_setd(linp, m.address);
    if (run286_trace)
	trc("run286:   linmem[%d] %u bytes at %#lx, dpmi pool\n", i, size,
		(unsigned long)m.address);
    return 0;
}

/* USHORT DosFreeLinMem(ULONG lin_add) */
static uint16_t dos_free_lin_mem(struct call *c)
{
    uint32_t lin = call_argd(c, 0);
    int i;

    for (i = 0; i < MAX_LINMEM; i++) {
	if (linmem[i].size && linmem[i].address == lin)
	    break;
    }
    if (i == MAX_LINMEM || __dpmi_free_memory(linmem[i].handle) == -1) {
	if (run286_trace)
	    trc("run286:   linmem free %#lx: not ours\n", (unsigned long)lin);
	return ERROR_INVALID_PARAMETER;
    }
    if (run286_trace)
	trc("run286:   linmem[%d] freed %#lx\n", i, (unsigned long)lin);
    linmem[i].size = 0;
    lin_avail_known = 0;
    return 0;
}

/*
 * The interrupt block a program hands to _DosRealIntr: thirteen words in
 * the order OS/2 1.x used, with the register the caller cares about most
 * last but one. Taken from BioForge's own use of it, which memsets 26
 * bytes, puts 0x3400 at offset 18 to ask DOS for the InDOS flag and then
 * reads the answer back out of offsets 0 and 12: es and bx.
 */
#define RR_ES	0
#define RR_DS	2
#define RR_DI	4
#define RR_SI	6
#define RR_BP	8
#define RR_SP	10
#define RR_BX	12
#define RR_DX	14
#define RR_CX	16
#define RR_AX	18
#define RR_IP	20
#define RR_CS	22
#define RR_FLAGS 24

/*
 * With a null register block the call is documented to run on whatever the
 * caller already has in its registers, and to hand back what the real mode
 * routine left there. gate_entry() saved them on the program's stack, so
 * that frame is both the source and the destination.
 */
static void callerregs_get(struct call *c, __dpmi_regs *r)
{
    memset(r, 0, sizeof(*r));
    r->x.es = _farpeekw(c->ss, c->sp + CALL_ES);
    r->x.ds = _farpeekw(c->ss, c->sp + CALL_DS);
    r->x.di = _farpeekw(c->ss, c->sp + CALL_EDI);
    r->x.si = _farpeekw(c->ss, c->sp + CALL_ESI);
    r->x.bp = _farpeekw(c->ss, c->sp + CALL_EBP);
    r->x.bx = _farpeekw(c->ss, c->sp + CALL_EBX);
    r->x.dx = _farpeekw(c->ss, c->sp + CALL_EDX);
    r->x.cx = _farpeekw(c->ss, c->sp + CALL_ECX);
    r->x.ax = _farpeekw(c->ss, c->sp + CALL_EAX);
}

/*
 * Only the general registers go back. ES and DS stay as the program had
 * them: they are protected mode selectors there and real mode segments
 * here, and gate_entry() reloads them on the way out. AX stays too, since
 * that is where the caller reads the PHAPI status code from.
 */
static void callerregs_put(struct call *c, const __dpmi_regs *r)
{
    _farpokew(c->ss, c->sp + CALL_EDI, r->x.di);
    _farpokew(c->ss, c->sp + CALL_ESI, r->x.si);
    _farpokew(c->ss, c->sp + CALL_EBP, r->x.bp);
    _farpokew(c->ss, c->sp + CALL_EBX, r->x.bx);
    _farpokew(c->ss, c->sp + CALL_EDX, r->x.dx);
    _farpokew(c->ss, c->sp + CALL_ECX, r->x.cx);
}

static void realregs_get(uint16_t sel, uint16_t off, __dpmi_regs *r)
{
    memset(r, 0, sizeof(*r));
    r->x.es = _farpeekw(sel, off + RR_ES);
    r->x.ds = _farpeekw(sel, off + RR_DS);
    r->x.di = _farpeekw(sel, off + RR_DI);
    r->x.si = _farpeekw(sel, off + RR_SI);
    r->x.bp = _farpeekw(sel, off + RR_BP);
    r->x.bx = _farpeekw(sel, off + RR_BX);
    r->x.dx = _farpeekw(sel, off + RR_DX);
    r->x.cx = _farpeekw(sel, off + RR_CX);
    r->x.ax = _farpeekw(sel, off + RR_AX);
    /* let DPMI pick the real mode stack */
    r->x.ss = r->x.sp = 0;
}

static void realregs_put(uint16_t sel, uint16_t off, const __dpmi_regs *r)
{
    _farpokew(sel, off + RR_ES, r->x.es);
    _farpokew(sel, off + RR_DS, r->x.ds);
    _farpokew(sel, off + RR_DI, r->x.di);
    _farpokew(sel, off + RR_SI, r->x.si);
    _farpokew(sel, off + RR_BP, r->x.bp);
    _farpokew(sel, off + RR_BX, r->x.bx);
    _farpokew(sel, off + RR_DX, r->x.dx);
    _farpokew(sel, off + RR_CX, r->x.cx);
    _farpokew(sel, off + RR_AX, r->x.ax);
    _farpokew(sel, off + RR_FLAGS, r->x.flags);
}

/*
 * USHORT _DosRealIntr(USHORT intno, PREALREGS regs, USHORT copy, ULONG rsv)
 *
 * The leading underscore is not decoration: these entry points take their
 * arguments the C way, so the caller pops them and the first one is the
 * one nearest the return address. The stub for them must not pop, which
 * is what an argument count of zero in the table below means.
 */
static uint16_t dos_real_intr(struct call *c)
{
    uint16_t intno = call_argw(c, 0);
    uint16_t off = call_argw(c, 2);
    uint16_t sel = call_argw(c, 4);
    __dpmi_regs r;

    if (sel)
	realregs_get(sel, off, &r);
    else
	callerregs_get(c, &r);
    r.x.ss = r.x.sp = 0;
    if (__dpmi_int(intno, &r) == -1)
	return ERROR_INVALID_PARAMETER;
    if (sel)
	realregs_put(sel, off, &r);
    else
	callerregs_put(c, &r);
    return 0;
}

/*
 * USHORT _DosRealFarCall(REALPTR fn, PREALREGS regs, ULONG copy, ...)
 *
 * The register block is optional: with a null pointer the call runs on the
 * caller's own registers and gives them back, which is how BioForge drives
 * its real mode routines.
 */
static uint16_t dos_real_far_call(struct call *c)
{
    uint32_t fn = call_argd(c, 0);
    uint16_t off = call_argw(c, 4);
    uint16_t sel = call_argw(c, 6);
    __dpmi_regs r;

    if (sel)
	realregs_get(sel, off, &r);
    else
	callerregs_get(c, &r);
    r.x.ss = r.x.sp = 0;
    r.x.cs = fn >> 16;
    r.x.ip = fn & 0xffff;
    if (__dpmi_simulate_real_mode_procedure_retf(&r) == -1)
	return ERROR_INVALID_PARAMETER;
    if (sel)
	realregs_put(sel, off, &r);
    else
	callerregs_put(c, &r);
    return 0;
}

/*
 * USHORT DosSetPassToProtVec(USHORT intno, PFN protfn, PPFN oldprotp,
 *			      PREALPTR oldrealp)
 *
 * Hook an interrupt so that it reaches a protected mode handler whichever
 * mode it arrives in. That is what a DPMI host does for a vector the
 * client owns, so setting the protected mode vector is the whole of it.
 * Which of the two out parameters is the real one and which the protected
 * one is a guess: they are neighbouring longs in the caller's data and
 * BioForge only hands them back to us when it unhooks.
 */
/*
 * The program's interrupt handlers are 16bit and end in a plain iret, which
 * pops six bytes. We are a 32bit DPMI client, so the host hands every
 * handler a twelve byte frame, as the spec tells it to; the handler's own
 * segment does not say which iret it will run, so the host cannot get this
 * right on its own and should not try. Put a thunk of our own on the
 * vector instead: it is 32bit, so it gets the frame the host means to give,
 * lays the six byte one the program expects on top of it, and jumps to the
 * handler. The program's iret lands on a 16bit stub holding nothing but a
 * 66 CF, which is iretd, and that returns the host's own frame.
 */
#define THUNK_SLOTS	48
#define THUNK_SLOT_SIZE	32
#define THUNK_RET_OFF	0		/* the shared 66 CF, before the slots */
#define THUNK_BASE	THUNK_SLOT_SIZE

static __dpmi_meminfo thunk_mem;
static uint16_t thunk_code32_sel;	/* what goes on the vector */
static uint16_t thunk_code16_sel;	/* the stub the program irets to */
static uint16_t thunk_data_sel;		/* how we write the bytes */
static uint32_t thunk_target[THUNK_SLOTS];
static unsigned thunk_used;

static int thunk_init(void)
{
    uint16_t sel;

    if (thunk_code32_sel)
	return 0;
    thunk_mem.size = THUNK_BASE + THUNK_SLOTS * THUNK_SLOT_SIZE;
    if (__dpmi_allocate_memory(&thunk_mem) == -1)
	return -1;
    sel = __dpmi_allocate_ldt_descriptors(3);
    if (sel == (uint16_t)-1)
	return -1;
    if (__dpmi_set_segment_base_address(sel, thunk_mem.address) == -1 ||
	    __dpmi_set_segment_limit(sel, thunk_mem.size - 1) == -1 ||
	    __dpmi_set_descriptor_access_rights(sel, AR_CODE32) == -1 ||
	    __dpmi_set_segment_base_address(sel + 8, thunk_mem.address) == -1 ||
	    __dpmi_set_segment_limit(sel + 8, thunk_mem.size - 1) == -1 ||
	    __dpmi_set_descriptor_access_rights(sel + 8, AR_CODE16) == -1 ||
	    __dpmi_set_segment_base_address(sel + 16, thunk_mem.address) == -1 ||
	    __dpmi_set_segment_limit(sel + 16, thunk_mem.size - 1) == -1 ||
	    __dpmi_set_descriptor_access_rights(sel + 16, AR_DATA16) == -1)
	return -1;
    thunk_code32_sel = sel;
    thunk_code16_sel = sel + 8;
    thunk_data_sel = sel + 16;
    _farpokeb(thunk_data_sel, THUNK_RET_OFF, 0x66);	/* iretd */
    _farpokeb(thunk_data_sel, THUNK_RET_OFF + 1, 0xcf);
    if (run286_trace)
	trc("run286: thunks at %#lx, %u bytes, sel %04x/%04x/%04x\n",
		(unsigned long)thunk_mem.address, thunk_mem.size,
		thunk_code32_sel, thunk_code16_sel, thunk_data_sel);
    return 0;
}

/*
 * Give back the address to put on the vector, or the handler itself if we
 * cannot build a thunk: better a handler that may not return than none.
 */
static uint32_t thunk_for(uint32_t protfn)
{
    unsigned i, off;

    if (thunk_init() != 0)
	return protfn;
    for (i = 0; i < thunk_used; i++) {
	if (thunk_target[i] == protfn)
	    return ((uint32_t)thunk_code32_sel << 16) |
		    (THUNK_BASE + i * THUNK_SLOT_SIZE);
    }
    if (thunk_used == THUNK_SLOTS)
	return protfn;
    i = thunk_used++;
    thunk_target[i] = protfn;
    off = THUNK_BASE + i * THUNK_SLOT_SIZE;

    /* pushw 8(%esp) - the low half of the eflags the host pushed */
    _farpokeb(thunk_data_sel, off++, 0x66);
    _farpokeb(thunk_data_sel, off++, 0xff);
    _farpokeb(thunk_data_sel, off++, 0x74);
    _farpokeb(thunk_data_sel, off++, 0x24);
    _farpokeb(thunk_data_sel, off++, 0x08);
    /* pushw $thunk_code16_sel - the cs the program irets to */
    _farpokeb(thunk_data_sel, off++, 0x66);
    _farpokeb(thunk_data_sel, off++, 0x68);
    _farpokew(thunk_data_sel, off, thunk_code16_sel);
    off += 2;
    /* pushw $THUNK_RET_OFF - and the ip */
    _farpokeb(thunk_data_sel, off++, 0x66);
    _farpokeb(thunk_data_sel, off++, 0x68);
    _farpokew(thunk_data_sel, off, THUNK_RET_OFF);
    off += 2;
    if (run286_trace)
	trc("run286:   thunk %u at %04x:%04x -> %04x:%04x\n", i,
		thunk_code32_sel, THUNK_BASE + i * THUNK_SLOT_SIZE,
		(uint16_t)(protfn >> 16), (uint16_t)protfn);
    /* ljmpw $sel:$off - into the program's handler */
    _farpokeb(thunk_data_sel, off++, 0x66);
    _farpokeb(thunk_data_sel, off++, 0xea);
    _farpokew(thunk_data_sel, off, protfn & 0xffff);
    off += 2;
    _farpokew(thunk_data_sel, off, protfn >> 16);

    return ((uint32_t)thunk_code32_sel << 16) |
	    (THUNK_BASE + i * THUNK_SLOT_SIZE);
}

static uint16_t dos_set_pass_to_prot_vec(struct call *c)
{
    uint32_t oldrealp = call_argd(c, 0);
    uint32_t oldprotp = call_argd(c, 4);
    uint32_t protfn = call_argd(c, 8);
    uint16_t intno = call_argw(c, 12);
    __dpmi_paddr pm;
    __dpmi_raddr rm;

    if (__dpmi_get_protected_mode_interrupt_vector(intno, &pm) == -1 ||
	    __dpmi_get_real_mode_interrupt_vector(intno, &rm) == -1)
	return ERROR_INVALID_PARAMETER;
    call_setd(oldprotp, ((uint32_t)pm.selector << 16) | (pm.offset32 & 0xffff));
    call_setd(oldrealp, ((uint32_t)rm.segment << 16) | rm.offset16);
    protfn = thunk_for(protfn);
    pm.selector = protfn >> 16;
    pm.offset32 = protfn & 0xffff;
    if (__dpmi_set_protected_mode_interrupt_vector(intno, &pm) == -1)
	return ERROR_INVALID_PARAMETER;
    return 0;
}

/*
 * USHORT DosSetRealProtVec(USHORT intno, PFN protfn, REALPTR realfn,
 *			    PPFN oldprotp, PREALPTR oldrealp)
 *
 * The same, except that the caller also names the real mode handler. Under
 * DPMI the two vectors are separate, so both get set; BioForge uses this
 * for its sound card IRQ and passes the vector the host already has as the
 * real mode half, so an interrupt that arrives in real mode still lands
 * somewhere sane.
 */
static uint16_t dos_set_real_prot_vec(struct call *c)
{
    uint32_t oldrealp = call_argd(c, 0);
    uint32_t oldprotp = call_argd(c, 4);
    uint32_t realfn = call_argd(c, 8);
    uint32_t protfn = call_argd(c, 12);
    uint16_t intno = call_argw(c, 16);
    __dpmi_paddr pm;
    __dpmi_raddr rm;

    if (__dpmi_get_protected_mode_interrupt_vector(intno, &pm) == -1 ||
	    __dpmi_get_real_mode_interrupt_vector(intno, &rm) == -1)
	return ERROR_INVALID_PARAMETER;
    call_setd(oldprotp, ((uint32_t)pm.selector << 16) | (pm.offset32 & 0xffff));
    call_setd(oldrealp, ((uint32_t)rm.segment << 16) | rm.offset16);
    protfn = thunk_for(protfn);
    pm.selector = protfn >> 16;
    pm.offset32 = protfn & 0xffff;
    rm.segment = realfn >> 16;
    rm.offset16 = realfn & 0xffff;
    if (__dpmi_set_protected_mode_interrupt_vector(intno, &pm) == -1 ||
	    __dpmi_set_real_mode_interrupt_vector(intno, &rm) == -1)
	return ERROR_INVALID_PARAMETER;
    return 0;
}

/* USHORT DosSetExceptionHandler(USHORT exc, PFN handler, PPFN oldp) */
static uint16_t dos_set_exception_handler(struct call *c)
{
    uint32_t oldp = call_argd(c, 0);
    uint32_t fn = call_argd(c, 4);
    uint16_t exc = call_argw(c, 8);
    __dpmi_paddr pm;

    if (__dpmi_get_processor_exception_handler_vector(exc, &pm) == -1)
	return ERROR_INVALID_PARAMETER;
    call_setd(oldp, ((uint32_t)pm.selector << 16) | (pm.offset32 & 0xffff));
    pm.selector = fn >> 16;
    pm.offset32 = fn & 0xffff;
    if (__dpmi_set_processor_exception_handler_vector(exc, &pm) == -1)
	return ERROR_INVALID_PARAMETER;
    return 0;
}

/* USHORT DosIsPharLap(void) - yes, of a sort */
static uint16_t dos_is_pharlap(struct call *c)
{
    return 1;
}

static const struct api_fn phapi[] = {
    { "DOSCREATEDSALIAS",	0, 6,	dos_create_ds_alias },
    { "DOSMAPREALSEG",		0, 10,	dos_map_real_seg },
    { "DOSMAPLINSEG",		0, 12,	dos_map_lin_seg },
    { "DOSGETBIOSSEG",		0, 4,	dos_get_bios_seg },
    { "DOSALLOCREALSEG",	0, 12,	dos_alloc_real_seg },
    { "DOSALLOCLINMEM",		0, 8,	dos_alloc_lin_mem },
    { "DOSFREELINMEM",		0, 4,	dos_free_lin_mem },
    { "DOSISPHARLAP",		0, 0,	dos_is_pharlap },
    { "DOSSETPASSTOPROTVEC",	0, 14,	dos_set_pass_to_prot_vec },
    { "DOSSETREALPROTVEC",	0, 18,	dos_set_real_prot_vec },
    { "DOSSETEXCEPTIONHANDLER",	0, 10,	dos_set_exception_handler },
    { "_DosRealIntr",		0, 0,	dos_real_intr },
    { "_DosRealFarCall",	0, 0,	dos_real_far_call },
};

const struct api_fn *phapi_lookup(const char *name)
{
    unsigned i;

    for (i = 0; i < sizeof(phapi) / sizeof(phapi[0]); i++) {
	if (!strcmp(phapi[i].name, name))
	    return &phapi[i];
    }
    return NULL;
}
