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

#ifndef __EMS_H
#define __EMS_H

#define EMS_HELPER_EMM_INIT 0
/* increase this when ems.S is changed */
#define DOSEMU_EMS_DRIVER_VERSION 9
#define DOSEMU_EMS_DRIVER_MIN_VERSION 9

#define EMS_ERROR_DISABLED_IN_CONFIG 1
#define EMS_ERROR_VERSION_MISMATCH 2
#define EMS_ERROR_PFRAME_UNAVAIL 3
#define EMS_ERROR_ALREADY_INITIALIZED 4

#ifndef __ASSEMBLER__
void ems_init(void);
void ems_reset(void);

int emm_is_pframe_addr(dosaddr_t addr, uint32_t *size);
#endif

#endif
