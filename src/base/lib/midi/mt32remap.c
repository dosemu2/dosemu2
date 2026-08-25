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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu.h"
#include "init.h"
#include "dosemu_config.h"
#include "mt32remap.h"

typedef unsigned char u8;
typedef unsigned int u32;

/* ------------------------------------------------------------------ sha1 */
typedef struct { u32 h[5]; unsigned char buf[64]; unsigned long long len; } sha1_t;

static u32 rol(u32 v, int n) { return (v << n) | (v >> (32 - n)); }

static void sha1_block(sha1_t *s, const unsigned char *p)
{
    u32 w[80], a, b, c, d, e, f, k, t;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((u32)p[4*i] << 24) | (p[4*i+1] << 16) | (p[4*i+2] << 8) | p[4*i+3];
    for (; i < 80; i++)
        w[i] = rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4];
    for (i = 0; i < 80; i++) {
        if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5a827999; }
        else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ed9eba1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8f1bbcdc; }
        else             { f = b ^ c ^ d;                    k = 0xca62c1d6; }
        t = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}

static void sha1_init(sha1_t *s)
{
    s->h[0] = 0x67452301; s->h[1] = 0xefcdab89; s->h[2] = 0x98badcfe;
    s->h[3] = 0x10325476; s->h[4] = 0xc3d2e1f0; s->len = 0;
}

static void sha1_update(sha1_t *s, const void *data, size_t n)
{
    const unsigned char *p = data;
    size_t used = s->len % 64, i;
    s->len += n;
    for (i = 0; i < n; i++) {
        s->buf[used++] = p[i];
        if (used == 64) { sha1_block(s, s->buf); used = 0; }
    }
}

/* hex digest truncated to 16 characters, as the Python logger does */
static void sha1_hex16(sha1_t *s, char out[17])
{
    unsigned long long bits = s->len * 8;
    unsigned char pad = 0x80, zero = 0, len[8];
    size_t i;
    sha1_update(s, &pad, 1);
    while (s->len % 64 != 56) sha1_update(s, &zero, 1);
    for (i = 0; i < 8; i++) len[i] = (unsigned char)(bits >> (56 - 8*i));
    sha1_update(s, len, 8);
    for (i = 0; i < 8; i++)
        sprintf(out + 2*i, "%02x", (unsigned char)(s->h[i/4] >> (24 - 8*(i%4))));
    out[16] = 0;
}

/* --------------------------------------------------------- MT-32 memory */
#define MEMADDR(x) (((x) & 0x7f0000) >> 2 | ((x) & 0x7f00) >> 1 | ((x) & 0x7f))
#define A_PATCH_TEMP  MEMADDR(0x030000)
#define A_RHYTHM_TEMP MEMADDR(0x030110)
#define A_TIMBRE_TEMP MEMADDR(0x040000)
#define A_PATCHES     MEMADDR(0x050000)
#define A_TIMBRES     MEMADDR(0x080000)
#define A_SYSTEM      MEMADDR(0x100000)

#define MT32_TIMBRE_SIZE 246
#define TIMBRE_SZ MT32_TIMBRE_SIZE      /* 246 */
#define PADDED_SZ 256                   /* stride of timbre memory entries */

typedef struct _mt32 {
    u8 patch_temp[9][16];
    u8 rhythm_temp[85][4];
    u8 timbre_temp[8][TIMBRE_SZ];
    u8 patches[128][8];
    u8 timbres[64][TIMBRE_SZ];          /* memory bank M1-M64 */
    u8 system[23];
    int cur_bank[16];
    int cur_prog[16];
    int cur_bend[16];
} mt32_t;

static u8 mt32_rom_timbres[192][MT32_TIMBRE_SIZE];
static u8 mt32_init_patch_temp[9][16];
static u8 mt32_init_timbre_temp[8][MT32_TIMBRE_SIZE];
static u8 mt32_init_rhythm_temp[85][4];
static u8 mt32_init_patches[128][8];
static u8 mt32_init_system[23];

static u8 mt32_max_patch_temp[16];
static u8 mt32_max_rhythm_temp[4];
static u8 mt32_max_timbre_temp[MT32_TIMBRE_SIZE];
static u8 mt32_max_patches[8];
static u8 mt32_max_timbres[MT32_TIMBRE_SIZE];
static u8 mt32_max_system[23];

typedef struct {
    char hash[17];
    short bank;
    short program;
    short bend;
} mt32_preset_t;

static mt32_preset_t *mt32_presets = NULL;
static size_t num_mt32_presets = 0;
static size_t cap_mt32_presets = 0;

