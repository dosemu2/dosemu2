/*
 *  Copyright (C) 2023,  stsp2@yandex.ru
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
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <dos.h>
#include "util.h"

long _long_read(int file, char *buf, unsigned long offset, unsigned long size)
{
    char tmp[PAGE_SIZE];
    unsigned long done = 0;

#define min(a, b) ((a) < (b) ? (a) : (b))
    while (size) {
        unsigned todo = min(size, sizeof(tmp));
        unsigned rd;
        int err = _dos_read(file, tmp, todo, &rd);
        if (!err) {
            /* word-align memcpy */
            memcpy(buf + offset + done, tmp, rd + (rd & 1));
            done += rd;
            size -= rd;
        }
        if (rd < todo)
            break;
    }
    return done;
}
