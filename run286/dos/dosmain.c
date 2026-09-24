/*
 * run286 under DOS: load the NE image of a Phar Lap bound program into
 * descriptors obtained from DPMI. Nothing here touches the LDT directly.
 *
 * Built with dj64, so the C below is host-side 64bit code; everything the
 * program itself will see comes from DPMI calls and from fmemcpy1().
 *
 * Free software, GPL v2 or later.
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <dpmi.h>
#include <sys/fmemcpy.h>
#include <sys/segments.h>
#include <sys/farptr.h>
#include <go32.h>
#include "neload.h"
#include "asm.h"
#include "run286.h"

/*
 * The trace is long and the programs that need it run in a graphics mode,
 * where DOS stdout is not readable. Put it in a file next to the image when
 * RUN286_LOG names one.
 */
static FILE *trace_fp;
int run286_trace;

/*
 * The trace goes out through DOS, and the program calls our imports from
 * its interrupt handlers too: BioForge's timer handler calls
 * DosRealFarCall on every tick. A tick that lands while DOS is inside a
 * call the program made (an open, a read) must not enter DOS again: DOS
 * is not reentrant, and the outer call comes back with the registers of
 * our write, so the program's BP turns zero and it dies on its next
 * retf. While the InDOS flag is up, keep the text and write it out with
 * the next line that finds DOS free.
 */
static unsigned indos_addr;
static char trc_held[16384];
static unsigned trc_held_len, trc_dropped;

static void trc_init_indos(void)
{
    __dpmi_regs r;

    memset(&r, 0, sizeof(r));
    r.h.ah = 0x34;
    if (__dpmi_int(0x21, &r) == 0)
	indos_addr = r.x.es * 16 + r.x.bx;
}

static int dos_busy(void)
{
    return indos_addr && _farpeekb(_dos_ds, indos_addr);
}

static void trc_out(const char *s)
{
    if (trace_fp) {
	fputs(s, trace_fp);
	fflush(trace_fp);
    } else {
	fputs(s, stdout);
	fflush(stdout);
    }
}

void trc(const char *fmt, ...)
{
    char line[1024];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (dos_busy()) {
	size_t len = strlen(line);

	if (trc_held_len + len < sizeof(trc_held)) {
	    memcpy(trc_held + trc_held_len, line, len + 1);
	    trc_held_len += len;
	} else {
	    trc_dropped++;
	}
	return;
    }
    if (trc_held_len) {
	trc_held_len = 0;
	trc_out(trc_held);
	trc_out("run286: PROBE held the lines above while DOS was busy\n");
    }
    if (trc_dropped) {
	char note[80];

	snprintf(note, sizeof(note), "run286: %u trace lines lost while "
		"DOS was busy\n", trc_dropped);
	trc_dropped = 0;
	trc_out(note);
    }
    trc_out(line);
}

/*
 * What the program's own interrupt handlers have seen. Per vector, because
 * "the last one" is always the timer once the timer runs at all, and the
 * question is usually about a handler that never runs: BioForge hooks the
 * timer, the keyboard and its sound IRQ, and only one of the three firing
 * looks exactly like all three firing if you only print the last.
 */
unsigned gate_depth, gate_nested, gate_deepest;
/* set by the imports that are meant to hand the caller new registers */
int frame_bp_ok;

/* INSTRUMENTATION: how far down does each stack the program uses ever get? */
static struct sswatch {
    unsigned ss, minsp, maxsp, n;
} sstab[8];
static unsigned ss_ticks;

static void ss_note(unsigned ss, unsigned sp)
{
    unsigned i;

    ss &= 0xffff;
    sp &= 0xffff;
    for (i = 0; i < 8; i++) {
	if (!sstab[i].n) {
	    sstab[i].ss = ss;
	    sstab[i].minsp = sstab[i].maxsp = sp;
	    sstab[i].n = 1;
	    return;
	}
	if (sstab[i].ss == ss)
	    break;
    }
    if (i == 8)
	return;
    if (sp < sstab[i].minsp)
	sstab[i].minsp = sp;
    if (sp > sstab[i].maxsp)
	sstab[i].maxsp = sp;
    sstab[i].n++;
}

static void ss_report(const char *when)
{
    unsigned i;

    trc("run286:   stacks %s: our int stack %04x (top %04x)\n", when,
	    (uint16_t)int_stk_ss, (uint16_t)int_stk_esp);
    trc("run286:   gate depth %u, nested %u times, deepest %u\n",
	    gate_depth, gate_nested, gate_deepest);
    for (i = 0; i < 8 && sstab[i].n; i++)
	trc("run286:   stack %04x: sp seen %04x..%04x, limit %08x, %u samples\n",
		(uint16_t)sstab[i].ss, sstab[i].minsp, sstab[i].maxsp,
		__dpmi_get_segment_limit(sstab[i].ss), sstab[i].n);
}

void rr_report(void);

/*
 * How many interrupts of one vector were ever in flight at once. Each one
 * takes a piece of that vector's stack and gives it back on the way out,
 * so the lowest the free pointer ever went says how deep the vector
 * nested. A handler that enables interrupts nests; one that does not
 * never goes past one.
 */
static void int_depth_report(void)
{
    unsigned i;
    unsigned *hdr = (unsigned *)int_slot_esp;

    trc("run286:   PROBE: saved BP changed under %u handlers (bp %04x -> %04x), "
	    "%u interrupts arrived with bp 0\n", hdr[12], hdr[13] & 0xffff,
	    hdr[14] & 0xffff, hdr[15]);

    trc("run286:   %u interrupts landed while another was on its way out; "
	    "the last one landed at %04x:%08x\n", int_in_ret,
	    (uint16_t)int_last_cs, int_last_eip);
    for (i = 0; i < INT_SLOTS; i++) {
	unsigned top = INT_STACK_HDR + (i + 1) * INT_STACK_LEN;

	if (!int_slot_low[i] || int_slot_low[i] >= top - INT_FRAME_LEN)
	    continue;
	trc("run286:   slot %u went %u deep\n", i,
		(top - int_slot_low[i]) / INT_FRAME_LEN);
    }
}

static void trace_interrupts(const char *when)
{
    char buf[INT_SLOTS * 12], *p = buf;
    unsigned i;

    for (i = 0; i < INT_SLOTS; i++)
	p += sprintf(p, "%u ", int_count[i]);
    trc("run286:   %u interrupts %s (%u entries into the stub), by slot: %s\n",
	    int_taken, when, int_entered, buf);
    dump_hooked_vectors();
}


/* access rights for a 16bit, DPL 3, present segment */
#define AR_CODE16	0x00fa		/* code, readable */
#define AR_DATA16	0x00f2		/* data, writable */

