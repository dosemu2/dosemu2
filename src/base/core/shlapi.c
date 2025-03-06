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
 * Author: @stsp
 *
 * External API for libdosemu2.
 */
#include <string.h>
#include "emu.h"
#include "dosemu_config.h"
#include "dos2linux.h"
#include "dosemu2/emu.h"

void dosemu2_set_elfload_type(int type)
{
    config.elfload_type = type;
}

void dosemu2_set_elfload_args(int argc, char **argv)
{
    config.elfload_argc = argc;
    config.elfload_argv = argv;
}

void dosemu2_set_exit_after_load(void)
{
    set_exit_after_load();
}

void dosemu2_set_unix_path(const char *path)
{
    free(config.unix_path);
    config.unix_path = strdup(path);
}

int dosemu2_emulate(int argc, char **argv, char * const *envp)
{
    return emulate(argc, argv, envp);
}
