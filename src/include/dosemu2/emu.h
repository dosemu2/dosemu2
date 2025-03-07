/*
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
#ifndef DOSEMU2_EMU_H
#define DOSEMU2_EMU_H

int dosemu2_emulate(int argc, char **argv, char * const *envp);
void dosemu2_set_elfload_type(int type);
void dosemu2_set_elfload_args(int argc, char **argv);
void dosemu2_set_exit_after_load(void);
void dosemu2_set_unix_path(const char *path);
void dosemu2_set_boot_cls(void);

#endif