#define GATE_INT	0x66		/* free vector the stubs go through */
#define TRAP_IDX	0xffff		/* the stub int 3 goes through */
#define TRAP_EXC_IDX	0xfffe		/* the same, as a DPMI exception */
#define STUB_SIZE	8		/* one import stub, see make_stub() */
#define MAX_IMPORTS	1024
#define MAX_MODULES	8
#define SEG_STRIDE	0x10000		/* linear room reserved per segment */

struct import {
    char mod[9];
    char name[64];		/* empty when imported by ordinal */
    uint16_t ord;
    const struct api_fn *fn;	/* NULL while unimplemented */
};

struct seg_info {
    uint8_t *shadow;
    uint32_t size;
    unsigned long lin;
    uint16_t sel;
};

/*
 * The program and every DLL it imports from. A DLL the games ship beside
 * the program is a plain NE of its own, so it loads the same way: its
 * segments get descriptors of their own and its relocations go through
 * the same ops, with its own module as the context.
 */
struct module {
    char name[9];
    uint8_t *file;
    struct ne_image ne;
    struct seg_info *seg;
    __dpmi_meminfo mem;
    uint16_t base_sel;
};

struct dos_ldr {
    struct module mod[MAX_MODULES];	/* the program is mod[0] */
    int nmod;
    unsigned nstub;		/* imports we could not resolve yet */
    unsigned ncall;		/* API calls served so far */
    int trace;
    /* the 16bit segment holding one stub per import */
    __dpmi_meminfo stub_mem;
    uint16_t stub_code_sel;	/* what the program far-calls into */
    uint16_t stub_data_sel;	/* the same memory, so we can write it */
    struct import imp[MAX_IMPORTS];
};

static struct dos_ldr ldr;

static uint8_t *slurp(const char *path, size_t *size);
/* Low address space held back while the image is allocated, so that the
 * program's own arena can have it instead. */
#define LOW_RESERVE	(12 * 1024 * 1024)

static int load_segments(struct module *m);
static int commit_segments(struct module *m);
static int dos_resolve_ord(void *ctx, const char *mod, uint16_t ord,
	struct ne_far *a);
static int dos_resolve_name(void *ctx, const char *mod, const char *name,
	struct ne_far *a);

static int dos_seg_addr(void *ctx, uint16_t segnum, struct ne_far *a)
{
    struct module *m = ctx;

    if (!segnum || segnum > m->ne.cseg)
	return -1;
    a->sel = m->seg[segnum - 1].sel;
    a->off = 0;
    return 0;
}

static uint8_t *dos_seg_mem(void *ctx, uint16_t segnum, uint32_t *size)
{
    struct module *m = ctx;

    if (!segnum || segnum > m->ne.cseg)
	return NULL;
    *size = m->seg[segnum - 1].size;
    return m->seg[segnum - 1].shadow;
}

/* PHAPI and DOSCALLS are not implemented yet. Every import gets a stub of
 * its own in a 16bit code segment, so the first call the program makes
 * tells us by name what it wanted. */
static int make_stub(struct dos_ldr *l, const char *mod, const char *name,
	uint16_t ord, struct ne_far *a)
{
    unsigned n = l->nstub;
    struct import *im = &l->imp[n];
    uint32_t off;

    if (n >= MAX_IMPORTS)
	return -1;
    snprintf(im->mod, sizeof(im->mod), "%s", mod ?: "?");
    snprintf(im->name, sizeof(im->name), "%s", name ?: "");
    im->ord = ord;
    if (name && !strcmp(im->mod, "PHAPI"))
	im->fn = phapi_lookup(name);
    else if (!name && !strcmp(im->mod, "DOSCALLS"))
	im->fn = doscalls_lookup(ord);

    off = n * STUB_SIZE;
    /* mov ax,n; int GATE_INT; retf [args] */
    _farpokeb(l->stub_data_sel, off + 0, 0xb8);
    _farpokew(l->stub_data_sel, off + 1, n);
    _farpokeb(l->stub_data_sel, off + 3, 0xcd);
    _farpokeb(l->stub_data_sel, off + 4, GATE_INT);
    if (im->fn && im->fn->args) {
	_farpokeb(l->stub_data_sel, off + 5, 0xca);
	_farpokew(l->stub_data_sel, off + 6, im->fn->args);
    } else {
	_farpokeb(l->stub_data_sel, off + 5, 0xcb);
	_farpokew(l->stub_data_sel, off + 6, 0x9090);
    }

    a->sel = l->stub_code_sel;
    a->off = off;
    l->nstub++;
    return 0;
}

/*
 * A DLL's exported functions run on the DLL's own data, not on the data
 * of whoever called them, and the linker leaves the loader to say so: an
 * entry whose flags carry NE_ENT_DATA begins with "mov ax,ds; nop", three
 * bytes that are there to be rewritten into "mov ax,DGROUP". The prologue
 * that follows pushes DS and loads AX into it, so the rewrite is the whole
 * of the mechanism.
 *
 * Skipping it is not harmless. AILXMI's functions then ran on BioForge's
 * own DGROUP and wrote their state over the game's, which carried on for
 * a few thousand calls and died with its stack overrun.
 */
static void patch_dll_prologues(struct module *m)
{
    struct ne_entry_iter it = {};
    uint16_t ord, segnum, off, dgroup;
    uint8_t flags;
    unsigned n = 0;

    if (!m->ne.autodata || m->ne.autodata > m->ne.cseg)
	return;
    dgroup = m->seg[m->ne.autodata - 1].sel;
    while (ne_entry_next(&m->ne, &it, &ord, &flags, &segnum, &off)) {
	struct seg_info *si;
	uint8_t *code;

	if (!(flags & NE_ENT_DATA) || !segnum || segnum > m->ne.cseg)
	    continue;
	si = &m->seg[segnum - 1];
	code = si->shadow;
	if (!code || off + 3 > si->size)
	    continue;
	if (code[off] != 0x8c || code[off + 1] != 0xd8 || code[off + 2] != 0x90)
	    continue;
	code[off] = 0xb8;		/* mov ax,imm16 */
	code[off + 1] = dgroup & 0xff;
	code[off + 2] = dgroup >> 8;
	n++;
    }
    if (n)
	trc("run286: %s: %u entry points now load DGROUP %#x\n", m->name, n,
		dgroup);
}

static struct module *find_module(const char *name)
{
    int i;

    for (i = 0; i < ldr.nmod; i++)
	if (!strcasecmp(ldr.mod[i].name, name))
	    return &ldr.mod[i];
    return NULL;
}

/*
 * A module the program imports from that is not one of ours. The games
 * ship these as plain NE DLLs in the directory they run from, so load
 * <NAME>.DLL the same way as the program: descriptors of its own, its own
 * relocations, and its code made executable before anything calls into it.
 *
 * DOS opens the file whatever case the name is in, and these DLLs have no
 * initialisation entry point, so there is nothing to enter before use.
 */
