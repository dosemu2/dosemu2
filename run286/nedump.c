/*
 * Host-side test harness: parse a bound 286|DOS-Extender executable,
 * lay its NE image out in memory and run every relocation against a
 * dummy backend. Free software, GPL v2 or later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "neload.h"

#define MAX_IMP 512

struct imp {
    char mod[9];
    char name[64];		/* empty for an ordinal import */
    uint16_t ord;
    unsigned refs;
};

struct host_ctx {
    const struct ne_image *ne;
    uint8_t **mem;
    uint32_t *size;
    struct imp imp[MAX_IMP];
    int nimp;
};

static int host_seg_addr(void *ctx, uint16_t segnum, struct ne_far *a)
{
    struct host_ctx *h = ctx;

    if (!segnum || segnum > h->ne->cseg)
	return -1;
    a->sel = (segnum << 3) | 7;
    a->off = 0;
    return 0;
}

static uint8_t *host_seg_mem(void *ctx, uint16_t segnum, uint32_t *size)
{
    struct host_ctx *h = ctx;

    if (!segnum || segnum > h->ne->cseg)
	return NULL;
    *size = h->size[segnum - 1];
    return h->mem[segnum - 1];
}

static struct imp *imp_find(struct host_ctx *h, const char *mod,
	const char *name, uint16_t ord)
{
    int i;

    for (i = 0; i < h->nimp; i++) {
	if (strcmp(h->imp[i].mod, mod))
	    continue;
	if (name) {
	    if (!strcmp(h->imp[i].name, name))
		return &h->imp[i];
	} else if (!h->imp[i].name[0] && h->imp[i].ord == ord) {
	    return &h->imp[i];
	}
    }
    if (h->nimp == MAX_IMP)
	return NULL;
    memset(&h->imp[h->nimp], 0, sizeof(h->imp[0]));
    snprintf(h->imp[h->nimp].mod, sizeof(h->imp[0].mod), "%s", mod);
    if (name)
	snprintf(h->imp[h->nimp].name, sizeof(h->imp[0].name), "%s", name);
    h->imp[h->nimp].ord = ord;
    return &h->imp[h->nimp++];
}

static int host_resolve_ord(void *ctx, const char *mod, uint16_t ord,
	struct ne_far *a)
{
    struct imp *im = imp_find(ctx, mod, NULL, ord);

    if (!im)
	return -1;
    im->refs++;
    a->sel = 0x1000 | 7;
    a->off = ord;
    return 0;
}

static int host_resolve_name(void *ctx, const char *mod, const char *name,
	struct ne_far *a)
{
    struct imp *im = imp_find(ctx, mod, name, 0);

