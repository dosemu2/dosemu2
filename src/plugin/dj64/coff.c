/*
 *  COFF loader.
 *  Copyright (C) 2023,  stsp2@yandex.ru
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <dos.h>
#include "util.h"
#include "coff.h"

#define STUB_DEBUG 0
#if STUB_DEBUG
#define stub_debug(...) printf(__VA_ARGS__)
#else
#define stub_debug(...)
#endif

struct coff_header {
    unsigned short 	f_magic;	/* Magic number */
    unsigned short 	f_nscns;	/* Number of Sections */
    int32_t 		f_timdat;	/* Time & date stamp */
    int32_t 		f_symptr;	/* File pointer to Symbol Table */
    int32_t 		f_nsyms;	/* Number of Symbols */
    unsigned short 	f_opthdr;	/* sizeof(Optional Header) */
    unsigned short 	f_flags;	/* Flags */
};

struct opt_header {
    unsigned short 	magic;          /* Magic Number                    */
    unsigned short 	vstamp;         /* Version stamp                   */
    uint32_t 		tsize;          /* Text size in bytes              */
    uint32_t 		dsize;          /* Initialised data size           */
    uint32_t 		bsize;          /* Uninitialised data size         */
    uint32_t 		entry;          /* Entry point                     */
    uint32_t 		text_start;     /* Base of Text used for this file */
    uint32_t 		data_start;     /* Base of Data used for this file */
};

struct scn_header {
    char		s_name[8];	/* Section Name */
    int32_t		s_paddr;	/* Physical Address */
    int32_t		s_vaddr;	/* Virtual Address */
    int32_t		s_size;		/* Section Size in Bytes */
    int32_t		s_scnptr;	/* File offset to the Section data */
    int32_t		s_relptr;	/* File offset to the Relocation table for this Section */
    int32_t		s_lnnoptr;	/* File offset to the Line Number table for this Section */
    unsigned short	s_nreloc;	/* Number of Relocation table entries */
    unsigned short	s_nlnno;	/* Number of Line Number table entries */
    int32_t		s_flags;	/* Flags for this section */
};

enum { SCT_TEXT, SCT_DATA, SCT_BSS, SCT_MAX };

static struct scn_header scns[SCT_MAX];

struct coff_h {
    uint32_t length;
    uint32_t entry;
};

static void read_section(char *buf, int ifile, long coffset, int sc)
{
    long bytes;
    _dos_seek(ifile, coffset + scns[sc].s_scnptr, SEEK_SET);
    bytes = _long_read(ifile, buf, scns[sc].s_vaddr,
            scns[sc].s_size);
    stub_debug("read returned %li\n", bytes);
    if (bytes != scns[sc].s_size) {
        fprintf(stderr, "err reading %i bytes, got %li\n",
                scns[sc].s_size, bytes);
//        _exit(EXIT_FAILURE);
    }
}

static void *read_coff_headers(int ifile)
{
    struct coff_header chdr;
    struct opt_header ohdr;
    struct coff_h *h;
    int rc;
    unsigned rd;

    rc = _dos_read(ifile, &chdr, sizeof(chdr), &rd); /* get the COFF header */
    if (rc || rd != sizeof(chdr)) {
        fprintf(stderr, "bad COFF header\n");
        return NULL;
    }
    if (chdr.f_opthdr < sizeof(ohdr)) {
        fprintf(stderr, "opt header size mismatch: %i %zi\n",
                chdr.f_opthdr, sizeof(ohdr));
        return NULL;
    }
    rc = _dos_read(ifile, &ohdr, sizeof(ohdr), &rd); /* get the COFF opt header */
    if (rc || rd != sizeof(ohdr)) {
        fprintf(stderr, "bad COFF opt header\n");
        return NULL;
    }
    if (chdr.f_opthdr > sizeof(ohdr))
        _dos_seek(ifile, chdr.f_opthdr - sizeof(ohdr), SEEK_CUR);
    rc = _dos_read(ifile, scns, sizeof(scns[0]) * SCT_MAX, &rd);
    if (rc || rd != sizeof(scns[0]) * SCT_MAX) {
        fprintf(stderr, "failed to read section headers\n");
        return NULL;
    }
#if STUB_DEBUG
    for (int i = 0; i < SCT_MAX; i++) {
        struct scn_header *h = &scns[i];
        stub_debug("Section %s pa 0x%lx va 0x%lx size 0x%lx foffs 0x%lx\n",
                h->s_name, h->s_paddr, h->s_vaddr, h->s_size, h->s_scnptr);
    }
#endif
    h = malloc(sizeof(*h));
    assert(h);
    h->entry = ohdr.entry;
    h->length = scns[SCT_BSS].s_vaddr + scns[SCT_BSS].s_size;
    return h;
}

static uint32_t get_coff_va(void *handle)
{
    return 0; // ???
}

static uint32_t get_coff_length(void *handle)
{
    struct coff_h *h = handle;
    return h->length;
}

static uint32_t get_coff_entry(void *handle)
{
    struct coff_h *h = handle;
    return h->entry;
}

static void read_coff_sections(void *handle, char *ptr, int ifile,
        uint32_t offset)
{
    struct coff_h *h = handle;
    read_section(ptr, ifile, offset, SCT_TEXT);
    read_section(ptr, ifile, offset, SCT_DATA);
    memset(ptr + scns[SCT_BSS].s_vaddr, 0, scns[SCT_BSS].s_size +
        (scns[SCT_BSS].s_size & 1));
    free(h);
}


struct ldops coff_ops = {
    read_coff_headers,
    get_coff_va,
    get_coff_length,
    get_coff_entry,
    read_coff_sections,
};
