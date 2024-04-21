/*
 *  go32-compatible COFF, PE32 and ELF loader stub.
 *  Copyright (C) 2022,  stsp <stsp@users.sourceforge.net>
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
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <dos.h>
#include <dpmi.h>
#include <assert.h>
#include "plugin_config.h"
#include "dosemu_debug.h"
#include "stubinfo.h"
#include "coff.h"
#include "elfp.h"
#include "util.h"
#include "stub_ex.h"
#include "stub.h"

#define STUB_DEBUG 0
#if STUB_DEBUG
#define stub_debug(...) J_printf(__VA_ARGS__)
#else
#define stub_debug(...)
#endif

typedef struct
{
    uint32_t offset32;
    unsigned short selector;
} DPMI_FP;

typedef struct
{
    uint16_t rm;
    int pm;
} dpmi_dos_block;

static char *client_memory;
static DPMI_FP clnt_entry;

static const char *_basename(const char *name)
{
    const char *p;
    p = strrchr(name, '\\');
    if (!p)
        p = name;
    else
        p++;
    return p;
}

static char *_fname(char *name)
{
    char *p, *p1;
    p = strrchr(name, '\\');
    if (!p)
        p = name;
    else
        p++;
    p1 = strrchr(p, '.');
    if (p1)
        p1[0] = '\0';
    return p;
}

#define exit(x) return x
int djstub_main(int argc, char *argv[], char *envp[], unsigned psp_sel,
    cpuctx_t *scp)
{
    int ifile;
    off_t coffset = 0;
    uint32_t coffsize = 0;
    uint32_t noffset = 0;
    uint32_t nsize = 0;
    uint32_t noffset2 = 0;
    uint32_t nsize2 = 0;
    char ovl_name[16] = {};
    int rc, i;
#define BUF_SIZE 0x40
    char buf[BUF_SIZE];
    int done = 0;
    uint32_t va;
    uint32_t va_size;
    uint32_t mem_lin;
    uint32_t mem_base;
    unsigned short clnt_ds;
    unsigned short stubinfo_fs;
    __dpmi_meminfo info;
    dpmi_dos_block db;
    void *handle;
    _GO32_StubInfo stubinfo;
    _GO32_StubInfo *stubinfo_p;
    struct ldops *ops = NULL;
    char *argv0 = strdup(argv[0]);

    rc = _dos_open(argv[0], O_RDONLY, &ifile);
    if (rc) {
        fprintf(stderr, "cannot open %s\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    while (!done) {
        unsigned rd;
#if STUB_DEBUG
        int cnt = 0;
#endif

        stub_debug("Expecting header at 0x%lx\n", coffset);
        rc = _dos_read(ifile, buf, BUF_SIZE, &rd);
        if (rc) {
            error("stub: read() failure\n");
            exit(EXIT_FAILURE);
        }
        if (rd != BUF_SIZE) {
            error("stub: read(%i)=%i, wrong exe file\n", BUF_SIZE, rd);
            exit(EXIT_FAILURE);
        }
        if (buf[0] == 'M' && buf[1] == 'Z' && buf[8] == 4 && buf[9] == 0) {
            /* lfanew */
            uint32_t offs;
#if STUB_DEBUG
            cnt++;
#endif
            stub_debug("Found exe header %i at 0x%lx\n", cnt, coffset);
            memcpy(&offs, &buf[0x3c], sizeof(offs));
            coffset = offs;
            memcpy(&coffsize, &buf[0x1c], sizeof(coffsize));
            if (coffsize)
                noffset = coffset + coffsize;
            memcpy(&nsize, &buf[0x20], sizeof(nsize));
            if (nsize)
                noffset2 = noffset + nsize;
            memcpy(&nsize2, &buf[0x24], sizeof(nsize2));
            strncpy(ovl_name, &buf[0x30], 12);
            ovl_name[12] = '\0';
        } else if (buf[0] == 0x4c && buf[1] == 0x01) { /* it's a COFF */
            done = 1;
            ops = &coff_ops;
        } else if (buf[0] == 0x7f && buf[1] == 0x45 &&
                buf[2] == 0x4c && buf[3] == 0x46) { /* it's an ELF */
            done = 1;
            ops = &elf_ops;
        } else {
            fprintf(stderr, "not an exe %s at %lx\n", argv[0], coffset);
            exit(EXIT_FAILURE);
        }
        _dos_seek(ifile, coffset, SEEK_SET);
    }

    assert(ops);
    handle = ops->read_headers(ifile);
    if (!handle)
        exit(EXIT_FAILURE);
    va = ops->get_va(handle);
    va_size = ops->get_length(handle);
    clnt_entry.offset32 = ops->get_entry(handle);
    stub_debug("va 0x%x va_size 0x%x entry 0x%x\n",
            va, va_size, clnt_entry.offset32);

    strncpy(stubinfo.magic, "dj64 (C) stsp", sizeof(stubinfo.magic));
    stubinfo.size = sizeof(stubinfo);
    i = 0;
    while(*envp) {
        i += strlen(*envp) + 1;
        envp++;
    }
    if (i) {
        i += strlen(argv0) + 1;
        i += 2;
    }
    stub_debug("env size %i\n", i);
    stubinfo.env_size = i;
    stubinfo.minstack = 0x80000;
    stubinfo.minkeep = 0x4000;
    strncpy(stubinfo.argv0, _basename(argv0), sizeof(stubinfo.argv0));
    stubinfo.argv0[sizeof(stubinfo.argv0) - 1] = '\0';
    strncpy(stubinfo.basename, _fname(argv0), sizeof(stubinfo.basename));