static struct module *dll_load(const char *name)
{
    struct module *m;
    struct pl_bound b;
    struct ne_ldr_ops ops = {};
    struct ne_reloc_stats st = {};
    const char *err = "";
    char path[16];
    size_t size;
    int i;

    if (ldr.nmod >= MAX_MODULES) {
	trc("run286: too many modules to load %s\n", name);
	return NULL;
    }
    m = &ldr.mod[ldr.nmod];
    memset(m, 0, sizeof(*m));
    snprintf(m->name, sizeof(m->name), "%s", name);
    snprintf(path, sizeof(path), "%.8s.DLL", name);
    m->file = slurp(path, &size);
    if (!m->file) {
	trc("run286: cannot read %s\n", path);
	return NULL;
    }
    if (pl_bound_parse(&b, m->file, size, &err) != 0 ||
	    ne_parse(&m->ne, m->file, size, b.app_off, &err) != 0) {
	trc("run286: %s: %s\n", path, err);
	free(m->file);
	m->file = NULL;
	return NULL;
    }
    m->seg = calloc(m->ne.cseg, sizeof(*m->seg));
    if (!m->seg || load_segments(m) != 0)
	return NULL;
    /* claim the slot before relocating: a DLL may import from another */
    ldr.nmod++;

    ops.ctx = m;
    ops.seg_addr = dos_seg_addr;
    ops.seg_mem = dos_seg_mem;
    ops.resolve_ord = dos_resolve_ord;
    ops.resolve_name = dos_resolve_name;
    for (i = 1; i <= m->ne.cseg; i++) {
	if (ne_relocate(&m->ne, i, &ops, &st, &err) != 0) {
	    trc("run286: %s segment %d: %s\n", path, i, err);
	    return NULL;
	}
    }
    patch_dll_prologues(m);
    if (commit_segments(m) != 0)
	return NULL;
    trc("run286: loaded %s, %u segments, %u fixups, %u unresolved\n",
	    path, m->ne.cseg, st.applied, st.unresolved);
    return m;
}

/*
 * Resolve an import against a DLL rather than against our own API. The
 * name a program imports by and the name the DLL exports under differ in
 * case often enough that ne_name_ordinal() ignores it.
 */
static int dll_import(const char *mod, const char *name, uint16_t ord,
	struct ne_far *a)
{
    struct module *m;
    uint16_t segnum, off;

    if (!mod || !strcmp(mod, "PHAPI") || !strcmp(mod, "DOSCALLS"))
	return -1;
    m = find_module(mod);
    if (!m) {
	m = dll_load(mod);
	if (!m)
	    return -1;
    }
    if (name) {
	ord = ne_name_ordinal(&m->ne, name);
	if (!ord) {
	    trc("run286: %s exports no %s\n", mod, name);
	    return -1;
	}
    }
    if (ne_entry_lookup(&m->ne, ord, &segnum, &off) != 0 ||
	    !segnum || segnum > m->ne.cseg) {
	trc("run286: %s has no entry %u\n", mod, ord);
	return -1;
    }
    a->sel = m->seg[segnum - 1].sel;
    a->off = off;
    if (ldr.trace)
	trc("run286: %s.%s#%u -> %04x:%04x\n", mod, name ?: "", ord,
		a->sel, a->off);
    return 0;
}

static int dos_resolve_ord(void *ctx, const char *mod, uint16_t ord,
	struct ne_far *a)
{
    uint16_t val;

    if (dll_import(mod, NULL, ord, a) == 0)
	return 0;
    /* A constant export, not an address: the fixup is an OFF16 and the
     * value goes into the instruction as it stands. */
    if (mod && !strcmp(mod, "DOSCALLS") && doscalls_const(ord, &val) == 0) {
	a->sel = 0;
	a->off = val;
	return 0;
    }
    return make_stub(&ldr, mod, NULL, ord, a);
}

static int dos_resolve_name(void *ctx, const char *mod, const char *name,
	struct ne_far *a)
{
    if (dll_import(mod, name, 0, a) == 0)
	return 0;
    return make_stub(&ldr, mod, name, 0, a);
}

/*
 * The program executed int 3. Say where, and hand back whatever message it
 * had built below its frame pointer: a program that traps on purpose has
 * just formatted one there, and it is the only explanation we are going to
 * get.
 *
 * The frames both start above gate_entry's own saves and the int GATE_INT
 * frame the stub added, and the host builds them to its client's bitness,
 * which here is 32bit even though the stub the trap arrives on is not. An
 * exception frame carries a far return address and an error code before
 * the machine state, and names the stack the program was on; an interrupt
 * frame is the machine state alone, on that same stack.
 */
#define TRAP_BASE	(CALL_EAX + 4 + 12)	/* past the GATE_INT frame */

static void report_trap(struct call *c, int exception)
{
    uint16_t bp = _farpeekw(c->ss, c->sp + CALL_EBP);
    unsigned st = c->sp + TRAP_BASE + (exception ? 12 : 0);
    uint16_t ss = exception ? _farpeekl(c->ss, st + 16) : c->ss;
    char buf[0x108];
    unsigned i, run = 0, lines = 0;

    trc("run286: the program trapped at %04x:%08x\n",
	    (uint16_t)_farpeekl(c->ss, st + 4), _farpeekl(c->ss, st));
    if (bp < sizeof(buf))
	return;
    for (i = 0; i < sizeof(buf); i++)
	buf[i] = _farpeekb(ss, bp - sizeof(buf) + i);
    /* whatever text is in there, a piece at a time */
    for (i = 0; i < sizeof(buf); i++) {
	if (buf[i] == '\n' || buf[i] == '\r' ||
		(buf[i] >= 0x20 && (unsigned char)buf[i] < 0x7f)) {
	    run++;
	    continue;
	}
	buf[i] = 0;
	if (run >= 6 && lines < 8) {
	    trc("run286:   %s\n", buf + i - run);
	    lines++;
	}
	run = 0;
    }
}

/*
 * Take the processor exceptions the program has not asked for. Without
 * this the host reflects them to whatever dj64 put there, which quietly
 * ends the process: the program disappears mid-frame with nothing said.
 * The ones a program hooks itself are left alone, and so are the two the
 * loader needs for itself.
 */
static __dpmi_paddr exc_prev[EXC_SLOTS];
static uint32_t exc_hooked;

static void hook_exceptions(void)
{
    unsigned i;

    for (i = 0; i < EXC_SLOTS; i++) {
	__dpmi_paddr pm;

	if (i == 1 || i == 3)		/* the debugger's own */
	    continue;
	if (__dpmi_get_processor_exception_handler_vector(i, &pm) == -1)
	    continue;
	exc_prev[i] = pm;
	pm.selector = gate_cs32;
	pm.offset32 = exc_stubs + i * EXC_SLOT_SIZE;
	if (__dpmi_set_processor_exception_handler_vector(i, &pm) == 0)
	    exc_hooked |= 1u << i;
    }
}

