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

/*
 * Purpose: dlmalloc's memory spaces support
 * and circumvent W^X restrictions.
 *
 * Author: @stsp
 *
 */
#include <stddef.h>
#include <assert.h>
#include <sys/mman.h>
#include "emu86.h"
#include "misc/dlmalloc.h"

#ifndef MAP_JIT
#define MAP_JIT 0
#endif
#define CODEBUF_SZ (128 * 1024 * 1024)
static mspace cspace;
static unsigned char *wbase;
static unsigned char *xbase;

void InitGenCodeBuf(void)
{
    void *addr;

#if HAVE_DECL_MREMAP_MAYMOVE
    int err;

    addr = mmap(NULL, CODEBUF_SZ, PROT_NONE,
            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(addr != MAP_FAILED);
    xbase = mremap(addr, 0, CODEBUF_SZ, MREMAP_MAYMOVE);
    assert(xbase != MAP_FAILED && xbase != addr);
    err = mprotect(addr, CODEBUF_SZ, PROT_READ | PROT_WRITE);
    assert(!err);
    err = mprotect(xbase, CODEBUF_SZ, PROT_EXEC);
    assert(!err);
#else
    addr = mmap(NULL, CODEBUF_SZ, PROT_READ | PROT_WRITE | PROT_EXEC,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
    assert(addr != MAP_FAILED);
    xbase = addr;
#endif
    wbase = addr;
    cspace = create_mspace_with_base(addr, CODEBUF_SZ, 0);
    assert(cspace);
}

void EndGenCodeBuf(void)
{
    destroy_mspace(cspace);
    if (xbase != wbase)
        munmap(xbase, CODEBUF_SZ);
    munmap(wbase, CODEBUF_SZ);
}

struct cbptr AllocGenCodeBuf(size_t size)
{
    struct cbptr ret;

    ret.ptr = mspace_malloc(cspace, size);
    assert(ret.ptr);
    ret.xptr = xbase + (ret.ptr - wbase);
    return ret;
}

void ShrinkGenCodeBuf(struct cbptr ptr, size_t size)
{
    void *ptr2 = mspace_realloc(cspace, ptr.ptr, size);
    assert(ptr2 == ptr.ptr);
}

void FreeGenCodeBuf(void *ptr)
{
    if (!ptr)
        return;
    mspace_free(cspace, ptr);
}

unsigned char *GetGenCodeBuf(const unsigned char *eip)
{
    assert(eip >= xbase && eip < xbase + CODEBUF_SZ);
    return wbase + (eip - xbase);
}

unsigned char *GetExecCodeBuf(const unsigned char *ptr)
{
    assert(ptr >= wbase && ptr < wbase + CODEBUF_SZ);
    return xbase + (ptr - wbase);
}
