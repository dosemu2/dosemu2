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
#include <stdlib.h>
#include <string.h>
#include <dpmi.h>
#include <sys/fmemcpy.h>
#include <sys/segments.h>
#include <sys/farptr.h>
#include "neload.h"
#include "asm.h"

/* access rights for a 16bit, DPL 3, present segment */
#define AR_CODE16	0x00fa		/* code, readable */
#define AR_DATA16	0x00f2		/* data, writable */

#define GATE_INT	0x66		/* free vector the stubs go through */
#define STUB_SIZE	8		/* one import stub, see make_stub() */
#define MAX_IMPORTS	1024

struct import {
    char mod[9];
    char name[64];		/* empty when imported by ordinal */
    uint16_t ord;
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
    uint32_t off;

    if (n >= MAX_IMPORTS)
	return -1;
    snprintf(l->imp[n].mod, sizeof(l->imp[n].mod), "%s", mod ?: "?");
    snprintf(l->imp[n].name, sizeof(l->imp[n].name), "%s", name ?: "");
    l->imp[n].ord = ord;

    off = n * STUB_SIZE;
    /* mov ax,n; int GATE_INT; retf; nop */
    _farpokeb(l->stub_data_sel, off + 0, 0xb8);
    _farpokew(l->stub_data_sel, off + 1, n);
    _farpokeb(l->stub_data_sel, off + 3, 0xcd);
    _farpokeb(l->stub_data_sel, off + 4, GATE_INT);
    _farpokeb(l->stub_data_sel, off + 5, 0xcb);
    _farpokew(l->stub_data_sel, off + 6, 0x9090);

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

/* Called from gate_entry once the program enters a stub. */
int ASMCFUNC run286_import(void)
{
    unsigned n = gate_index;
    const struct import *im;

    if (n >= ldr.nstub) {
	printf("run286: bogus import index %u\n", n);
	gate_exit_code = 1;
	return 1;
    }
    im = &ldr.imp[n];
    if (im->name[0])
	printf("run286: first import call: %s.%s, from %04x:%04x\n",
		im->mod, im->name, gate_cli_ss, gate_cli_esp);
    else
	printf("run286: first import call: %s.%u, from %04x:%04x\n",
		im->mod, im->ord, gate_cli_ss, gate_cli_esp);
    gate_exit_code = 0;
    return 1;			/* nothing is implemented yet, so stop here */
}

static int stub_seg_init(struct dos_ldr *l, unsigned nimp)
{
    __dpmi_paddr h;

    l->stub_mem.size = nimp * STUB_SIZE;
    if (__dpmi_allocate_memory(&l->stub_mem) == -1) {
	printf("run286: cannot allocate the stub segment\n");
	return -1;
    }
    l->stub_code_sel = __dpmi_allocate_ldt_descriptors(2);
    if (l->stub_code_sel == (uint16_t)-1) {
	printf("run286: cannot allocate the stub descriptors\n");
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
	printf("run286: cannot set up the stub descriptors\n");
	return -1;
    }

    gate_ds32 = _my_ds();
    gate_stk_ss = _my_ds();
    gate_stk_esp = gate_stack_end;
    h.selector = _my_cs();
    h.offset32 = gate_entry;
    if (__dpmi_set_protected_mode_interrupt_vector(GATE_INT, &h) == -1) {
	printf("run286: cannot hook int %#x\n", GATE_INT);
	return -1;
    }
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

static int load_segments(struct dos_ldr *l, const uint8_t *file)
{
    const struct ne_image *ne = l->ne;
    unsigned long total = 0, off;
    int i;

    for (i = 0; i < ne->cseg; i++)
	total += (ne->seg[i].minalloc + 15) & ~15UL;

    l->mem.size = total;
    if (__dpmi_allocate_memory(&l->mem) == -1) {
	printf("run286: cannot allocate %lu bytes of DPMI memory\n", total);
	return -1;
    }
    l->base_sel = __dpmi_allocate_ldt_descriptors(ne->cseg);
    if (l->base_sel == (uint16_t)-1) {
	printf("run286: cannot allocate %u descriptors\n", ne->cseg);
	return -1;
    }
    printf("run286: %lu bytes at linear %#lx, %u selectors from %#x\n",
	    total, (unsigned long)l->mem.address, ne->cseg, l->base_sel);

    off = 0;
    for (i = 0; i < ne->cseg; i++) {
	struct ne_seg *sg = &ne->seg[i];
	struct seg_info *si = &l->seg[i];
	uint32_t len = ne_seg_len(sg);

	si->size = sg->minalloc;
	si->lin = l->mem.address + off;
	si->sel = l->base_sel + i * 8;
	off += (sg->minalloc + 15) & ~15UL;

	/* everything is a writable data segment while we fill it in */
	if (__dpmi_set_segment_base_address(si->sel, si->lin) == -1 ||
		__dpmi_set_segment_limit(si->sel, si->size - 1) == -1 ||
		__dpmi_set_descriptor_access_rights(si->sel, AR_DATA16) == -1) {
	    printf("run286: cannot set up descriptor %#x for segment %d\n",
		    si->sel, i + 1);
	    return -1;
	}
	si->shadow = calloc(1, si->size);
	if (!si->shadow) {
	    printf("run286: out of memory for segment %d\n", i + 1);
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
	    printf("run286: cannot make segment %d executable\n", i + 1);
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
	printf("run286: no image given; set RUN286_IMAGE or write RUN286.CFG\n");
	return 2;
    }

    printf("run286: loading %s\n", path);
    file = slurp(path, &size);
    if (!file) {
	printf("run286: cannot read %s\n", path);
	return 2;
    }
    if (pl_bound_parse(&b, file, size, &err) != 0) {
	printf("run286: %s\n", err);
	return 1;
    }
    if (ne_parse(&ne, file, size, b.app_off, &err) != 0) {
	printf("run286: %s\n", err);
	return 1;
    }
    printf("run286: NE at %#x, %u segments, entry %04x:%04x\n", b.app_off,
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
	    printf("run286: segment %d: %s\n", i, err);
	    return 1;
	}
    }
    printf("run286: %u fixups, %u locations, %u unresolved, %u import stubs\n",
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
	printf("run286: bad entry %08x, stack %08x or autodata %u\n",
		ne.csip, ne.sssp, ne.autodata);
	return 1;
    }
    if (!sp)				/* top of the stack segment */
	sp = l->seg[ss_seg - 1].size;
    printf("run286: entering %04x:%04x, stack %04x:%04x, ds %04x\n",
	    l->seg[entry_seg - 1].sel, (unsigned)(ne.csip & 0xffff),
	    l->seg[ss_seg - 1].sel, sp, l->seg[ne.autodata - 1].sel);
    fflush(stdout);
    rc = ne_enter(l->seg[entry_seg - 1].sel, ne.csip & 0xffff,
	    l->seg[ss_seg - 1].sel, sp,
	    l->seg[ne.autodata - 1].sel, l->seg[ne.autodata - 1].sel);
    printf("run286: back from the program, rc %d\n", rc);
    ne_free(&ne);
    return 0;
}