/*
 * Give the vectors back before we go. Our stubs live in memory the host
 * frees with us, and an exception taken on the way out lands on a
 * selector that no longer exists: dosemu2 says "CS selector invalid" and
 * takes the session down instead of letting the program exit.
 */
static void unhook_exceptions(void)
{
    unsigned i;

    for (i = 0; i < EXC_SLOTS; i++) {
	if (exc_hooked & (1u << i))
	    __dpmi_set_processor_exception_handler_vector(i, &exc_prev[i]);
    }
    exc_hooked = 0;
}

/*
 * Own int 21h in protected mode, ahead of the host.
 *
 * dj64 makes us a 32bit DPMI client, and a DPMI host is told the bitness
 * once, for the client as a whole. The program is 16bit: where DOS takes
 * a DS:DX it writes DX and leaves the high half of EDX as it found it, so
 * the host follows a pointer built half from the program and half from
 * whatever was there. The stub narrows the registers for calls made from
 * 16bit code and passes ours through untouched.
 */
static int int21_hooked;

static void hook_int21(void)
{
    __dpmi_paddr pm;

    if (__dpmi_get_protected_mode_interrupt_vector(0x21, &pm) == -1) {
	trc("run286: cannot read the int 21h vector, DOS calls from the "
		"program may get a stray high half of edx\n");
	return;
    }
    int21_prev[0] = pm.offset32 & 0xffff;
    int21_prev[1] = pm.offset32 >> 16;
    int21_prev[2] = pm.selector;
    pm.selector = gate_cs32;
    pm.offset32 = int21_stub;
    if (__dpmi_set_protected_mode_interrupt_vector(0x21, &pm) == -1) {
	trc("run286: cannot take the int 21h vector, DOS calls from the "
		"program may get a stray high half of edx\n");
	return;
    }
    int21_hooked = 1;
    trc("run286: int 21h through our stub, chaining to %04x:%08x\n",
	    int21_prev[2], (unsigned)(int21_prev[0] | (int21_prev[1] << 16)));
}

/* and back to whoever had it, for the same reason as the exceptions */
static void unhook_int21(void)
{
    __dpmi_paddr pm;

    if (!int21_hooked)
	return;
    pm.selector = int21_prev[2];
    pm.offset32 = int21_prev[0] | (int21_prev[1] << 16);
    __dpmi_set_protected_mode_interrupt_vector(0x21, &pm);
    int21_hooked = 0;
}

/* What the LDT alias says about one entry, for a fault that names it. */
static void dump_ldt_entry(const char *what, unsigned off)
{
    unsigned alias = gate_ldt_alias & 0xffff;
    unsigned char desc[8];
    char buf[32];
    char *p = buf;
    unsigned i;

    if (!alias || off + 8 > ldt_size) {
	trc("run286:   %s entry %#x is outside the table\n", what, off);
	return;
    }
    for (i = 0; i < 8; i++)
	p += sprintf(p, "%02x ", _farpeekb(alias, off + i));
    trc("run286:   %s entry %#x: %s\n", what, off, buf);
    /* the alias is what the program wrote; ask the host what it kept */
    if (__dpmi_get_descriptor((off | 7), desc) == 0) {
	for (i = 0, p = buf; i < 8; i++)
	    p += sprintf(p, "%02x ", desc[i]);
	trc("run286:   %s as the host has it: %s\n", what, buf);
    } else {
	trc("run286:   %s: the host will not show %#x\n", what, off | 7);
    }
}

/*
 * Called from _exc_common with our own stack under us. gate_exc_ss:esp
 * points at the two registers the stub saved, then the number it pushed,
 * then what the host put there: the address to return to, the error code
 * and the frame that faulted.
 */
