/*
 *  Copyright (C) 2026  @stsp and OpenMT32 project authors
 *
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef MT32_BPF_H
#define MT32_BPF_H

#include <stdlib.h>
#include <string.h>

typedef struct { unsigned short code; unsigned char jt, jf; unsigned k; } mt32_bpf_insn;

typedef struct {
    unsigned base;
    const mt32_bpf_insn *offs, *len;
    unsigned n_offs, n_len;
} mt32_bpf_entry;

typedef struct {
    unsigned img_sz, n_entries, tail_len;
    mt32_bpf_insn *insns;
    mt32_bpf_entry *entries;
    const unsigned char *tail;
} mt32_bpf_t;

/* classes, sizes and modes, as <linux/filter.h> spells them */
#define MT32_BPF_LD   0x00
#define MT32_BPF_ALU  0x04
#define MT32_BPF_JMP  0x05
#define MT32_BPF_RET  0x06
#define MT32_BPF_MISC 0x07

static inline unsigned mt32_bpf_allowed(unsigned code)
{
    switch (code) {
    case 0x30: case 0x50:                       /* LD B ABS, LD B IND */
    case 0x07:                                  /* MISC TAX */
    case 0x04: case 0x24: case 0x44: case 0x54: /* ALU add/mul/or/and K */
    case 0x64: case 0x74:                       /* ALU lsh/rsh K */
    case 0x15: case 0x25: case 0x35: case 0x45: /* JMP jeq/jgt/jge/jset K */
    case 0x06: case 0x16:                       /* RET K, RET A */
        return 1;
    }
    return 0;
}

static inline unsigned mt32_bpf_run(const mt32_bpf_insn *p, unsigned n,
                                    const unsigned char *pkt, unsigned len)
{
    unsigned A = 0, X = 0, pc = 0, i;

    while (pc < n) {
        const mt32_bpf_insn *in = &p[pc++];
        switch (in->code & 0x07) {
        case MT32_BPF_LD:
            i = in->k + (((in->code & 0x60) == 0x40) ? X : 0);
            if (i >= len) return 0;
            A = pkt[i];
            break;
        case MT32_BPF_MISC:
            X = A;
            break;
        case MT32_BPF_ALU:
            switch (in->code & 0xf0) {
            case 0x00: A += in->k; break;
            case 0x20: A *= in->k; break;
            case 0x40: A |= in->k; break;
            case 0x50: A &= in->k; break;
            case 0x60: A <<= in->k; break;
            case 0x70: A >>= in->k; break;
            }
            break;
        case MT32_BPF_JMP: {
            unsigned hit = 0;
            switch (in->code & 0xf0) {
            case 0x10: hit = (A == in->k); break;
            case 0x20: hit = (A >  in->k); break;
            case 0x30: hit = (A >= in->k); break;
            case 0x40: hit = (A &  in->k) != 0; break;
            }
            pc += hit ? in->jt : in->jf;
            break;
        }
        case MT32_BPF_RET:
            return (in->code & 0x18) == 0x10 ? A : in->k;
        }
    }
    return 0;                                   /* unreachable once validated */
}

static inline unsigned mt32_bpf_u16(const unsigned char *p) { return p[0] | (p[1] << 8); }

static inline int mt32_bpf_check_prog(const mt32_bpf_insn *p, unsigned n)
{
    unsigned pc;
    if (!n) return 0;
    for (pc = 0; pc < n; pc++) {
        if (!mt32_bpf_allowed(p[pc].code)) return 0;
        if ((p[pc].code & 0x07) == MT32_BPF_JMP) {
            /* forward-only, in range: this is what makes loops impossible */
            if (pc + 1 + p[pc].jt >= n || pc + 1 + p[pc].jf >= n) return 0;
        }
    }
    return (p[n-1].code & 0x07) == MT32_BPF_RET;
}

/* Parse and validate a <mt32_traversal> blob. Returns 0 and touches nothing
 * on any malformed input; the caller keeps ownership of `blob`, whose tail
 * bytes are borrowed rather than copied. Free with mt32_bpf_free(). */
