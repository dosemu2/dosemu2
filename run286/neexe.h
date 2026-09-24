/*
 * Parsing of Phar Lap 286|DOS-Extender bound executables and of the
 * 16bit NE images they carry.
 *
 * The bound file made by BIND286 is a real-mode MZ loader, followed by
 * the P2 image of the extender itself, followed by a "DLLD" directory of
 * the DLLs bound into the file, followed by the NE image of the program.
 *
 * This file is free software, GPL v2 or later.
 */
#ifndef NEEXE_H
#define NEEXE_H

#include <stdint.h>
#include <stddef.h>

#define NE_SEG_DATA	0x0001
#define NE_SEG_MOVEABLE	0x0010
#define NE_SEG_PRELOAD	0x0040
#define NE_SEG_RELOC	0x0100
#define NE_SEG_DPL_MASK	0x0c00
#define NE_SEG_DPL_SHIFT 10

/* ne_exetyp of the programs built for 286|DOS-Extender */
#define NE_EXETYP_OS2		0x01
#define NE_EXETYP_WIN		0x02
#define NE_EXETYP_PHARLAP_OS2	0x81
#define NE_EXETYP_PHARLAP_WIN	0x82

/* low 2 bits of a relocation's type byte */
#define NE_REL_INTERNAL	0
#define NE_REL_ORDINAL	1
#define NE_REL_NAME	2
#define NE_REL_OSFIXUP	3
#define NE_REL_ADDITIVE	0x04

/* relocation address types */
#define NE_ADDR_LOBYTE	0
#define NE_ADDR_SEG	2
#define NE_ADDR_FARPTR	3
#define NE_ADDR_OFFSET	5

#define NE_MAX_MODREF	16

/* flags byte of an entry-table entry */
#define NE_ENT_EXPORTED	0x01
#define NE_ENT_DATA	0x02	/* wants the module's own DGROUP in DS */

struct ne_seg {
    uint32_t file_off;		/* 0 if the segment has no file image */
    uint32_t len;		/* 0 in the file means 64K */
    uint16_t flags;
    uint32_t minalloc;		/* 0 in the file means 64K */
    uint32_t reloc_off;		/* 0 if NE_SEG_RELOC is clear */
    uint16_t nreloc;
};

struct ne_image {
    const uint8_t *file;
    size_t file_size;
    uint32_t hdr;		/* offset of the NE header in the file */
    uint16_t flags, autodata, heap, stack, cseg, cmod, cmovent;
    uint16_t align_shift;
    uint8_t exetyp;
    uint32_t csip, sssp;
    uint32_t segtab, restab, modtab, imptab, enttab, nrestab;
    uint16_t cbenttab, cbnrestab;
    struct ne_seg *seg;		/* cseg entries */
    char modref[NE_MAX_MODREF][9];
};

struct ne_dll {
    char name[9];
    uint32_t off;
};

struct pl_bound {
    const uint8_t *file;
    size_t file_size;
    uint32_t stub_size;		/* size of the real-mode MZ loader */
    uint32_t dosx_off, dosx_size;	/* the P2 image of the extender */
    uint32_t dlld_off;		/* 0 if the file has no DLL directory */
    int ndll;
    struct ne_dll dll[NE_MAX_MODREF];
    uint32_t app_off;		/* the NE image of the program */
};

/* Both return 0 on success and fill *err with a static string on failure. */
int pl_bound_parse(struct pl_bound *b, const uint8_t *file, size_t size,
	const char **err);
int ne_parse(struct ne_image *ne, const uint8_t *file, size_t size,
	uint32_t hdr, const char **err);
void ne_free(struct ne_image *ne);

static inline uint32_t ne_seg_len(const struct ne_seg *s)
{
    return s->len ? s->len : 0x10000;
}

const char *ne_impname(const struct ne_image *ne, uint16_t off, char *buf,
	size_t len);
/* Walking the entry table. Zero the iterator to start; ne_entry_next()
 * returns 1 for as long as it yields an entry and 0 at the end of the
 * table. Segment numbers are 1-based, as in the file. */
struct ne_entry_iter {
    uint32_t off;		/* how far into the table we are */
    uint16_t ord;		/* the ordinal of the entry to yield next */
    uint8_t cnt, type;		/* what is left of the bundle we are in */
};

int ne_entry_next(const struct ne_image *ne, struct ne_entry_iter *it,
	uint16_t *ord, uint8_t *flags, uint16_t *segnum, uint16_t *off);
/* Maps an entry-table ordinal to a segment number and an offset within it.
 * Returns 0 on success. Segment numbers are 1-based, as in the file. */
int ne_entry_lookup(const struct ne_image *ne, uint16_t ord, uint16_t *segnum,
	uint16_t *off);
/* The ordinal a module exports a name under, or 0 if it exports no such
 * name. The linkers disagree about case, so the comparison ignores it. */
uint16_t ne_name_ordinal(const struct ne_image *ne, const char *name);

#endif