void ASMCFUNC run286_exception(void)
{
    unsigned ss = gate_exc_ss;
    unsigned sp = gate_exc_esp;
    unsigned ds = _farpeekl(ss, sp);
    unsigned es = _farpeekl(ss, sp + 4);
    unsigned n = _farpeekl(ss, sp + 48);
    unsigned err = _farpeekl(ss, sp + 60);
    unsigned eip = _farpeekl(ss, sp + 64);
    unsigned cs = _farpeekl(ss, sp + 68);
    unsigned fl = _farpeekl(ss, sp + 72);
    unsigned esp = _farpeekl(ss, sp + 76);
    unsigned fss = _farpeekl(ss, sp + 80);
    char code[48];
    char *p = code;
    unsigned i;

    /* nothing may land on the host stack while we read it */
    __dpmi_get_and_disable_virtual_interrupt_state();
    {
	/* The frame the last interrupt stub saved on the host's stack:
	 * gs fs es ds, then pushal (edi esi ebp esp ebx edx ecx eax), then
	 * the vector and the host's eip cs eflags. If the program's BP came
	 * back as zero from an interrupt, it came from here. */
	static const char *nm[] = { "gs", "fs", "es", "ds", "edi", "esi",
		"ebp", "esp", "ebx", "edx", "ecx", "eax", "vec", "eip", "cs",
		"efl" };
	unsigned k, a = int_last_esp;

	trc("run286:   last int frame at %04x:%08x:", (uint16_t)int_last_ss, a);
	for (k = 0; k < 16; k++)
	    trc(" %s %08x", nm[k], _farpeekl(int_last_ss, a + k * 4));
	trc("\n");
	for (a = (int_last_esp & ~0xfu) - 0x80; a < (int_last_esp & ~0xfu) + 0x80;
		a += 0x10)
	    trc("run286:   %04x:%08x: %08x %08x %08x %08x\n",
		    (uint16_t)int_last_ss, a, _farpeekl(int_last_ss, a),
		    _farpeekl(int_last_ss, a + 4), _farpeekl(int_last_ss, a + 8),
		    _farpeekl(int_last_ss, a + 12));
    }
    trc("run286: exception %#x at %04x:%08x, error %#x, flags %#x\n",
	    n, (uint16_t)cs, eip, err, fl);
    trc("run286:   its stack %04x:%08x, ours %04x:%08x, calls served %u\n",
	    (uint16_t)fss, esp, (uint16_t)ss, sp, ldr.ncall);
    /* pushal order, from the lowest address up */
    trc("run286:   edi %08x esi %08x ebp %08x ebx %08x\n",
	    _farpeekl(ss, sp + 16), _farpeekl(ss, sp + 20),
	    _farpeekl(ss, sp + 24), _farpeekl(ss, sp + 32));
    trc("run286:   edx %08x ecx %08x eax %08x\n", _farpeekl(ss, sp + 36),
	    _farpeekl(ss, sp + 40), _farpeekl(ss, sp + 44));
    trc("run286:   ds %04x es %04x\n", (uint16_t)ds, (uint16_t)es);
    for (i = 0; i < 12; i++)
	p += sprintf(p, "%02x ", _farpeekb(cs, eip + i));
    trc("run286:   code at the fault: %s\n", code);
    /* the bytes in front of the faulting one, to say which routine it is */
    for (i = 0, p = code; i < 12; i++)
	p += sprintf(p, "%02x ", _farpeekb(cs, eip - 12 + i));
    trc("run286:   code before it: %s\n", code);
    for (i = 0, p = code; i < 8; i++)
	p += sprintf(p, "%04x ", _farpeekw(fss, esp + i * 2));
    trc("run286:   its stack holds: %s\n", code);
    /* and the start of that stack: a stack that ran out reads as used up
     * to where it stopped, one that was never set up reads as zeroes */
    for (i = 0, p = code; i < 8; i++)
	p += sprintf(p, "%04x ", _farpeekw(fss, i * 2));
    trc("run286:   stack segment starts: %s\n", code);
    /* a selector fault names the entry; show it and the one in es, as
     * the programs build these themselves through the LDT alias */
    if (n == 0x0a || n == 0x0b || n == 0x0c || n == 0x0d)
	dump_ldt_entry("blamed", err & 0xfff8);
    dump_ldt_entry("es", es & 0xfff8);
    dump_ldt_entry("ss", fss & 0xfff8);
    trc("run286:   %u interrupts taken, the last in slot %u on stack %04x:%08x\n",
	    int_taken, int_last, (uint16_t)int_last_ss, int_last_esp);
    ss_report("at the fault");
    rr_report();
    int_depth_report();
    {
	/* Is the stack itself still there, or only SS:SP lost? Say where
	 * its live data starts and show the words there. */
	unsigned lim = __dpmi_get_segment_limit(fss);
	unsigned off, live = 0;
	char b[3 * 16 + 1], *q;
	int i;

	for (off = 0; off < lim && off < 0x10000; off += 2) {
	    if (_farpeekw(fss, off)) {
		live = off;
		break;
	    }
	}
	trc("run286:   stack %04x limit %04x, first non-zero word at %04x\n",
		fss, lim, live);
	for (off = live & ~0xfu; off < (live & ~0xfu) + 0x40 && off < lim;
		off += 0x10) {
	    q = b;
	    for (i = 0; i < 8; i++)
		q += sprintf(q, "%04x ", _farpeekw(fss, off + i * 2));
	    trc("run286:   %04x: %s\n", off, b);
	}
	/*
	 * The frame the faulting function was really using. Its BP came
	 * back as zero, but the registers around it still point into the
	 * live part of the stack, so show what is there: if the saved BP
	 * is still in its slot, the frame was never touched and only the
	 * register was lost.
	 */
	{
	    unsigned dx = _farpeekl(ss, sp + 36) & 0xffff;
	    unsigned base = dx > 0x40 ? (dx - 0x40) & ~0xfu : 0;

	    trc("run286:   around dx %04x:\n", dx);
	    for (off = base; off < base + 0xa0 && off < lim; off += 0x10) {
		q = b;
		for (i = 0; i < 8; i++)
		    q += sprintf(q, "%04x ", _farpeekw(fss, off + i * 2));
		trc("run286:   %04x: %s\n", off, b);
	    }
	    trc("run286:   the top of it:\n");
	    for (off = (lim - 0x40) & ~0xfu; off + 15 < lim; off += 0x10) {
		q = b;
		for (i = 0; i < 8; i++)
		    q += sprintf(q, "%04x ", _farpeekw(fss, off + i * 2));
		trc("run286:   %04x: %s\n", off, b);
	    }
	}
    }
    gate_exit_code = 1;
}

/* Called from gate_entry once the program enters a stub. Returns nonzero to
 * unwind ne_enter() instead of resuming the program. */
int ASMCFUNC run286_import(void)
{
    unsigned n = gate_index;
    const struct import *im;
    struct call c;
    uint16_t rc;

    c.ss = gate_cli_ss;
    c.sp = gate_cli_esp;
    gate_depth++;
    if (gate_depth > 1) {
	gate_nested++;
	if (gate_depth > gate_deepest)
	    gate_deepest = gate_depth;
    }
    ss_note(c.ss, c.sp);
    if (++ss_ticks % 2048 == 0)
	ss_report("so far");
    if (n == TRAP_IDX || n == TRAP_EXC_IDX) {
	/* an interrupt frame is IP, CS, flags; an exception frame has a
	 * far return and an error code in front of it */
	report_trap(&c, n == TRAP_EXC_IDX);
	gate_exit_code = 1;
	return 1;
    }
    if (n >= ldr.nstub) {
	trc("run286: bogus import index %u\n", n);
	gate_exit_code = 1;
	return 1;
    }
    im = &ldr.imp[n];
    if (!im->fn) {
	char nm[72];

	if (im->name[0])
	    snprintf(nm, sizeof(nm), "%s", im->name);
	else
	    snprintf(nm, sizeof(nm), "#%u", im->ord);
	trc("run286: unimplemented %s.%s, called from %04x:%04x\n",
		im->mod, nm, _farpeekw(c.ss, c.sp + CALL_ARGS - 2),
		_farpeekw(c.ss, c.sp + CALL_ARGS - 4));
	trc("run286:   stack %04x %04x %04x %04x %04x %04x %04x %04x\n",
		call_argw(&c, 0), call_argw(&c, 2), call_argw(&c, 4),
		call_argw(&c, 6), call_argw(&c, 8), call_argw(&c, 10),
		call_argw(&c, 12), call_argw(&c, 14));
	gate_exit_code = 1;
	return 1;
    }

    if (ldr.trace)
	trc("run286: -> %s.%s%u\n", im->mod,
		im->name[0] ? im->name : "#", im->ord);
    /*
     * What the program had in BP and where it called from, as the gate
     * saved them on its own stack. If either has moved by the time the
     * call is done, something wrote over the frame while we were away,
     * and that is the thing to find.
     */
    {
	unsigned bp0 = _farpeekw(c.ss, c.sp + CALL_EBP);
	unsigned rcs = _farpeekw(c.ss, c.sp + CALL_ARGS - 2);
	unsigned rip = _farpeekw(c.ss, c.sp + CALL_ARGS - 4);

	rc = im->fn->fn(&c);
	if (_farpeekw(c.ss, c.sp + CALL_ARGS - 2) != rcs ||
		_farpeekw(c.ss, c.sp + CALL_ARGS - 4) != rip)
	    trc("run286: FRAME the caller of %s.%s%u was %04x:%04x and now "
		    "reads %04x:%04x\n", im->mod,
		    im->name[0] ? im->name : "#", im->ord, rcs, rip,
		    _farpeekw(c.ss, c.sp + CALL_ARGS - 2),
		    _farpeekw(c.ss, c.sp + CALL_ARGS - 4));
	else if (_farpeekw(c.ss, c.sp + CALL_EBP) != bp0 && !frame_bp_ok)
	    trc("run286: FRAME bp of the caller of %s.%s%u was %04x and now "
		    "reads %04x\n", im->mod,
		    im->name[0] ? im->name : "#", im->ord, bp0,
		    _farpeekw(c.ss, c.sp + CALL_EBP));
	frame_bp_ok = 0;
    }
    ldr.ncall++;
    if (ldr.trace)
	trc("run286: %s.%s%u(%04x %04x %04x %04x %04x %04x %04x) = %u\n",
		im->mod, im->name[0] ? im->name : "#", im->ord,
		call_argw(&c, 0), call_argw(&c, 2), call_argw(&c, 4),
		call_argw(&c, 6), call_argw(&c, 8), call_argw(&c, 10),
		call_argw(&c, 12), rc);
    if (ldr.trace)
	trc("run286:   called from %04x:%04x, stack %04x:%04x bp %04x "
		"si %04x di %04x\n",
		_farpeekw(c.ss, c.sp + CALL_ARGS - 2),
		_farpeekw(c.ss, c.sp + CALL_ARGS - 4),
		(uint16_t)c.ss, (uint16_t)(c.sp + CALL_ARGS),
		_farpeekw(c.ss, c.sp + CALL_EBP),
		_farpeekw(c.ss, c.sp + CALL_ESI),
		_farpeekw(c.ss, c.sp + CALL_EDI));
    /* A program that sits in a poll loop makes the trace one repeated line
     * and says nothing about whether its interrupt handlers still run, which
     * is the first thing to ask when it stops moving. Say so now and then. */
    if (ldr.trace && ldr.ncall % 512 == 0)
	trace_interrupts("so far");
    /* the result goes back in AX, which gate_entry pops off the program's
     * own stack on the way out */
    _farpokew(c.ss, c.sp + CALL_EAX, rc);
    gate_depth--;
    return 0;
}