static void load_openmt32_sfz(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open sfz file: %s\n", path);
        exit(1);
    }

    char line[4096];
    u8 *curr_buf = NULL;
    size_t curr_cap = 0;
    size_t curr_pos = 0;
    int is_presets = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Strip newline and whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\r' || *p == '\n') continue;
        if (p[0] == '/' && p[1] == '/') continue;

        if (p[0] == '<') {
            char *end_tag = strchr(p, '>');
            if (end_tag) {
                *end_tag = '\0';
                char *tag = p + 1;
                is_presets = 0;
                curr_buf = NULL;
                curr_cap = 0;
                curr_pos = 0;

                if (!strcmp(tag, "mt32_rom_timbres")) {
                    curr_buf = (u8 *)mt32_rom_timbres; curr_cap = sizeof(mt32_rom_timbres);
                } else if (!strcmp(tag, "mt32_init_patch_temp")) {
                    curr_buf = (u8 *)mt32_init_patch_temp; curr_cap = sizeof(mt32_init_patch_temp);
                } else if (!strcmp(tag, "mt32_init_timbre_temp")) {
                    curr_buf = (u8 *)mt32_init_timbre_temp; curr_cap = sizeof(mt32_init_timbre_temp);
                } else if (!strcmp(tag, "mt32_init_rhythm_temp")) {
                    curr_buf = (u8 *)mt32_init_rhythm_temp; curr_cap = sizeof(mt32_init_rhythm_temp);
                } else if (!strcmp(tag, "mt32_init_patches")) {
                    curr_buf = (u8 *)mt32_init_patches; curr_cap = sizeof(mt32_init_patches);
                } else if (!strcmp(tag, "mt32_init_system")) {
                    curr_buf = mt32_init_system; curr_cap = sizeof(mt32_init_system);
                } else if (!strcmp(tag, "mt32_max_patch_temp")) {
                    curr_buf = mt32_max_patch_temp; curr_cap = sizeof(mt32_max_patch_temp);
                } else if (!strcmp(tag, "mt32_max_rhythm_temp")) {
                    curr_buf = mt32_max_rhythm_temp; curr_cap = sizeof(mt32_max_rhythm_temp);
                } else if (!strcmp(tag, "mt32_max_timbre_temp")) {
                    curr_buf = mt32_max_timbre_temp; curr_cap = sizeof(mt32_max_timbre_temp);
                } else if (!strcmp(tag, "mt32_max_patches")) {
                    curr_buf = mt32_max_patches; curr_cap = sizeof(mt32_max_patches);
                } else if (!strcmp(tag, "mt32_max_timbres")) {
                    curr_buf = mt32_max_timbres; curr_cap = sizeof(mt32_max_timbres);
                } else if (!strcmp(tag, "mt32_max_system")) {
                    curr_buf = mt32_max_system; curr_cap = sizeof(mt32_max_system);
                } else if (!strcmp(tag, "mt32_presets")) {
                    is_presets = 1;
                }
                continue;
            }
        }

        if (is_presets) {
            char hash_val[64] = "";
            int bank = -1, prog = -1, bend = -1;
            char *token = strtok(p, " \t\r\n");
            while (token) {
                if (!strncmp(token, "hash=", 5)) strncpy(hash_val, token + 5, sizeof(hash_val) - 1);
                else if (!strncmp(token, "bank=", 5)) bank = atoi(token + 5);
                else if (!strncmp(token, "program=", 8)) prog = atoi(token + 8);
                else if (!strncmp(token, "bend=", 5)) bend = atoi(token + 5);
                token = strtok(NULL, " \t\r\n");
            }
            if (hash_val[0] && bank >= 0 && prog >= 0) {
                if (num_mt32_presets >= cap_mt32_presets) {
                    cap_mt32_presets = cap_mt32_presets ? cap_mt32_presets * 2 : 256;
                    mt32_presets = realloc(mt32_presets, cap_mt32_presets * sizeof(*mt32_presets));
                }
                memset(mt32_presets[num_mt32_presets].hash, 0, 17);
                memcpy(mt32_presets[num_mt32_presets].hash, hash_val, 16);
                mt32_presets[num_mt32_presets].bank = bank;
                mt32_presets[num_mt32_presets].program = prog;
                mt32_presets[num_mt32_presets].bend = bend;
                num_mt32_presets++;
            }
        } else if (curr_buf) {
            char *token = strtok(p, ",\t\r\n");
            while (token) {
                while (*token == ' ' || *token == '\t') token++;
                if (*token != '\0') {
                    if (curr_pos < curr_cap) {
                        curr_buf[curr_pos++] = (u8)strtoul(token, NULL, 0);
                    }
                }
                token = strtok(NULL, ",\t\r\n");
            }
        }
    }

    fclose(f);
}

