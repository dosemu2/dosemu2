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
#include "../emudpmi.h"
#include "../dpmi_api.h"
#include "dpmi.h"

#define DD(r, n, a, ...) \
r ___##n a { \
  return _##n(dpmi_get_scp(), dpmi_is_32(), __VA_ARGS__); \
}
#define DDv(r, n) \
r ___##n(void) { \
  return _##n(dpmi_get_scp(), dpmi_is_32()); \
}
#define vDD(n, a, ...) \
void ___##n a { \
  _##n(dpmi_get_scp(), dpmi_is_32(), __VA_ARGS__); \
}
#define vDDv(n) \
void ___##n(void) { \
  _##n(dpmi_get_scp(), dpmi_is_32()); \
}

#include "dpmi_inc.h"