    if (!im)
	return -1;
    im->refs++;
    a->sel = 0x1008 | 7;
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

int main(int argc, char **argv)
{
    struct pl_bound b;
    struct ne_image ne;
    struct host_ctx h = {};
    struct ne_ldr_ops ops = {};
    struct ne_reloc_stats st = {};
    const char *err = "";
    uint8_t *file;
    size_t size;
    unsigned code = 0, data = 0, moveable = 0, total = 0;
    int i, verbose = 0;

    if (argc < 2) {
	fprintf(stderr, "usage: nedump [-v] <bound.exe>\n");
	return 2;
    }
    if (!strcmp(argv[1], "-v")) {
	verbose = 1;
	argv++;
	argc--;
    }
    file = slurp(argv[1], &size);
    if (!file) {
	perror(argv[1]);
	return 2;
    }
    printf("%s: %zu bytes\n", argv[1], size);

    if (pl_bound_parse(&b, file, size, &err) != 0) {
	fprintf(stderr, "bound image: %s\n", err);
	return 1;
    }
    printf("  real-mode stub  %#x..%#x (%u bytes)\n", 0, b.stub_size,
	    b.stub_size);
    if (b.dosx_size)
	printf("  extender image  %#x..%#x (%u bytes, '%c%c')\n", b.dosx_off,
		b.dosx_off + b.dosx_size, b.dosx_size, file[b.dosx_off],
		file[b.dosx_off + 1]);
    if (b.dlld_off) {
	printf("  DLL directory   %#x, %d entries:", b.dlld_off, b.ndll);
	for (i = 0; i < b.ndll; i++)
	    printf(" %s@%#x", b.dll[i].name, b.dll[i].off);
	printf("\n");
    }
    printf("  program image   %#x\n", b.app_off);

    if (ne_parse(&ne, file, size, b.app_off, &err) != 0) {
	fprintf(stderr, "NE image: %s\n", err);
	return 1;
    }
    printf("NE: exetyp %#x flags %#x segments %u modules %u align %u\n",
	    ne.exetyp, ne.flags, ne.cseg, ne.cmod, 1u << ne.align_shift);
    printf("    entry %04x:%04x  autodata %u  heap %u  stack %u\n",
	    (unsigned)(ne.csip >> 16), (unsigned)(ne.csip & 0xffff),
	    ne.autodata, ne.heap, ne.stack);
    printf("    imports:");
    for (i = 0; i < ne.cmod; i++)
	printf(" %s", ne.modref[i]);
    printf("\n");

    h.ne = &ne;
    h.mem = calloc(ne.cseg, sizeof(*h.mem));
    h.size = calloc(ne.cseg, sizeof(*h.size));
    for (i = 0; i < ne.cseg; i++) {
	struct ne_seg *sg = &ne.seg[i];
	uint32_t len = ne_seg_len(sg);

	h.size[i] = sg->minalloc;
	h.mem[i] = calloc(1, h.size[i]);
	if (!h.mem[i]) {
	    fprintf(stderr, "out of memory for segment %d\n", i + 1);
	    return 1;
	}
	if (sg->file_off)
	    memcpy(h.mem[i], file + sg->file_off, len);
	if (sg->flags & NE_SEG_DATA)
	    data++;
	else
	    code++;
	if (sg->flags & NE_SEG_MOVEABLE)
	    moveable++;
	total += sg->minalloc;
	if (verbose)
	    printf("    seg %3d %s len %6u alloc %6u flags %04x dpl %u %s\n",
		    i + 1, (sg->flags & NE_SEG_DATA) ? "data" : "code", len,
		    sg->minalloc, sg->flags,
		    (sg->flags & NE_SEG_DPL_MASK) >> NE_SEG_DPL_SHIFT,
		    (sg->flags & NE_SEG_RELOC) ? "reloc" : "");
    }
    printf("    %u code, %u data, %u moveable, %u bytes of segments\n",
	    code, data, moveable, total);

    ops.ctx = &h;
    ops.seg_addr = host_seg_addr;
    ops.seg_mem = host_seg_mem;
    ops.resolve_ord = host_resolve_ord;
    ops.resolve_name = host_resolve_name;
    for (i = 1; i <= ne.cseg; i++) {
	if (ne_relocate(&ne, i, &ops, &st, &err) != 0) {
	    fprintf(stderr, "segment %d: %s\n", i, err);
	    return 1;
	}
    }
    printf("relocations: %u records, %u locations patched\n", st.applied,
	    st.patched);
    printf("    internal %u (moveable %u), ordinal %u, name %u, osfixup %u,"
	    " additive %u\n", st.by_type[0], st.moveable, st.by_type[1],
	    st.by_type[2], st.by_type[3], st.additive);
    printf("    unresolved %u, out of range %u\n", st.unresolved,
	    st.out_of_range);
    printf("imports used: %d\n", h.nimp);
    for (i = 0; i < h.nimp; i++) {
	if (h.imp[i].name[0])
	    printf("    %-8s %s (%u)\n", h.imp[i].mod, h.imp[i].name,
		    h.imp[i].refs);
	else
	    printf("    %-8s #%u (%u)\n", h.imp[i].mod, h.imp[i].ord,
		    h.imp[i].refs);
    }
    ne_free(&ne);
    return st.unresolved || st.out_of_range ? 1 : 0;
}
