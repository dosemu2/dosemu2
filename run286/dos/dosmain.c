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

void trc(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (trace_fp) {
	vfprintf(trace_fp, fmt, ap);
	fflush(trace_fp);
    } else {
	vprintf(fmt, ap);
    }
    va_end(ap);
}

/* access rights for a 16bit, DPL 3, present segment */
#define AR_CODE16	0x00fa		/* code, readable */
#define AR_DATA16	0x00f2		/* data, writable */

#define GATE_INT	0x66		/* free vector the stubs go through */
#define TRAP_IDX	0xffff		/* the stub int 3 goes through */
#define TRAP_EXC_IDX	0xfffe		/* the same, as a DPMI exception */
#define STUB_SIZE	8		/* one import stub, see make_stub() */
#define MAX_IMPORTS	1024
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

struct dos_ldr {
    const struct ne_image *ne;
    struct seg_info *seg;
    __dpmi_meminfo mem;
    uint16_t base_sel;
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

static int dos_seg_addr(void *ctx, uint16_t segnum, struct ne_far *a)
{
    struct dos_ldr *l = ctx;

    if (!segnum || segnum > l->ne->cseg)
	return -1;
    a->sel = l->seg[segnum - 1].sel;
    a->off = 0;
    return 0;
}

static uint8_t *dos_seg_mem(void *ctx, uint16_t segnum, uint32_t *size)
{
    struct dos_ldr *l = ctx;

    if (!segnum || segnum > l->ne->cseg)
	return NULL;
    *size = l->seg[segnum - 1].size;
    return l->seg[segnum - 1].shadow;
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

static int dos_resolve_ord(void *ctx, const char *mod, uint16_t ord,
	struct ne_far *a)
{
    return make_stub(ctx, mod, NULL, ord, a);
}

static int dos_resolve_name(void *ctx, const char *mod, const char *name,
	struct ne_far *a)
{
    return make_stub(ctx, mod, name, 0, a);
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
static void hook_exceptions(void)
{
    unsigned i;

    for (i = 0; i < EXC_SLOTS; i++) {
	__dpmi_paddr pm;

	if (i == 1 || i == 3)		/* the debugger's own */
	    continue;
	if (__dpmi_get_processor_exception_handler_vector(i, &pm) == -1)
	    continue;
	pm.selector = gate_cs32;
	pm.offset32 = exc_stubs + i * EXC_SLOT_SIZE;
	__dpmi_set_processor_exception_handler_vector(i, &pm);
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
    for (i = 0, p = code; i < 8; i++)
	p += sprintf(p, "%04x ", _farpeekw(fss, esp + i * 2));
    trc("run286:   its stack holds: %s\n", code);
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
    rc = im->fn->fn(&c);
    ldr.ncall++;
    if (ldr.trace)
	trc("run286: %s.%s%u(%04x %04x %04x %04x %04x %04x %04x) = %u\n",
		im->mod, im->name[0] ? im->name : "#", im->ord,
		call_argw(&c, 0), call_argw(&c, 2), call_argw(&c, 4),
		call_argw(&c, 6), call_argw(&c, 8), call_argw(&c, 10),
		call_argw(&c, 12), rc);
    if (ldr.trace)
	trc("run286:   called from %04x:%04x\n",
		_farpeekw(c.ss, c.sp + CALL_ARGS - 2),
		_farpeekw(c.ss, c.sp + CALL_ARGS - 4));
    /* the result goes back in AX, which gate_entry pops off the program's
     * own stack on the way out */
    _farpokew(c.ss, c.sp + CALL_EAX, rc);
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

static int load_segments(struct dos_ldr *l, const uint8_t *file)
{
    const struct ne_image *ne = l->ne;
    unsigned long total = 0, off;
    int i;

    /* A 16bit selector cannot reach past 64K, so give every segment that
     * much room: DosReallocSeg() then only ever moves a limit, and nothing
     * the program holds a pointer into has to be copied anywhere. */
    total = (unsigned long)ne->cseg * SEG_STRIDE;

    l->mem.size = total;
    if (__dpmi_allocate_memory(&l->mem) == -1) {
	trc("run286: cannot allocate %lu bytes of DPMI memory\n", total);
	return -1;
    }
    l->base_sel = __dpmi_allocate_ldt_descriptors(ne->cseg);
    if (l->base_sel == (uint16_t)-1) {
	trc("run286: cannot allocate %u descriptors\n", ne->cseg);
	return -1;
    }
    trc("run286: %lu bytes at linear %#lx, %u selectors from %#x\n",
	    total, (unsigned long)l->mem.address, ne->cseg, l->base_sel);

    off = 0;
    for (i = 0; i < ne->cseg; i++) {
	struct ne_seg *sg = &ne->seg[i];
	struct seg_info *si = &l->seg[i];
	uint32_t len = ne_seg_len(sg);

	si->size = sg->minalloc;
	si->lin = l->mem.address + off;
	si->sel = l->base_sel + i * 8;
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

static int commit_segments(struct dos_ldr *l)
{
    const struct ne_image *ne = l->ne;
    __dpmi_paddr dst = {};
    int i;

    for (i = 0; i < ne->cseg; i++) {
	struct seg_info *si = &l->seg[i];

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

int main(int argc, char **argv)
{
    /* When dj64 loads us as a bare ELF, dosemu2 replaces the command line
     * with its own ("elfload2 0"), so the image to run comes from the
     * environment instead. A stubbed run286.exe will get a real argv. */
    const char *path = getenv("RUN286_IMAGE");
    struct dos_ldr *l = &ldr;
    struct pl_bound b;
    struct ne_image ne;
    struct ne_ldr_ops ops = {};
    struct ne_reloc_stats st = {};
    const char *err = "";
    char cfg[128];
    char logf[128] = "";
    uint8_t *file;
    size_t size;
    unsigned entry_seg, ss_seg, sp;
    int i, rc;

    if (!path && argc > 1 && argv[1][0] && strcmp(argv[1], "0") != 0 &&
	    strcmp(argv[1], "1") != 0)
	path = argv[1];
    if (!path)
	path = read_cfg(cfg);
    if (!path) {
	trc("run286: no image given; set RUN286_IMAGE or write RUN286.CFG\n");
	return 2;
    }

    l->trace = getenv("RUN286_TRACE") != NULL || read_cfg_trace(logf,
	    sizeof(logf));
    run286_trace = l->trace;
    if (l->trace) {
	const char *log = getenv("RUN286_LOG");

	if (!log && logf[0])
	    log = logf;
	if (log)
	    trace_fp = fopen(log, "w");
    }
    trc("run286: loading %s\n", path);
    file = slurp(path, &size);
    if (!file) {
	trc("run286: cannot read %s\n", path);
	return 2;
    }
    if (pl_bound_parse(&b, file, size, &err) != 0) {
	trc("run286: %s\n", err);
	return 1;
    }
    if (ne_parse(&ne, file, size, b.app_off, &err) != 0) {
	trc("run286: %s\n", err);
	return 1;
    }
    trc("run286: NE at %#x, %u segments, entry %04x:%04x\n", b.app_off,
	    ne.cseg, (unsigned)(ne.csip >> 16), (unsigned)(ne.csip & 0xffff));

    l->ne = &ne;
    l->seg = calloc(ne.cseg, sizeof(*l->seg));
    if (!l->seg || load_segments(l, file) != 0)
	return 1;
    if (stub_seg_init(l, MAX_IMPORTS) != 0)
	return 1;

    ops.ctx = l;
    ops.seg_addr = dos_seg_addr;
    ops.seg_mem = dos_seg_mem;
    ops.resolve_ord = dos_resolve_ord;
    ops.resolve_name = dos_resolve_name;
    for (i = 1; i <= ne.cseg; i++) {
	if (ne_relocate(&ne, i, &ops, &st, &err) != 0) {
	    trc("run286: segment %d: %s\n", i, err);
	    return 1;
	}
    }
    trc("run286: %u fixups, %u locations, %u unresolved, %u import stubs\n",
	    st.applied, st.patched, st.unresolved, l->nstub);
    if (commit_segments(l) != 0)
	return 1;

    entry_seg = ne.csip >> 16;
    ss_seg = ne.sssp >> 16;
    sp = ne.sssp & 0xffff;
    if (!ss_seg)			/* SS follows the automatic data segment */
	ss_seg = ne.autodata;
    if (!entry_seg || entry_seg > ne.cseg || !ss_seg || ss_seg > ne.cseg ||
	    !ne.autodata || ne.autodata > ne.cseg) {
	trc("run286: bad entry %08x, stack %08x or autodata %u\n",
		ne.csip, ne.sssp, ne.autodata);
	return 1;
    }
    if (!sp)				/* top of the stack segment */
	sp = l->seg[ss_seg - 1].size;
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
	ldt_size = alias ? __dpmi_get_segment_limit(alias) + 1 : 0;
	trc("run286: ldtr %04x, ldt alias %04x at %#x limit %#x\n",
		gate_ldt_sel & 0xffff, alias, ldt_base,
		ldt_size ? ldt_size - 1 : 0);
    }
    if (gate_thunk_err)
	trc("run286: no THUNK_16_32x, DOS calls from the program may "
		"get a stray high half of edx\n");
    hook_exceptions();
    trc("run286: entering %04x:%04x, stack %04x:%04x, ds %04x\n",
	    l->seg[entry_seg - 1].sel, (unsigned)(ne.csip & 0xffff),
	    l->seg[ss_seg - 1].sel, sp, l->seg[ne.autodata - 1].sel);
    fflush(stdout);
    rc = ne_enter(l->seg[entry_seg - 1].sel, ne.csip & 0xffff,
	    l->seg[ss_seg - 1].sel, sp,
	    l->seg[ne.autodata - 1].sel, l->seg[ne.autodata - 1].sel,
	    env_init(path), l->seg[ne.autodata - 1].size);
    trc("run286: back from the program after %u API calls, rc %d\n",
	    l->ncall, rc);
    ne_free(&ne);
    return 0;
}
