/*
 *  Copyright (C) 2024  stsp
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Purpose: uffd-based VGA acceleration for remote DPMI.
 *
 * Author: @stsp
 *
 */
#include <stdio.h>
#include <fcntl.h>             /* Definition of O_* constants */
#include <sys/syscall.h>       /* Definition of SYS_* constants */
#include <linux/userfaultfd.h> /* Definition of UFFD_* constants */
#include <linux/fs.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <linux/version.h>
#endif
#include "emu.h"
#include "vgaemu.h"
#include "utilities.h"
#include "ioselect.h"
#include "searpc_config.hh"
#include "uffd.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12
#endif

static int ffds[VGAEMU_MAX_MAPPINGS];
#if HAVE_DECL_UFFD_FEATURE_WP_ASYNC
static int pagemap_fd;
#endif
static unsigned int base0;

static int uffd_create(int flags)
{
    return syscall(SYS_userfaultfd, flags);
}

static int uffd_preinit(int fd)
{
    struct uffdio_api uffdio_api;
    int err;

    uffdio_api.api = UFFD_API;
#if 0
    /* this seems broken */
    uffdio_api.features = 0;
    err = ioctl(fd, UFFDIO_API, &uffdio_api);
    if (err) {
        perror("ioctl(UFFDIO_API)");
        return err;
    }
    if (!(uffdio_api.features & UFFD_FEATURE_PAGEFAULT_FLAG_WP)) {
        fprintf(stderr, "UFFD_FEATURE_PAGEFAULT_FLAG_WP not supported\n");
        return -1;
    }
    if (!(uffdio_api.features & UFFD_FEATURE_WP_HUGETLBFS_SHMEM)) {
        fprintf(stderr, "UFFD_FEATURE_WP_HUGETLBFS_SHMEM not supported\n");
        return -1;
    }
#endif
    uffdio_api.api = UFFD_API;
    uffdio_api.features = UFFD_FEATURE_PAGEFAULT_FLAG_WP |
            UFFD_FEATURE_WP_HUGETLBFS_SHMEM;
#if HAVE_DECL_UFFD_FEATURE_WP_ASYNC
    uffdio_api.features |= UFFD_FEATURE_WP_ASYNC;
#endif
    err = ioctl(fd, UFFDIO_API, &uffdio_api);
    if (err) {
        perror("ioctl(UFFDIO_API)");
#ifdef __linux__
        if (kernel_version_code < KERNEL_VERSION(6, 7, 0))
            fprintf(stderr, "Your kernel is too old, needs v6.7+\n");
#endif
        return err;
    }
    return 0;
}

#if !HAVE_DECL_UFFD_FEATURE_WP_ASYNC
static void uffd_async(int fd, void *arg)
{
    struct uffd_msg msg;
    int rv, j;
    int i = (uintptr_t)arg;
    unsigned page_fault;

    rv = read(fd, &msg, sizeof(msg));
    if (rv != sizeof(msg) && errno != EAGAIN) {
        perror("read()");
        return;
    }
    page_fault = DOSADDR_REL((void *)(uintptr_t)msg.arg.pagefault.address) >>
            PAGE_SHIFT;

    j = page_fault - vga.mem.map[i].base_page;
    if (j >= 0 && j < vga.mem.map[i].pages) {
        int vga_page = j + vga.mem.map[i].first_page;
        vga_emu_adjust_protection(vga_page, VGA_PROT_RW, 1, 0);
    }
}

#else

static long pagemap_ioctl(int fd, void *start, int len, void *vec, int vec_len, int flag,
			  int max_pages, long required_mask, long anyof_mask, long excluded_mask,
			  long return_mask)
{
	struct pm_scan_arg arg;

	arg.start = (uintptr_t)start;
	arg.end = (uintptr_t)(start + len);
	arg.vec = (uintptr_t)vec;
	arg.vec_len = vec_len;
	arg.flags = flag;
	arg.size = sizeof(struct pm_scan_arg);
	arg.max_pages = max_pages;
	arg.category_mask = required_mask;
	arg.category_anyof_mask = anyof_mask;
	arg.category_inverted = excluded_mask;
	arg.return_mask = return_mask;

	return ioctl(fd, PAGEMAP_SCAN, &arg);
}