/*
 * An NE program is entered with AX holding a selector for its environment
 * segment, BX the offset of the command line in it and CX the size of the
 * automatic data segment. BioForge stores AX straight into its PSP and
 * then walks the strings, so the environment has to be a real one: a list
 * of NUL terminated strings, an empty string to end it, a word of 1 and
 * the program's own path.
 */
static uint16_t env_init(const char *path)
{
    static const char vars[] = "PATH=\0";
    int sel, para, off;

    para = __dpmi_allocate_dos_memory(16, &sel);
    if (para == -1)
	return 0;
    for (off = 0; off < (int)sizeof(vars); off++)
	_farpokeb(sel, off, vars[off]);
    _farpokew(sel, off, 1);
    off += 2;
    while (*path)
	_farpokeb(sel, off++, *path++);
    _farpokeb(sel, off++, 0);
    _farpokeb(sel, off, 0);		/* an empty command line after it */
    return sel;
}

static int stub_seg_init(struct dos_ldr *l, unsigned nimp)
{
    __dpmi_paddr h;
    uint32_t trap;

    /* two slots past the imports for the int 3 stubs below */
    l->stub_mem.size = (nimp + 2) * STUB_SIZE;
    if (__dpmi_allocate_memory(&l->stub_mem) == -1) {
	trc("run286: cannot allocate the stub segment\n");
	return -1;
    }
    l->stub_code_sel = __dpmi_allocate_ldt_descriptors(2);
    if (l->stub_code_sel == (uint16_t)-1) {
	trc("run286: cannot allocate the stub descriptors\n");
	return -1;
    }
    l->stub_data_sel = l->stub_code_sel + 8;
    if (__dpmi_set_segment_base_address(l->stub_code_sel,
		l->stub_mem.address) == -1 ||
	    __dpmi_set_segment_limit(l->stub_code_sel,
		l->stub_mem.size - 1) == -1 ||
	    __dpmi_set_descriptor_access_rights(l->stub_code_sel,
		AR_CODE16) == -1 ||
	    __dpmi_set_segment_base_address(l->stub_data_sel,
		l->stub_mem.address) == -1 ||
	    __dpmi_set_segment_limit(l->stub_data_sel,
		l->stub_mem.size - 1) == -1 ||
	    __dpmi_set_descriptor_access_rights(l->stub_data_sel,
		AR_DATA16) == -1) {
	trc("run286: cannot set up the stub descriptors\n");
	return -1;
    }

    gate_ds32 = _my_ds();
    gate_stk_ss = _my_ds();
    gate_stk_esp = gate_stack_end;
    gate_exc_stk_ss = _my_ds();
    gate_exc_stk_esp = exc_stack_end;
    h.selector = _my_cs();
    h.offset32 = gate_entry;
    if (__dpmi_set_protected_mode_interrupt_vector(GATE_INT, &h) == -1) {
	trc("run286: cannot hook int %#x\n", GATE_INT);
	return -1;
    }

    /* A program that traps would otherwise take the host down with it, and
     * these ones trap on purpose: Origin's error() formats its message on
     * the stack, prints it and executes int 3. Route the trap through a
     * stub of the same shape as an import so it arrives in C, where the
     * message is still there to be read. */
    trap = nimp * STUB_SIZE;
    _farpokeb(l->stub_data_sel, trap + 0, 0xb8);
    _farpokew(l->stub_data_sel, trap + 1, TRAP_IDX);
    _farpokeb(l->stub_data_sel, trap + 3, 0xcd);
    _farpokeb(l->stub_data_sel, trap + 4, GATE_INT);
    _farpokeb(l->stub_data_sel, trap + 5, 0xcf);	/* iret, never reached */
    h.selector = l->stub_code_sel;
    h.offset32 = trap;
    if (__dpmi_set_protected_mode_interrupt_vector(3, &h) == -1) {
	trc("run286: cannot hook int 3\n");
	return -1;
    }
    /* a host may deliver it as an exception instead, on its own frame */
    trap += STUB_SIZE;
    _farpokeb(l->stub_data_sel, trap + 0, 0xb8);
    _farpokew(l->stub_data_sel, trap + 1, TRAP_EXC_IDX);
    _farpokeb(l->stub_data_sel, trap + 3, 0xcd);
    _farpokeb(l->stub_data_sel, trap + 4, GATE_INT);
    _farpokeb(l->stub_data_sel, trap + 5, 0xcb);	/* never reached */
    h.offset32 = trap;
    if (__dpmi_set_processor_exception_handler_vector(3, &h) == -1)
	trc("run286: cannot hook exception 3\n");
    return 0;
}

