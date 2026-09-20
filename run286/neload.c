/* Applying NE relocations. Free software, GPL v2 or later. */
#include <string.h>
#include "neload.h"

static uint16_t rd16(const uint8_t *p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xff;
    p[1] = v >> 8;
}

static int reloc_target(const struct ne_image *ne, struct ne_ldr_ops *ops,
	uint8_t type, uint16_t p1, uint16_t p2, struct ne_far *a,
	struct ne_reloc_stats *st)
{
    char buf[256];
    const char *mod, *name;

    switch (type & 3) {
    case NE_REL_INTERNAL:
	if (p1 == 0xff) {		/* moveable, p2 is an entry ordinal */
	    uint16_t segnum, off;

	    st->moveable++;
	    if (ne_entry_lookup(ne, p2, &segnum, &off) != 0)
		return -1;
	    if (ops->seg_addr(ops->ctx, segnum, a) != 0)
		return -1;
	    a->off = off;
	    return 0;
	}
	if (ops->seg_addr(ops->ctx, p1, a) != 0)
	    return -1;
	a->off = p2;
	return 0;
    case NE_REL_ORDINAL:
	if (!p1 || p1 > ne->cmod)
	    return -1;
	mod = ne->modref[p1 - 1];
	return ops->resolve_ord(ops->ctx, mod, p2, a);
    case NE_REL_NAME:
	if (!p1 || p1 > ne->cmod)
	    return -1;
	mod = ne->modref[p1 - 1];
	name = ne_impname(ne, p2, buf, sizeof(buf));
	if (!name)
	    return -1;
	return ops->resolve_name(ops->ctx, mod, name, a);
    default:
	if (!ops->osfixup)
	    return -1;
	return ops->osfixup(ops->ctx, p1, p2, a);
    }
}

int ne_relocate(const struct ne_image *ne, uint16_t segnum,
	struct ne_ldr_ops *ops, struct ne_reloc_stats *st, const char **err)
{
    const struct ne_seg *sg;
    uint8_t *mem;
    uint32_t size;
    unsigned i;

    if (!segnum || segnum > ne->cseg) {
	*err = "segment number out of range";
	return -1;
    }
    sg = &ne->seg[segnum - 1];
    if (!(sg->flags & NE_SEG_RELOC) || !sg->nreloc)
	return 0;
    mem = ops->seg_mem(ops->ctx, segnum, &size);
    if (!mem) {
	*err = "segment not loaded";
	return -1;
    }

    for (i = 0; i < sg->nreloc; i++) {
	const uint8_t *r = ne->file + sg->reloc_off + i * 8;
	uint8_t atype = r[0], rtype = r[1];
	uint32_t off = rd16(r + 2);
	uint16_t p1 = rd16(r + 4), p2 = rd16(r + 6);
	struct ne_far a;
	int additive = !!(rtype & NE_REL_ADDITIVE);

	st->applied++;
	st->by_type[rtype & 3]++;
	if (additive)
	    st->additive++;
	if (reloc_target(ne, ops, rtype, p1, p2, &a, st) != 0) {
	    st->unresolved++;
	    continue;
	}

	for (;;) {
	    uint32_t next;
	    uint8_t *sp;

	    if (off + 2 > size) {
		st->out_of_range++;
		break;
	    }
	    sp = mem + off;
	    next = rd16(sp);
	    st->patched++;
	    switch (atype & 0x7f) {
	    case NE_ADDR_LOBYTE:
		*sp = additive ? (uint8_t)(*sp + a.off) : (uint8_t)a.off;
		break;
	    case NE_ADDR_OFFSET:
		wr16(sp, additive ? (uint16_t)(rd16(sp) + a.off) : a.off);
		break;
	    case NE_ADDR_FARPTR:
		if (off + 4 > size) {
		    st->out_of_range++;
		    return 0;
		}
		wr16(sp, additive ? (uint16_t)(rd16(sp) + a.off) : a.off);
		wr16(sp + 2, a.sel);
		break;
	    case NE_ADDR_SEG:
		wr16(sp, a.sel);
		break;
	    default:
		*err = "unknown relocation address type";
		return -1;
	    }
	    if (additive || next == off || next == 0xffff)
		break;
	    off = next;
	}
    }
    return 0;
}