static void mt32_reset(mt32_t *m)
{
    memcpy(m->patch_temp,  mt32_init_patch_temp,  sizeof m->patch_temp);
    memcpy(m->rhythm_temp, mt32_init_rhythm_temp, sizeof m->rhythm_temp);
    memcpy(m->timbre_temp, mt32_init_timbre_temp, sizeof m->timbre_temp);
    memcpy(m->patches,     mt32_init_patches,     sizeof m->patches);
    memcpy(m->system,      mt32_init_system,      sizeof m->system);
    memset(m->timbres, 0, sizeof m->timbres);   /* munt clears bank M on open */
    for (int i = 0; i < 16; i++) { m->cur_bank[i] = m->cur_prog[i] = m->cur_bend[i] = -1; }
}

/* the timbre a patch points at: groups A/B/R come from ROM, M from memory */
static const u8 *timbre_source(const mt32_t *m, int group, int num)
{
    switch (group) {
    case 0: case 1: return mt32_rom_timbres[group * 64 + num];
    case 2: return m->timbres[num & 63];
    default: return mt32_rom_timbres[128 + num];   /* group R */
    }
}

/* Part::resetTimbre() -- reload the part's timbre from its patch reference */
static void reset_timbre(mt32_t *m, int part)
{
    if (part > 7) return;
    memcpy(m->timbre_temp[part],
           timbre_source(m, m->patch_temp[part][0] & 3,
                         m->patch_temp[part][1] & 63), TIMBRE_SZ);
}

/* Part::setProgram() -- program change selects a patch, which reloads timbre */
static void set_program(mt32_t *m, int part, int patch)
{
    if (part > 7) return;
    memcpy(m->patch_temp[part], m->patches[patch & 127], 8);
    reset_timbre(m, part);
}

/* Every byte the MT-32 accepts has a maximum; munt clamps sysex writes to
 * it and games do send out-of-range values, so clamp identically. */
static u8 clamp(u8 v, u8 max) { return v > max ? max : v; }

/* write one region, mirroring Synth::writeSysexGlobal */
static void write_region(mt32_t *m, u32 addr, const u8 *data, int len)
{
    u32 off;
    int first, last, i;

    if (addr >= A_SYSTEM && addr < A_SYSTEM + 0x100) {
        off = addr - A_SYSTEM;
        for (i = 0; i < len && off + i < sizeof m->system; i++)
            m->system[off + i] = clamp(data[i], mt32_max_system[off + i]);
        return;
    }
    if (addr >= A_TIMBRES && addr < A_TIMBRES + 64 * PADDED_SZ) {
        off = addr - A_TIMBRES;
        for (i = 0; i < len; i++, off++) {
            u32 slot = off / PADDED_SZ, in = off % PADDED_SZ;
            if (slot < 64 && in < TIMBRE_SZ)
                m->timbres[slot][in] = clamp(data[i], mt32_max_timbres[in]);
        }
        return;
    }
    if (addr >= A_PATCHES && addr < A_PATCHES + 128 * 8) {
        off = addr - A_PATCHES;
        for (i = 0; i < len && off + i < 128 * 8; i++)
            m->patches[(off + i) / 8][(off + i) % 8] =
                clamp(data[i], mt32_max_patches[(off + i) % 8]);
        return;
    }
    if (addr >= A_TIMBRE_TEMP && addr < A_TIMBRE_TEMP + 8 * TIMBRE_SZ) {
        off = addr - A_TIMBRE_TEMP;
        for (i = 0; i < len && off + i < 8 * TIMBRE_SZ; i++)
            m->timbre_temp[(off + i) / TIMBRE_SZ][(off + i) % TIMBRE_SZ] =
                clamp(data[i], mt32_max_timbre_temp[(off + i) % TIMBRE_SZ]);
        return;
    }
    if (addr >= A_RHYTHM_TEMP && addr < A_RHYTHM_TEMP + 85 * 4) {
        off = addr - A_RHYTHM_TEMP;
        for (i = 0; i < len && off + i < 85 * 4; i++)
            m->rhythm_temp[(off + i) / 4][(off + i) % 4] =
                clamp(data[i], mt32_max_rhythm_temp[(off + i) % 4]);
        return;
    }
    if (addr >= A_PATCH_TEMP && addr < A_PATCH_TEMP + 9 * 16) {
        off = addr - A_PATCH_TEMP;
        first = off / 16;
        last = (off + len - 1) / 16;
        if (last > 8) last = 8;
        for (i = 0; i < len && off + i < 9 * 16; i++)
            m->patch_temp[(off + i) / 16][(off + i) % 16] =
                clamp(data[i], mt32_max_patch_temp[(off + i) % 16]);
        /* munt: reload the timbre unless the write missed the timbre ref
           (bytes 0..2) of the first part it touched */
        for (i = first; i <= last; i++) {
            if (i == first && (off % 16) > 2) continue;
            reset_timbre(m, i);
        }
        return;
    }
    /* display, reset and everything else does not affect the sound state */
}

