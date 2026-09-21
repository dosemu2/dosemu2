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
#include "neload.h"

/* access rights for a 16bit, DPL 3, present segment */
#define AR_CODE16	0x00fa		/* code, readable */
#define AR_DATA16	0x00f2		/* data, writable */

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
};

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

/* PHAPI and DOSCALLS are not implemented yet: point every import at a
 * selector of zero, which faults visibly the moment the program calls it. */
static int dos_resolve_ord(void *ctx, const char *mod, uint16_t ord,
	struct ne_far *a)
{
    struct dos_ldr *l = ctx;

    l->nstub++;
    a->sel = 0;
    a->off = ord;
    return 0;
}

static int dos_resolve_name(void *ctx, const char *mod, const char *name,
	struct ne_far *a)
{
    struct dos_ldr *l = ctx;

    l->nstub++;
    a->sel = 0;
    a->off = 0;
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
    struct pl_bound b;
    struct ne_image ne;
    struct dos_ldr l = {};
    struct ne_ldr_ops ops = {};
    struct ne_reloc_stats st = {};
    const char *err = "";
    char cfg[128];
    uint8_t *file;
    size_t size;
    int i;

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

    l.ne = &ne;
    l.seg = calloc(ne.cseg, sizeof(*l.seg));
    if (!l.seg || load_segments(&l, file) != 0)
	return 1;

    ops.ctx = &l;
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
    printf("run286: %u fixups, %u locations, %u unresolved, %u stubbed imports\n",
	    st.applied, st.patched, st.unresolved, l.nstub);
    if (commit_segments(&l) != 0)
	return 1;

    printf("run286: entry at %04x:%04x, first code selector %#x, last %#x\n",
	    l.seg[(ne.csip >> 16) - 1].sel, (unsigned)(ne.csip & 0xffff),
	    l.seg[0].sel, l.seg[ne.cseg - 1].sel);
    printf("run286: image loaded, nothing calls it yet\n");
    ne_free(&ne);
    return 0;
}
