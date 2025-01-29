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
 * Purpose: seccomp syscall filter for remote DPMI.
 *
 * Author: @stsp
 *
 * Note: Many syscalls are coming from libc and other libs.
 *       So I am not sure just how often is needed to update the list...
 */
#include <fcntl.h>
#include <seccomp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stddef.h>
#include "utilities.h"
#include "sc_filter.h"

static const int sc_list[] = {
    SCMP_SYS(close),
    SCMP_SYS(exit_group),
    SCMP_SYS(exit),
    SCMP_SYS(recv),
    SCMP_SYS(recvfrom),
    SCMP_SYS(recvmsg),
    SCMP_SYS(send),
    SCMP_SYS(sendto),
    SCMP_SYS(sendmsg),
    SCMP_SYS(read),
    SCMP_SYS(write),
    SCMP_SYS(kill),  // TODO: make sure pid is our own's
    SCMP_SYS(tgkill),
    SCMP_SYS(getpid),
    SCMP_SYS(gettid),
    SCMP_SYS(ioctl),
    SCMP_SYS(lseek),
    SCMP_SYS(futex),
    SCMP_SYS(mmap),
    SCMP_SYS(munmap),
    SCMP_SYS(mprotect),
    SCMP_SYS(madvise),
    SCMP_SYS(sigaction),
    SCMP_SYS(rt_sigaction),
    SCMP_SYS(sigprocmask),
    SCMP_SYS(rt_sigprocmask),
    SCMP_SYS(sigreturn),
    SCMP_SYS(rt_sigreturn),
    SCMP_SYS(sigaltstack),
    SCMP_SYS(prctl),
    SCMP_SYS(arch_prctl),
    SCMP_SYS(modify_ldt),
    SCMP_SYS(brk),
    SCMP_SYS(fstat),
};

int scf_start(void)
{
    int rc = -1;
    scmp_filter_ctx ctx;
    int i;

    ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
    if (ctx == NULL)
        return -1;

    for (i = 0; i < ARRAY_SIZE(sc_list); i++) {
        rc = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, sc_list[i], 0);
        if (rc < 0)
            goto out;
    }

    rc = seccomp_load(ctx);

  out:
    seccomp_release(ctx);
    return rc;
}