static inline int mt32_bpf_load(mt32_bpf_t *t, const unsigned char *blob, size_t n)
{
    unsigned i, total = 0, pos;
    const unsigned char *hdrs;

    memset(t, 0, sizeof *t);
    if (n < 6) return 0;
    t->img_sz    = mt32_bpf_u16(blob);
    t->n_entries = mt32_bpf_u16(blob + 2);
    t->tail_len  = mt32_bpf_u16(blob + 4);
    if (!t->img_sz || !t->n_entries) return 0;
    if (n < 6 + (size_t)t->n_entries * 6) return 0;

    hdrs = blob + 6;
    for (i = 0; i < t->n_entries; i++) {
        unsigned n_offs = mt32_bpf_u16(hdrs + 6*i + 2);
        unsigned n_len  = mt32_bpf_u16(hdrs + 6*i + 4);
        if (mt32_bpf_u16(hdrs + 6*i) > t->img_sz) return 0;
        total += n_offs + n_len;
    }
    pos = 6 + t->n_entries * 6;
    if (n != (size_t)pos + (size_t)total * 8 + t->tail_len) return 0;

    t->insns = (mt32_bpf_insn *)calloc(total ? total : 1, sizeof *t->insns);
    t->entries = (mt32_bpf_entry *)calloc(t->n_entries, sizeof *t->entries);
    if (!t->insns || !t->entries) { free(t->insns); free(t->entries); return 0; }

    for (i = 0; i < total; i++) {
        const unsigned char *p = blob + pos + 8*i;
        t->insns[i].code = mt32_bpf_u16(p);
        t->insns[i].jt = p[2];
        t->insns[i].jf = p[3];
        t->insns[i].k = p[4] | (p[5] << 8) | ((unsigned)p[6] << 16) |
                        ((unsigned)p[7] << 24);
    }
    total = 0;
    for (i = 0; i < t->n_entries; i++) {
        mt32_bpf_entry *e = &t->entries[i];
        e->base   = mt32_bpf_u16(hdrs + 6*i);
        e->n_offs = mt32_bpf_u16(hdrs + 6*i + 2);
        e->n_len  = mt32_bpf_u16(hdrs + 6*i + 4);
        e->offs = t->insns + total; total += e->n_offs;
        e->len  = t->insns + total; total += e->n_len;
        if (!mt32_bpf_check_prog(e->offs, e->n_offs) ||
            !mt32_bpf_check_prog(e->len, e->n_len)) {
            free(t->insns); free(t->entries);
            memset(t, 0, sizeof *t);
            return 0;
        }
    }
    t->tail = blob + pos + (size_t)total * 8;
    return 1;
}

static inline void mt32_bpf_free(mt32_bpf_t *t)
{
    free(t->insns);
    free(t->entries);
    memset(t, 0, sizeof *t);
}

static inline int mt32_bpf_walk(const mt32_bpf_t *t, const unsigned char *pkt,
                         void (*emit)(void *, const unsigned char *, unsigned),
                         void *ctx)
{
    static const unsigned char zeros[64];
    unsigned i, pktlen = t->img_sz + t->tail_len;

    for (i = 0; i < t->n_entries; i++) {
        const mt32_bpf_entry *e = &t->entries[i];
        unsigned off = mt32_bpf_run(e->offs, e->n_offs, pkt, pktlen);
        unsigned len = mt32_bpf_run(e->len, e->n_len, pkt, pktlen);
        unsigned n;
        if (off > t->img_sz || e->base + off > t->img_sz ||
            len > t->img_sz - e->base - off)
            return 0;
        for (n = off; n; ) {
            unsigned chunk = n > sizeof zeros ? (unsigned)sizeof zeros : n;
            emit(ctx, zeros, chunk);
            n -= chunk;
        }
        emit(ctx, pkt + e->base + off, len);
    }
    return 1;
}

#endif /* MT32_BPF_H */