//    stubinfo.basename[sizeof(stubinfo.basename) - 1] = '\0';
    strncpy(stubinfo.dpmi_server, "CWSDPMI.EXE", sizeof(stubinfo.dpmi_server));
#define max(a, b) ((a) > (b) ? (a) : (b))
    stubinfo.initial_size = max(va_size, 0x10000);
    stubinfo.psp_selector = psp_sel;
    /* DJGPP relies on ds_selector, cs_selector and ds_segment all mapping
     * the same real-mode memory block. */
    _dos_link_umb(1);
    db.rm = __dpmi_allocate_dos_memory(stubinfo.minkeep >> 4, &db.pm);
    _dos_link_umb(0);
    stub_debug("rm seg 0x%x\n", db.rm);
    stubinfo.ds_selector = db.pm;
    stubinfo.ds_segment = db.rm;
    /* create alias */
    stubinfo.cs_selector = __dpmi_create_alias_descriptor(db.pm);
    /* set descriptor access rights */
    __dpmi_set_descriptor_access_rights(stubinfo.cs_selector, 0x00fb);

    clnt_entry.selector = __dpmi_allocate_ldt_descriptors(1);
    clnt_ds = __dpmi_allocate_ldt_descriptors(1);
    info.size = PAGE_ALIGN(stubinfo.initial_size);
    /* allocate mem */
    __dpmi_allocate_memory(&info);
    stubinfo.memory_handle = info.handle;
    mem_lin = info.address;
    mem_base = mem_lin - va;
    stubinfo.mem_base = mem_base;
    stub_debug("mem_lin 0x%x mem_base 0x%x\n", mem_lin, mem_base);
    /* set base */
    __dpmi_set_segment_base_address(clnt_entry.selector, mem_base);
    /* set descriptor access rights */
    __dpmi_set_descriptor_access_rights(clnt_entry.selector, 0xc0fb);
    /* set limit */
    __dpmi_set_segment_limit(clnt_entry.selector, 0xffffffff);
    /* set base */
    __dpmi_set_segment_base_address(clnt_ds, mem_base);
    /* set descriptor access rights */
    __dpmi_set_descriptor_access_rights(clnt_ds, 0xc0f3);
    /* set limit */
    __dpmi_set_segment_limit(clnt_ds, 0xffffffff);

    client_memory = SEL_ADR(clnt_ds, 0);
    stubinfo_fs = __dpmi_allocate_ldt_descriptors(1);
    info.size = PAGE_ALIGN(sizeof(_GO32_StubInfo));
    __dpmi_allocate_memory(&info);
    __dpmi_set_segment_base_address(stubinfo_fs, info.address);
    __dpmi_set_segment_limit(stubinfo_fs, sizeof(_GO32_StubInfo) - 1);
    stubinfo_p = SEL_ADR(stubinfo_fs, 0);

    ops->read_sections(handle, client_memory, ifile, coffset);

    stubinfo.self_fd = ifile;
    stubinfo.self_offs = coffset;
    stubinfo.self_size = coffsize;
    stubinfo.payload_offs = noffset;
    stubinfo.payload_size = nsize;
    stubinfo.payload2_offs = noffset2;
    stubinfo.payload2_size = nsize2;
    if (ovl_name[0]) {
        snprintf(stubinfo.payload2_name, sizeof(stubinfo.payload2_name),
                 "%s.dbg", ovl_name);
        dbug_printf("loading %s\n", ovl_name);
    }
    _dos_seek(ifile, noffset, SEEK_SET);
    if (nsize > 0)
        stub_debug("Found payload of size %i at 0x%x\n", nsize, noffset);
    stubinfo.stubinfo_ver = 2;

    memcpy(stubinfo_p, &stubinfo, sizeof(stubinfo));
    stub_debug("Jump to entry...\n");
    _fs = stubinfo_fs;
    _es = _ds;
    _ds = clnt_ds;
    _cs = clnt_entry.selector;
    _eip = clnt_entry.offset32;
    return 0;
}
