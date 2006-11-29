/*
 *  Copyright (C) 2006 Stas Sergeev <stsp@users.sourceforge.net>
 *
 * The below copyright strings have to be distributed unchanged together
 * with this file. This prefix can not be modified or separated.
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
#include "emu.h"
#include "init.h"
#include "sound/midi.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>


static int pipe_fd = -1;
static const char *midopipe_name = "MIDI Output: named pipe";

static int midopipe_init(void)
{
    char *name = DOSEMU_MIDI_PATH;
    mkfifo(name, 0666);
    pipe_fd =
	RPT_SYSCALL(open(name, O_WRONLY | O_CREAT | O_NONBLOCK, 0666));
    if (pipe_fd == -1) {
	S_printf("%s: unable to open %s for writing: %s\n",
		 midopipe_name, name, strerror(errno));
	return 0;
    }
    return 1;
}

static void midopipe_done(void)
{
    if (pipe_fd == -1)
	return;
    close(pipe_fd);
    pipe_fd = -1;
}

static void midopipe_reset(void)
{
}

static void midopipe_write(unsigned char val)
{
    if (pipe_fd == -1)
	return;
    write(pipe_fd, &val, 1);
}

CONSTRUCTOR(static int midopipe_register(void))
{
    struct midi_out_plugin midopipe;
    midopipe.name = midopipe_name;
    midopipe.init = midopipe_init;
    midopipe.done = midopipe_done;
    midopipe.reset = midopipe_reset;
    midopipe.write = midopipe_write;
    midopipe.stop = NULL;
    return midi_register_output_plugin(midopipe);
}