static int uffd_get_dirty_map(int idx, unsigned char *map)
{
    struct page_region *rgns = alloca(sizeof(struct page_region) *
            vga.mem.pages);
    dosaddr_t base = vga.mem.map[idx].base_page << PAGE_SHIFT;
    int len = vga.mem.map[idx].pages << PAGE_SHIFT;
    int i, j, cnt, ret;

    if (idx == VGAEMU_MAP_BANK_MODE &&
            base0 != vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page)
        return 0;

    ret = pagemap_ioctl(pagemap_fd, MEM_BASE32(base), len, rgns, vga.mem.pages,
            /*PM_SCAN_WP_MATCHING | */PM_SCAN_CHECK_WPASYNC,
            0, PAGE_IS_WRITTEN, 0, 0, PAGE_IS_WRITTEN);
    if (ret < 0) {
        perror("ioctl(PAGEMAP_SCAN)");
        return ret;
    }
#define LEN(region)	((region.end - region.start) / PAGE_SIZE)
    cnt = 0;
    for (i = 0; i < ret; i++) {
        int start_page = (DOSADDR_REL((void *)(uintptr_t)rgns[i].start) - base) >> PAGE_SHIFT;
        for (j = 0; j < LEN(rgns[i]); j++) {
            int page = start_page + j;
            unsigned char *bm = &map[page / 8];
            *bm |= 1 << (page % 8);
            cnt++;
        }
    }
    return cnt;
}
#endif

static int do_attach(int idx)
{
    struct uffdio_register uffdio_register;
    int err;

    if (!vga.mem.map[idx].pages)
        return 0;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_WP;
    uffdio_register.range.start = (uintptr_t)MEM_BASE32(
            vga.mem.map[idx].base_page << PAGE_SHIFT);
    uffdio_register.range.len = vga.mem.map[idx].pages << PAGE_SHIFT;
    err = ioctl(ffds[idx], UFFDIO_REGISTER, &uffdio_register);
    if (err) {
        perror("ioctl(UFFDIO_REGISTER)");
        return err;
    }
    return 0;
}

int uffd_attach(void)
{
    int err;

    err = do_attach(VGAEMU_MAP_LFB_MODE);
    if (err)
        return err;
    err = do_attach(VGAEMU_MAP_BANK_MODE);
    if (err)
        return err;
    return 0;
}

int uffd_open(int sock)
{
    int i, j, err;

    for (i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
        int err;
        int ffd = uffd_create(O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
        if (ffd == -1) {
            perror("uffd()");
            break;
        }
        err = uffd_preinit(ffd);
        if (err)
            break;
        ffds[i] = ffd;
    }
    if (i < VGAEMU_MAX_MAPPINGS)
        goto fail;
    err = uffd_attach();
    if (err)
        goto fail;
    for (j = 0; j < VGAEMU_MAX_MAPPINGS; j++)
        send_fd(sock, ffds[j]);
    return 0;

fail:
    while (--i >= 0)
        close(ffds[i]);
    return -1;
}

int uffd_init(int sock)
{
    int i;
#if HAVE_DECL_UFFD_FEATURE_WP_ASYNC
    char buf[1024];
#endif

    for (i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
        ffds[i] = recv_fd(sock);
        if (ffds[i] == -1) {
            error("uffd init failed\n");
            return -1;
        }
#if !HAVE_DECL_UFFD_FEATURE_WP_ASYNC
        add_to_io_select_threaded(ffds[i], uffd_async, (void *)(uintptr_t)i);
#endif
    }
#if HAVE_DECL_UFFD_FEATURE_WP_ASYNC
    snprintf(buf, sizeof(buf), "/proc/%i/pagemap", dpmi_pid);
    pagemap_fd = open(buf, O_RDONLY | O_CLOEXEC);
    if (pagemap_fd == -1)
        return -1;
    vgaemu_register_dirty_hook(uffd_get_dirty_map);
#endif
    base0 = vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page;
    return 0;
}

int uffd_reattach(void *addr, size_t len)
{
    int i, j;
    unsigned page_fault = DOSADDR_REL(addr) >> PAGE_SHIFT;

    for (i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
        j = page_fault - vga.mem.map[i].base_page;
        if (j >= 0 && j < vga.mem.map[i].pages) {
            do_attach(i);
            break;
        }
    }
    return 0;
}

int uffd_reinit(void *addr, size_t len)
{
    int i, j;
    unsigned page_fault = DOSADDR_REL(addr) >> PAGE_SHIFT;

    for (i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
        j = page_fault - vga.mem.map[i].base_page;
        if (j >= 0 && j < vga.mem.map[i].pages) {
            if (i == VGAEMU_MAP_BANK_MODE)
                base0 = vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page;
            break;
        }
    }
    return 0;
}

int uffd_wp(int idx, void *addr, size_t len, int wp)
{
    int err;
    struct uffdio_writeprotect wpdata;

#if HAVE_DECL_UFFD_FEATURE_WP_ASYNC
    if (!wp) {
        error("ufdd: dropping WP?\n");
        return 0;
    }
#endif
    wpdata.range.start = (uintptr_t)addr;
    wpdata.range.len = len;
    wpdata.mode = (wp ? UFFDIO_WRITEPROTECT_MODE_WP : 0);
    assert(idx < VGAEMU_MAX_MAPPINGS);
    err = ioctl(ffds[idx], UFFDIO_WRITEPROTECT, &wpdata);
    if (err) {
        perror("ioctl(UFFDIO_WRITEPROTECT)");
        error("%llx %llx\n", wpdata.range.start,wpdata.range.len);
    }
    return err;
}
