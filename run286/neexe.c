/*
 * Parsing of Phar Lap bound executables and of the NE images inside them.
 * This file is free software, GPL v2 or later.
 */
#include <string.h>
#include <stdlib.h>
#include "neexe.h"

static uint16_t rd16(const uint8_t *p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return rd16(p) | ((uint32_t)rd16(p + 2) << 16);
}

/* The program's own image is marked 0x81/0x82 by Phar Lap's linker, but the
 * DLLs the games ship beside them (AILXMI.DLL, ASYLUM.DLL) carry the plain
 * OS/2 type. Only the search through a bound file insists on the Phar Lap
 * type, so that it cannot land on something else. */
static int ne_looks_valid(const uint8_t *f, size_t sz, uint32_t o, int strict)
{
    uint16_t cseg, segtab, align;
    uint8_t exetyp;

    if (o + 0x40 > sz || f[o] != 'N' || f[o + 1] != 'E')
	return 0;
    /* only the linker version Phar Lap's tools produced */
    if (f[o + 2] != 5)
	return 0;
    exetyp = f[o + 0x36];
    if (exetyp != NE_EXETYP_PHARLAP_OS2 && exetyp != NE_EXETYP_PHARLAP_WIN &&
	    (strict || (exetyp != NE_EXETYP_OS2 && exetyp != NE_EXETYP_WIN)))
	return 0;
    cseg = rd16(f + o + 0x1c);
    segtab = rd16(f + o + 0x22);
    align = rd16(f + o + 0x32);
    if (!cseg || cseg > 4096 || segtab < 0x40 || align > 15)
	return 0;
    if (o + segtab + (uint32_t)cseg * 8 > sz)
	return 0;
    return 1;
}

int pl_bound_parse(struct pl_bound *b, const uint8_t *file, size_t size,
	const char **err)
{
    uint32_t o, pages, lastpage;
    int i;

    memset(b, 0, sizeof(*b));
    b->file = file;
    b->file_size = size;

    if (size < 0x40 || file[0] != 'M' || file[1] != 'Z') {
	*err = "not an MZ executable";
	return -1;
    }
    lastpage = rd16(file + 2);
    pages = rd16(file + 4);
    if (!pages) {
	*err = "empty MZ image";
	return -1;
    }
    b->stub_size = (pages - 1) * 512 + (lastpage ? lastpage : 512);

    /* A bound file always carries the extender's image right after the
     * real-mode loader. Anything else is a plain NE, as the DLLs the games
     * ship beside them are. */
    o = b->stub_size;
    if (o + 2 <= size && file[o] == 'P' &&
	    (file[o + 1] == '2' || file[o + 1] == '3')) {
	b->dosx_off = o;
	b->dosx_size = rd32(file + o + 6);
	if (b->dosx_size < 0x80 || b->dosx_off + b->dosx_size > size) {
	    *err = "bad DOS-extender image size";
	    return -1;
	}
	o += b->dosx_size;
    } else {
	uint32_t lfanew = rd32(file + 0x3c);

	if (rd16(file + 0x18) >= 0x40 && ne_looks_valid(file, size, lfanew, 0)) {
	    b->stub_size = lfanew;
	    b->app_off = lfanew;
	    return 0;
	}
	if (b->stub_size >= size) {
	    *err = "no payload after the MZ image";
	    return -1;
	}
    }

    if (o + 16 <= size && !memcmp(file + o, "DLLD", 4)) {
	uint16_t recsz = rd16(file + o + 4);
	uint16_t n = rd16(file + o + 6);

	b->dlld_off = o;
	if (recsz < 12 || n > NE_MAX_MODREF) {
	    *err = "bad DLL directory";
	    return -1;
	}
	for (i = 0; i < n; i++) {
	    const uint8_t *r = file + o + 16 + (uint32_t)i * recsz;

	    if (r + recsz > file + size) {
		*err = "truncated DLL directory";
		return -1;
	    }
	    memcpy(b->dll[i].name, r, 8);
	    b->dll[i].name[8] = '\0';
	    b->dll[i].off = rd32(r + 8);
	}
	b->ndll = n;
    }

    /* The program's own image is the first NE that the DLL directory does
     * not claim. Scanning beats arithmetic here: the bound DLLs have no
     * sizes in the directory. */
    for (o = b->stub_size; o + 0x40 <= size; o += 2) {
	int bound = 0;

	if (!ne_looks_valid(file, size, o, 1))
	    continue;
	for (i = 0; i < b->ndll; i++)
	    if (b->dll[i].off == o)
		bound = 1;
	if (bound)
	    continue;
	b->app_off = o;
	return 0;
    }
    *err = "no program NE image found";
    return -1;
}