static uint8_t *slurp(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long sz;

    if (!f)
	return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(sz);
    if (!buf || fread(buf, 1, sz, f) != (size_t)sz) {
	free(buf);
	fclose(f);
	return NULL;
    }
    fclose(f);
    *size = sz;
    return buf;
}

/* dosemu2 rewrites the command line when it loads us as a bare ELF, and
 * dj64's getenv() sees the DOS environment, so as a last resort take the
 * image name from a one-line RUN286.CFG next to it. */
static char *read_cfg(char *buf)
{
    FILE *f = fopen("RUN286.CFG", "r");
    char *p;

    if (!f)
	return NULL;
    p = fgets(buf, 128, f);
    fclose(f);
    if (!p)
	return NULL;
    p = strpbrk(buf, "\r\n");
    if (p)
	*p = 0;
    return buf[0] ? buf : NULL;
}

/*
 * Second line of RUN286.CFG, if any, turns tracing on; a third line names
 * the file the trace goes to, which is the only way to read it when the
 * program has taken the screen into a graphics mode. Returns the name in
 * *logp, or leaves it alone.
 */
static int read_cfg_trace(char *logp, size_t logsz)
{
    FILE *f = fopen("RUN286.CFG", "r");
    char buf[128];
    int n = 0;

    if (!f)
	return 0;
    while (fgets(buf, sizeof(buf), f)) {
	char *p;

	n++;
	if (n != 3)
	    continue;
	p = strpbrk(buf, "\r\n");
	if (p)
	    *p = 0;
	if (buf[0])
	    snprintf(logp, logsz, "%s", buf);
    }
    fclose(f);
    return n > 1;
}

/* Low address space held back while the image is allocated, so that the
 * program's own arena can have it instead. */
#define LOW_RESERVE	(12 * 1024 * 1024)

static int load_segments(struct module *m)
{
    const struct ne_image *ne = &m->ne;
    const uint8_t *file = m->file;
    __dpmi_meminfo hole = {};
    unsigned long total = 0, off;
    int i, ret;

    /* A 16bit selector cannot reach past 64K, so give every segment that
     * much room: DosReallocSeg() then only ever moves a limit, and nothing
     * the program holds a pointer into has to be copied anywhere. */
    total = (unsigned long)ne->cseg * SEG_STRIDE;

    /*
     * Where the image lands decides how much room is left below it, and
     * the programs care: BioForge builds its arena out of blocks that end
     * below linear 30Mb and throws away everything else. A host hands out
     * address space in the order it is asked for, so take the low ground
     * first, put the image above it, and give it straight back; the arena
     * then gets the space the image would have stood on. A host with
     * nothing to spare down there just says no, and we load where we
     * would have loaded anyway.
     */
    hole.size = LOW_RESERVE;
    if (__dpmi_allocate_memory(&hole) == -1)
	hole.size = 0;
    m->mem.size = total;
    ret = __dpmi_allocate_memory(&m->mem);
    if (hole.size)
	__dpmi_free_memory(hole.handle);
    if (ret == -1) {
	trc("run286: cannot allocate %lu bytes of DPMI memory\n", total);
	return -1;
    }
    m->base_sel = __dpmi_allocate_ldt_descriptors(ne->cseg);
    if (m->base_sel == (uint16_t)-1) {
	trc("run286: cannot allocate %u descriptors\n", ne->cseg);
	return -1;
    }
    trc("run286: %s: %lu bytes at linear %#lx, %u selectors from %#x\n",
	    m->name, total, (unsigned long)m->mem.address, ne->cseg,
	    m->base_sel);

    off = 0;
    for (i = 0; i < ne->cseg; i++) {
	struct ne_seg *sg = &ne->seg[i];
	struct seg_info *si = &m->seg[i];
	uint32_t len = ne_seg_len(sg);

	si->size = sg->minalloc;
	si->lin = m->mem.address + off;
	si->sel = m->base_sel + i * 8;
	off += SEG_STRIDE;

	/* everything is a writable data segment while we fill it in */
	if (__dpmi_set_segment_base_address(si->sel, si->lin) == -1 ||
		__dpmi_set_segment_limit(si->sel, si->size - 1) == -1 ||
		__dpmi_set_descriptor_access_rights(si->sel, AR_DATA16) == -1) {
	    trc("run286: cannot set up descriptor %#x for segment %d\n",
		    si->sel, i + 1);
	    return -1;
	}
	si->shadow = calloc(1, si->size);
	if (!si->shadow) {
	    trc("run286: out of memory for segment %d\n", i + 1);
	    return -1;
	}
	if (sg->file_off)
	    memcpy(si->shadow, file + sg->file_off, len);
    }
    return 0;
}

static int commit_segments(struct module *m)
{
    const struct ne_image *ne = &m->ne;
    __dpmi_paddr dst = {};
    int i;

    for (i = 0; i < ne->cseg; i++) {
	struct seg_info *si = &m->seg[i];

	dst.selector = si->sel;
	dst.offset32 = 0;
	fmemcpy1(dst, si->shadow, si->size);
	free(si->shadow);
	si->shadow = NULL;
	if (!(ne->seg[i].flags & NE_SEG_DATA) &&
		__dpmi_set_descriptor_access_rights(si->sel, AR_CODE16) == -1) {
	    trc("run286: cannot make segment %d executable\n", i + 1);
	    return -1;
	}
    }
    return 0;
}

/*
 * A restubbed game carries the whole bound file inside our own executable,
 * after the stub and its overlays, with the offset in an eight byte
 * trailer at the very end. Then there is nothing to name on the command
 * line: the file we are is the game. See restub286.c.
 */
#define SELF_MAGIC	"R286"

