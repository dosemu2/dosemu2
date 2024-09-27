/*
 *  Copyright (C) 2024  @stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
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

#include <stdio.h>
#include "utilities.h"
#include "builtins.h"
#include "commands.h"
#include "doshelpers.h"

int emuipx_main(int argc, char **argv)
{
  // TODO!
  struct REGPACK r = REGPACK_INIT;

  r.r_ax = (DOS_HELPER_IPX_HELPER | (DOS_SUBHELPER_IPX_CONNECT << 8)) & 0xffff;
  com_intr(DOS_HELPER_INT, &r);
  if (r.r_flags & 1) {
    com_printf("IPX helper failed\n");
    return 1;
  }
  return 0;
}
