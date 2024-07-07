/*
 *  Copyright (C) 2024  @stsp
 *
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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include "dosemu_debug.h"

#define EARLY_LOG_SIZE 16384
// 256Mb
#define LOG_SIZE (1024 * 1024 * 256)
static char early_log[EARLY_LOG_SIZE];
static int early_pos;
static int log_fd = -1;

static int early_printf(const char *fmt, va_list args)
{
    int wr;
    int avail = EARLY_LOG_SIZE - early_pos;
    assert(avail > 0);
    wr = vsnprintf(early_log + early_pos, avail, fmt, args);
    if (wr >= avail)
        abort();  // truncated, not enough buf size
    early_pos += wr;
    return wr;
}

int vlog_printf(const char *fmt, va_list args)
{
    int wr;
    if (log_fd == -1)
        return early_printf(fmt, args);
    wr = vdprintf(log_fd, fmt, args);
    if (lseek(log_fd, 0, SEEK_END) > LOG_SIZE) {
        int err;
        lseek(log_fd, 0, SEEK_SET);
        err = ftruncate(log_fd, 0);
        assert(!err);
    }
    return wr;
}

int vlog_init(const char *file)
{
    if (strcmp(file, "-") == 0)
        log_fd = STDERR_FILENO;
    else
        log_fd = open(file, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (log_fd == -1)
        return -1;
    if (early_pos) {
        write(log_fd, early_log, early_pos);
        early_pos = 0;
    }
    return 0;
}

int vlog_get_fd(void)
{
    assert(log_fd != -1);
    return log_fd;
}