static uint8_t *slurp_self(const char *self, size_t *size)
{
    uint8_t trailer[8];
    uint32_t off;
    long end;
    uint8_t *buf;
    FILE *f = fopen(self, "rb");

    if (!f)
	return NULL;
    if (fseek(f, -(long)sizeof(trailer), SEEK_END) != 0 ||
	    fread(trailer, 1, sizeof(trailer), f) != sizeof(trailer) ||
	    memcmp(trailer, SELF_MAGIC, 4) != 0) {
	fclose(f);
	return NULL;
    }
    end = ftell(f) - (long)sizeof(trailer);
    off = trailer[4] | ((uint32_t)trailer[5] << 8) |
	    ((uint32_t)trailer[6] << 16) | ((uint32_t)trailer[7] << 24);
    if (end <= 0 || off >= (uint32_t)end) {
	fclose(f);
	return NULL;
    }
    *size = end - off;
    buf = malloc(*size);
    if (!buf || fseek(f, off, SEEK_SET) != 0 ||
	    fread(buf, 1, *size, f) != *size) {
	free(buf);
	fclose(f);
	return NULL;
    }
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    /* When dj64 loads us as a bare ELF, dosemu2 replaces the command line
     * with its own ("elfload2 0"), so the image to run comes from the
     * environment instead. A stubbed run286.exe will get a real argv. */
    const char *path = getenv("RUN286_IMAGE");
    struct dos_ldr *l = &ldr;
    struct module *m = &l->mod[0];
    struct pl_bound b;
    struct ne_ldr_ops ops = {};
    struct ne_reloc_stats st = {};
    const char *err = "";
    char cfg[128];
    char logf[128] = "";
    uint8_t *file, *self = NULL;
    size_t size;
    unsigned entry_seg, ss_seg, sp;
    int i, rc;

    if (!path && argc > 1 && argv[1][0] && strcmp(argv[1], "0") != 0 &&
	    strcmp(argv[1], "1") != 0)
	path = argv[1];
    if (!path && argc > 0 && argv[0] && argv[0][0]) {
	self = slurp_self(argv[0], &size);
	if (self)
	    path = argv[0];
    }
    if (!path)
	path = read_cfg(cfg);
    if (!path) {
	trc("run286: no image given; set RUN286_IMAGE or write RUN286.CFG\n");
	return 2;
    }

    l->trace = getenv("RUN286_TRACE") != NULL || read_cfg_trace(logf,
	    sizeof(logf));
    run286_trace = l->trace;
    trc_init_indos();
    if (l->trace) {
	const char *log = getenv("RUN286_LOG");

	if (!log && logf[0])
	    log = logf;
	if (log)
	    trace_fp = fopen(log, "w");
    }
    trc("run286: loading %s\n", path);
    snprintf(m->name, sizeof(m->name), "%s", "PROGRAM");
    file = m->file = self ?: slurp(path, &size);
    if (!file) {
	trc("run286: cannot read %s\n", path);
	return 2;
    }
    if (pl_bound_parse(&b, file, size, &err) != 0) {
	trc("run286: %s\n", err);
	return 1;
    }
    if (ne_parse(&m->ne, file, size, b.app_off, &err) != 0) {
	trc("run286: %s\n", err);
	return 1;
    }
    trc("run286: NE at %#x, %u segments, entry %04x:%04x\n", b.app_off,
	    m->ne.cseg, (unsigned)(m->ne.csip >> 16),
	    (unsigned)(m->ne.csip & 0xffff));

    l->nmod = 1;
    m->seg = calloc(m->ne.cseg, sizeof(*m->seg));
    if (!m->seg || load_segments(m) != 0)
	return 1;
    if (stub_seg_init(l, MAX_IMPORTS) != 0)
	return 1;

    ops.ctx = m;
    ops.seg_addr = dos_seg_addr;
    ops.seg_mem = dos_seg_mem;
    ops.resolve_ord = dos_resolve_ord;
    ops.resolve_name = dos_resolve_name;
    for (i = 1; i <= m->ne.cseg; i++) {
	if (ne_relocate(&m->ne, i, &ops, &st, &err) != 0) {
	    trc("run286: segment %d: %s\n", i, err);
	    return 1;
	}
    }
    trc("run286: %u fixups, %u locations, %u unresolved, %u import stubs\n",
	    st.applied, st.patched, st.unresolved, l->nstub);
    if (commit_segments(m) != 0)
	return 1;

    entry_seg = m->ne.csip >> 16;
    ss_seg = m->ne.sssp >> 16;
    sp = m->ne.sssp & 0xffff;
    if (!ss_seg)			/* SS follows the automatic data segment */
	ss_seg = m->ne.autodata;
    if (!entry_seg || entry_seg > m->ne.cseg || !ss_seg || ss_seg > m->ne.cseg ||
	    !m->ne.autodata || m->ne.autodata > m->ne.cseg) {
	trc("run286: bad entry %08x, stack %08x or autodata %u\n",
		m->ne.csip, m->ne.sssp, m->ne.autodata);
	return 1;
    }
    if (!sp)				/* top of the stack segment */
	sp = m->seg[ss_seg - 1].size;
    desc_probe();
    trc("run286: gdt %04x:%04x%04x, idt %04x:%04x%04x\n",
	    gate_gdt[0], gate_gdt[2], gate_gdt[1],
	    gate_idt[0], gate_idt[2], gate_idt[1]);
    {
	unsigned int ldt_base = 0;
	unsigned alias = gate_ldt_alias & 0xffff;

	if (alias)
	    __dpmi_get_segment_base_address(alias, &ldt_base);
	ldt_lin = ldt_base;
	ldt_sel_reg = gate_ldt_sel & 0xffff;
	/*
	 * The limit of the alias is not the size of the table: dosemu2
	 * grows it as descriptors get allocated and starts it at a few
	 * pages. Origin's wrapper reads that limit once and takes it for
	 * how many selectors exist, so it sized its pool at 2560 entries
	 * and BioForge ran out of them halfway through a level with
	 * "RESOURCE.C 170". Ask for the table up front; writes past the
	 * current limit fault on the alias page and dosemu2 grows it
	 * there (msdos_ldt_fault), so nothing else has to change.
	 */
	if (alias)
	    __dpmi_set_segment_limit(alias, LDT_FULL_SIZE - 1);
	ldt_size = alias ? __dpmi_get_segment_limit(alias) + 1 : 0;
	trc("run286: ldtr %04x, ldt alias %04x at %#x limit %#x\n",
		gate_ldt_sel & 0xffff, alias, ldt_base,
		ldt_size ? ldt_size - 1 : 0);
    }
    if (gate_thunk_err)
	trc("run286: no THUNK_16_32x, narrowing DOS calls ourselves\n");
    hook_exceptions();
    hook_int21();
    /* what a handler of the program's would have found in DS and ES had
     * it interrupted the program rather than us */
    int_ds = m->seg[m->ne.autodata - 1].sel;
    trc("run286: entering %04x:%04x, stack %04x:%04x, ds %04x\n",
	    m->seg[entry_seg - 1].sel, (unsigned)(m->ne.csip & 0xffff),
	    m->seg[ss_seg - 1].sel, sp, m->seg[m->ne.autodata - 1].sel);
    fflush(stdout);
    rc = ne_enter(m->seg[entry_seg - 1].sel, m->ne.csip & 0xffff,
	    m->seg[ss_seg - 1].sel, sp,
	    m->seg[m->ne.autodata - 1].sel, m->seg[m->ne.autodata - 1].sel,
	    env_init(path), m->seg[m->ne.autodata - 1].size);
    trc("run286: back from the program after %u API calls, rc %d\n",
	    l->ncall, rc);
    trace_interrupts("taken");
    int_depth_report();
    unhook_exceptions();
    unhook_int21();
    ne_free(&m->ne);
    return 0;
}
