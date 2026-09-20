/* Applying NE relocations. Free software, GPL v2 or later. */
#ifndef NELOAD_H
#define NELOAD_H

#include "neexe.h"

struct ne_far {
    uint16_t sel;
    uint16_t off;
};

struct ne_ldr_ops {
    void *ctx;
    /* segnum is 1-based, as everywhere in the file */
    int (*seg_addr)(void *ctx, uint16_t segnum, struct ne_far *a);
    uint8_t *(*seg_mem)(void *ctx, uint16_t segnum, uint32_t *size);
    int (*resolve_ord)(void *ctx, const char *mod, uint16_t ord,
	    struct ne_far *a);
    int (*resolve_name)(void *ctx, const char *mod, const char *name,
	    struct ne_far *a);
    int (*osfixup)(void *ctx, uint16_t type, uint16_t arg, struct ne_far *a);
};

struct ne_reloc_stats {
    unsigned applied;		/* fixup records processed */
    unsigned patched;		/* locations written, chains included */
    unsigned by_type[4];
    unsigned additive;
    unsigned moveable;		/* internal refs through the entry table */
    unsigned unresolved;
    unsigned out_of_range;
};

int ne_relocate(const struct ne_image *ne, uint16_t segnum,
	struct ne_ldr_ops *ops, struct ne_reloc_stats *st, const char **err);

#endif
