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
#ifdef HAVE_LINUX_LANDLOCK_H
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include "landlock_priv.h"
#include "landlock.h"

#ifndef LANDLOCK_ACCESS_FS_TRUNCATE
#define LANDLOCK_ACCESS_FS_TRUNCATE 0
#endif

#define ACCESS_FILE_RW ( \
	LANDLOCK_ACCESS_FS_WRITE_FILE | \
	LANDLOCK_ACCESS_FS_READ_FILE | \
	LANDLOCK_ACCESS_FS_TRUNCATE)

#define ACCESS_RW ( \
	ACCESS_FILE_RW | \
	LANDLOCK_ACCESS_FS_READ_DIR | \
	LANDLOCK_ACCESS_FS_REMOVE_DIR | \
	LANDLOCK_ACCESS_FS_REMOVE_FILE | \
	LANDLOCK_ACCESS_FS_MAKE_DIR | \
	LANDLOCK_ACCESS_FS_MAKE_REG | \
	LANDLOCK_ACCESS_FS_MAKE_SOCK | \
	LANDLOCK_ACCESS_FS_MAKE_FIFO | \
	LANDLOCK_ACCESS_FS_MAKE_SYM | \
	LANDLOCK_ACCESS_FS_REFER)

#define ACCESS_RO ( \
	LANDLOCK_ACCESS_FS_READ_FILE | \
	LANDLOCK_ACCESS_FS_READ_DIR)

#define ACCESS_ALL ACCESS_RW

static int ruleset_fd = -1;

int landlock_init(void)
{
    struct landlock_ruleset_attr ruleset_attr = {
        .handled_access_fs = ACCESS_ALL
    };

    assert(ruleset_fd == -1);
    ruleset_fd = landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
    if (ruleset_fd < 0) {
        perror("Failed to create a ruleset");
        return -1;
    }
    return 0;
}

int landlock_allow(const char *path, int ro)
{
    int err;
    struct landlock_path_beneath_attr path_beneath = {
        .allowed_access = (ro ? ACCESS_RO : ACCESS_RW)
    };
    path_beneath.parent_fd = open(path, O_PATH | O_CLOEXEC);
    if (path_beneath.parent_fd < 0) {
        perror("Failed to open file");
        close(ruleset_fd);
        return -1;
    }
    err = landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH,
                            &path_beneath, 0);
    close(path_beneath.parent_fd);
    if (err) {
        perror("Failed to update ruleset");
        close(ruleset_fd);
        return -1;
    }
    return 0;
}

int landlock_lock(void)
{
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
        perror("Failed to restrict privileges");
        close(ruleset_fd);
        return -1;
    }
    if (landlock_restrict_self(ruleset_fd, 0)) {
        perror("Failed to enforce ruleset");
        close(ruleset_fd);
        return -1;
    }
    close(ruleset_fd);
    ruleset_fd = -1;
    return 0;
}
#endif