static void do_mt32_sysex(mt32_t *m, const u8 *p, int len)
{
    u32 addr;
    if (len < 8 || p[0] != 0x41 || p[2] != 0x16 || p[3] != 0x12) return;
    if (p[1] != 0x10) return;              /* channel-specific: not used */
    addr = MEMADDR((p[4] << 16) | (p[5] << 8) | p[6]);
    len -= 8;                              /* drop header and checksum */
    if (len < 0) return;
    write_region(m, addr, p + 7, len);
}

/* the sound identity of a note, exactly as tools/mt32_state_logger.py */
static int note_state(const mt32_t *m, int part, int key, char hash[17])
{
    sha1_t s;
    sha1_init(&s);
    if (part == 8) {
        int t;
        if (key < 24 || key > 108) return 0;
        t = m->rhythm_temp[key - 24][0];
        if (t == 127) return 0;
        sha1_update(&s, "R", 1);
        sha1_update(&s, (const u8[]){ (u8)t }, 1);
        if (t < 64) sha1_update(&s, m->timbres[t], TIMBRE_SZ);
    } else {
        sha1_update(&s, "M", 1);
        sha1_update(&s, m->patch_temp[part], 5);
        sha1_update(&s, m->timbre_temp[part], TIMBRE_SZ);
    }
    sha1_hex16(&s, hash);
    return 1;
}

static const mt32_preset_t *find_preset(const char *hash)
{
    size_t i;
    for (i = 0; i < num_mt32_presets; i++)
        if (memcmp(mt32_presets[i].hash, hash, 16) == 0) return &mt32_presets[i];
    return NULL;
}

static void mt32_scrub(void)
{
    if (config.omt_sfz_path && config.omt_sfz_path[0])
        load_openmt32_sfz(config.omt_sfz_path);
}

CONSTRUCTOR(static void init(void))
{
  register_config_scrub(mt32_scrub);
}

mt32_t *mt32remap_init(void)
{
    mt32_t *mt = malloc(sizeof(*mt));
    mt32_reset(mt);
    return mt;
}

void mt32remap_done(mt32_t *mt)
{
    free(mt);
}

int mt32remap_channel_assigned(const mt32_t *m, int ch)
{
    int part;
    for (part = 0; part < 9; part++)
        if (m->system[13 + part] == ch) return 1;
    return 0;
}

int mt32remap_noteon(mt32_t *mt, int ch, int key, int vel,
        void (*write_cb)(unsigned char *data, int len))
{
    int unmapped = 0;
    for (int part = 0; part < 9; part++) {
        char hash[17];
        int bank;
        const mt32_preset_t *p;
        if (mt->system[13 + part] != ch) continue;
        if (!note_state(mt, part, key, hash)) break;
        p = find_preset(hash);
        if (!p) {
            int k;
            unmapped++;
            printf("%d %d %d %d %s %d %d", part, ch,
                               key, vel, hash, p ? p->bank : -1,
                               p ? p->program : -1);
            putchar(' ');
            if (part == 8) {
                int t = mt->rhythm_temp[key - 24][0];
                printf("52%02x", t);
                if (t < 64)
                    for (k = 0; k < TIMBRE_SZ; k++)
                        printf("%02x", mt->timbres[t][k]);
            } else {
                printf("4d");
                for (k = 0; k < 5; k++)
                    printf("%02x", mt->patch_temp[part][k]);
                for (k = 0; k < TIMBRE_SZ; k++)
                    printf("%02x", mt->timbre_temp[part][k]);
            }
            putchar('\n');
            break;
        }
        bank = p->bank % 128;
        if (mt->cur_bank[ch] != bank || mt->cur_prog[ch] != p->program) {
            write_cb((u8 []){0xb0 | ch, 0, bank}, 3);
            write_cb((u8 []){0xc0 | ch, p->program}, 2);
            mt->cur_bank[ch] = bank; mt->cur_prog[ch] = p->program;
        }
        if (p->bend >= 0 && mt->cur_bend[ch] != p->bend) {
            int bend = p->bend > 24 ? 24 : p->bend;
            write_cb((u8 []){0xb0 | ch, 101, 0}, 3);
            write_cb((u8 []){0xb0 | ch, 100, 0}, 3);
            write_cb((u8 []){0xb0 | ch, 6, bend}, 3);
            mt->cur_bend[ch] = p->bend;
        }
        break;  // XXX
    }
    return unmapped;
}

void mt32remap_program(mt32_t *mt, int ch, int prog)
{
    int part;
    for (part = 0; part < 9; part++)
        if (mt->system[13 + part] == ch) set_program(mt, part, prog);
}

void mt32remap_sysex(mt32_t *mt, const u8 *p, int len)
{
    do_mt32_sysex(mt, p, len);
}