int ne_parse(struct ne_image *ne, const uint8_t *file, size_t size,
	uint32_t hdr, const char **err)
{
    const uint8_t *h = file + hdr;
    int i;

    memset(ne, 0, sizeof(*ne));
    if (!ne_looks_valid(file, size, hdr, 0)) {
	*err = "not a Phar Lap NE image";
	return -1;
    }
    ne->file = file;
    ne->file_size = size;
    ne->hdr = hdr;
    ne->enttab = rd16(h + 0x04);
    ne->cbenttab = rd16(h + 0x06);
    ne->flags = rd16(h + 0x0c);
    ne->autodata = rd16(h + 0x0e);
    ne->heap = rd16(h + 0x10);
    ne->stack = rd16(h + 0x12);
    ne->csip = rd32(h + 0x14);
    ne->sssp = rd32(h + 0x18);
    ne->cseg = rd16(h + 0x1c);
    ne->cmod = rd16(h + 0x1e);
    ne->segtab = rd16(h + 0x22);
    ne->restab = rd16(h + 0x26);
    ne->modtab = rd16(h + 0x28);
    ne->imptab = rd16(h + 0x2a);
    ne->nrestab = rd32(h + 0x2c);
    ne->cbnrestab = rd16(h + 0x20);
    ne->cmovent = rd16(h + 0x30);
    ne->align_shift = rd16(h + 0x32);
    if (!ne->align_shift)
	ne->align_shift = 9;
    ne->exetyp = h[0x36];

    if (ne->cmod > NE_MAX_MODREF) {
	*err = "too many module references";
	return -1;
    }
    for (i = 0; i < ne->cmod; i++) {
	uint16_t off = rd16(h + ne->modtab + i * 2);
	const uint8_t *p = h + ne->imptab + off;
	uint8_t l;

	if (p + 1 > file + size) {
	    *err = "truncated module reference table";
	    return -1;
	}
	l = *p;
	if (l > 8 || p + 1 + l > file + size) {
	    *err = "bad module name";
	    return -1;
	}
	memcpy(ne->modref[i], p + 1, l);
	ne->modref[i][l] = '\0';
    }

    ne->seg = calloc(ne->cseg, sizeof(*ne->seg));
    if (!ne->seg) {
	*err = "out of memory";
	return -1;
    }
    for (i = 0; i < ne->cseg; i++) {
	const uint8_t *s = h + ne->segtab + i * 8;
	struct ne_seg *sg = &ne->seg[i];

	sg->file_off = (uint32_t)rd16(s) << ne->align_shift;
	sg->len = rd16(s + 2);
	sg->flags = rd16(s + 4);
	sg->minalloc = rd16(s + 6);
	if (!sg->minalloc)
	    sg->minalloc = 0x10000;
	if (sg->minalloc < ne_seg_len(sg))
	    sg->minalloc = ne_seg_len(sg);
	if (!sg->file_off)
	    continue;
	if (sg->file_off + ne_seg_len(sg) > size) {
	    *err = "segment image past the end of file";
	    goto fail;
	}
	if (sg->flags & NE_SEG_RELOC) {
	    uint32_t r = sg->file_off + ne_seg_len(sg);

	    if (r + 2 > size) {
		*err = "truncated relocation table";
		goto fail;
	    }
	    sg->nreloc = rd16(file + r);
	    sg->reloc_off = r + 2;
	    if (sg->reloc_off + (uint32_t)sg->nreloc * 8 > size) {
		*err = "truncated relocation records";
		goto fail;
	    }
	}
    }
    return 0;

fail:
    ne_free(ne);
    return -1;
}

void ne_free(struct ne_image *ne)
{
    free(ne->seg);
    ne->seg = NULL;
}

