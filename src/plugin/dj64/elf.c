/*
 *  ELF loader.
 *  Copyright (C) 2023,  @stsp
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
#include "stub.h"
#include "elf.h"
#include "elfp.h"

#define STUB_DEBUG 0
#if STUB_DEBUG
#define stub_debug(...) printf(__VA_ARGS__)
#else
#define stub_debug(...)
#endif

struct elf_h {
    uint32_t va;
    uint32_t length;
    uint32_t entry;
    int phnum;
    Elf32_Phdr phdr[0];
};

static void *read_elf_headers(int ifile)
{
    Elf32_Ehdr ehdr;
    struct elf_h *h;
    int i, rc;
    unsigned rd;
    long beg = 0, end = 0;

    rc = _dos_read(ifile, &ehdr, sizeof(ehdr), &rd); /* get the ELF header */
    if (rc || rd != sizeof(ehdr)) {
        fprintf(stderr, "cant read ELF header\n");
        return NULL;
    }
    if (memcmp(&ehdr.e_ident, ELFMAG, SELFMAG) ||
            ehdr.e_ehsize != sizeof(ehdr) ||
            ehdr.e_phentsize != sizeof(Elf32_Phdr)) {
        fprintf(stderr, "bad ELF header\n");
        return NULL;
    }
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS32) {
        fprintf(stderr, "bad ELF class %i\n", ehdr.e_ident[EI_CLASS]);
        return NULL;
    }
    if (ehdr.e_phoff > sizeof(ehdr))
        _dos_seek(ifile, ehdr.e_phoff - sizeof(ehdr), SEEK_CUR);
    h = malloc(sizeof(*h) + sizeof(Elf32_Phdr) * ehdr.e_phnum);
    assert(h);
    rc = _dos_read(ifile, h->phdr, sizeof(Elf32_Phdr) * ehdr.e_phnum, &rd);
    if (rc || rd != sizeof(Elf32_Phdr) * ehdr.e_phnum) {
        fprintf(stderr, "can't read phdr\n");
        return NULL;
    }
    h->phnum = ehdr.e_phnum;
    for (i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr *phdr = &h->phdr[i];
        if (phdr->p_type != PT_LOAD)
            continue;
        if (phdr->p_align != 4096) {
            fprintf(stderr, "unsupported ELF alignment %i\n", phdr->p_align);
            return NULL;
        }
        if (phdr->p_vaddr < beg || !beg)
            beg = phdr->p_vaddr;
        if (phdr->p_vaddr + phdr->p_memsz > end)
            end = phdr->p_vaddr + phdr->p_memsz;
#if STUB_DEBUG
        stub_debug("PHDR pa 0x%lx va 0x%lx size 0x%lx foffs 0x%lx\n",
                phdr->p_paddr, phdr->p_vaddr, phdr->p_filesz, phdr->p_offset);
#endif
    }
    h->entry = ehdr.e_entry;
    h->va = beg;
    h->length = end - beg;
    return h;
}

static uint32_t get_elf_length(void *handle)
{
    struct elf_h *h = handle;
    return h->length;
}

static uint32_t get_elf_entry(void *handle)
{
    struct elf_h *h = handle;
    return h->entry;
}

static uint32_t get_elf_va(void *handle)
{
    struct elf_h *h = handle;
    return h->va;
}

static void read_elf_sections(void *handle, char *ptr, int ifile,
        uint32_t offset)
{
    struct elf_h *h = handle;
    int i;

    for (i = 0; i < h->phnum; i++) {
        Elf32_Phdr *phdr = &h->phdr[i];
        long bytes;

        if (phdr->p_type != PT_LOAD)
            continue;
        _dos_seek(ifile, offset + phdr->p_offset, SEEK_SET);
        bytes = _long_read(ifile, ptr, phdr->p_vaddr, phdr->p_filesz);
        stub_debug("read returned %li\n", bytes);
        if (bytes != phdr->p_filesz) {
            fprintf(stderr, "err reading %i bytes, got %li\n",
                    phdr->p_filesz, bytes);
//            _exit(EXIT_FAILURE);
            goto fr;
        }
        if (phdr->p_memsz > phdr->p_filesz) {
            uint32_t len = phdr->p_memsz - phdr->p_filesz;
            len += len & 1;  // word-align
            memset(ptr + phdr->p_vaddr + phdr->p_filesz, 0, len);
        }
    }
fr:
    free(h);
}


struct ldops elf_ops = {
    read_elf_headers,
    get_elf_va,
    get_elf_length,
    get_elf_entry,
    read_elf_sections,
};