const char *ne_impname(const struct ne_image *ne, uint16_t off, char *buf,
	size_t len)
{
    const uint8_t *p = ne->file + ne->hdr + ne->imptab + off;
    size_t l;

    if (p + 1 > ne->file + ne->file_size)
	return NULL;
    l = *p;
    if (l >= len || p + 1 + l > ne->file + ne->file_size)
	return NULL;
    memcpy(buf, p + 1, l);
    buf[l] = '\0';
    return buf;
}

int ne_entry_next(const struct ne_image *ne, struct ne_entry_iter *it,
	uint16_t *ord, uint8_t *flags, uint16_t *segnum, uint16_t *off)
{
    const uint8_t *tab = ne->file + ne->hdr + ne->enttab;
    const uint8_t *end = tab + ne->cbenttab;
    const uint8_t *p;

    if (end > ne->file + ne->file_size)
	return 0;
    if (!it->ord)
	it->ord = 1;
    /* The table is a run of bundles: a count, a segment indicator, and
     * then that many entries. Indicator 0 is a gap in the ordinal space
     * and 0xff a bundle of moveable entries, which carry their segment
     * number one by one behind an int 3fh thunk. */
    while (!it->cnt) {
	p = tab + it->off;
	if (p + 2 > end || !p[0])
	    return 0;
	it->cnt = p[0];
	it->type = p[1];
	it->off += 2;
	if (!it->type) {
	    it->ord += it->cnt;
	    it->cnt = 0;
	}
    }
    p = tab + it->off;
    if (it->type == 0xff) {		/* moveable: flags, int 3fh, seg, off */
	if (p + 6 > end)
	    return 0;
	*flags = p[0];
	*segnum = p[3];
	*off = rd16(p + 4);
	it->off += 6;
    } else {				/* fixed: flags, off */
	if (p + 3 > end)
	    return 0;
	*flags = p[0];
	*segnum = it->type;
	*off = rd16(p + 1);
	it->off += 3;
    }
    *ord = it->ord++;
    it->cnt--;
    return 1;
}

int ne_entry_lookup(const struct ne_image *ne, uint16_t ord, uint16_t *segnum,
	uint16_t *off)
{
    struct ne_entry_iter it = {};
    uint16_t cur, sg, o;
    uint8_t flags;

    if (!ord)
	return -1;
    while (ne_entry_next(ne, &it, &cur, &flags, &sg, &o)) {
	if (cur == ord) {
	    *segnum = sg;
	    *off = o;
	    return 0;
	}
    }
    return -1;
}

static int name_eq(const uint8_t *p, size_t l, const char *name)
{
    size_t i;

    for (i = 0; i < l; i++) {
	int a = p[i], b = (unsigned char)name[i];

	if (a >= 'a' && a <= 'z')
	    a -= 'a' - 'A';
	if (b >= 'a' && b <= 'z')
	    b -= 'a' - 'A';
	if (!b || a != b)
	    return 0;
    }
    return !name[l];
}

/* A name table is a run of counted strings, each followed by the ordinal it
 * stands for, terminated by a zero length. The first entry is the module's
 * own name (resident) or its description (non-resident), under ordinal 0. */
static uint16_t name_table_lookup(const uint8_t *p, const uint8_t *end,
	const char *name)
{
    while (p < end) {
	size_t l = *p;

	if (!l || p + 1 + l + 2 > end)
	    break;
	if (name_eq(p + 1, l, name))
	    return rd16(p + 1 + l);
	p += 1 + l + 2;
    }
    return 0;
}

uint16_t ne_name_ordinal(const struct ne_image *ne, const char *name)
{
    const uint8_t *end = ne->file + ne->file_size;
    uint16_t ord;

    if (!name || !name[0])
	return 0;
    /* the resident table has no length of its own: it runs up to the
     * module reference table, which follows it in every NE */
    if (ne->restab < ne->modtab) {
	ord = name_table_lookup(ne->file + ne->hdr + ne->restab,
		ne->file + ne->hdr + ne->modtab, name);
	if (ord)
	    return ord;
    }
    if (ne->nrestab && ne->nrestab + ne->cbnrestab <= ne->file_size)
	end = ne->file + ne->nrestab + ne->cbnrestab;
    if (ne->nrestab && ne->nrestab < ne->file_size)
	return name_table_lookup(ne->file + ne->nrestab, end, name);
    return 0;
}
