/* adapted for dosemu2 by stsp
 * original copyrights below */

/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/* OPL implementation for OPL2LPT and OPL3LPT through libieee1284.
 */

#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <ieee1284.h>
#include "emu.h"
#include "sound/oplplug.h"

static const uint8_t OPL2LPTRegisterSelect[] = {
	(C1284_NSELECTIN | C1284_NSTROBE | C1284_NINIT) ^ C1284_INVERTED,
	(C1284_NSELECTIN | C1284_NSTROBE) ^ C1284_INVERTED,
	(C1284_NSELECTIN | C1284_NSTROBE | C1284_NINIT) ^ C1284_INVERTED
};

static const uint8_t OPL3LPTRegisterSelect[] = {
	(C1284_NSTROBE | C1284_NINIT) ^ C1284_INVERTED,
	C1284_NSTROBE ^ C1284_INVERTED,
	(C1284_NSTROBE | C1284_NINIT) ^ C1284_INVERTED
};

static const uint8_t OPL2LPTRegisterWrite[] = {
	(C1284_NSELECTIN | C1284_NINIT) ^ C1284_INVERTED,
	C1284_NSELECTIN ^ C1284_INVERTED,
	(C1284_NSELECTIN | C1284_NINIT) ^ C1284_INVERTED
};

static struct parport *_pport;
enum { Config_none, Config_kOpl2, Config_kOpl3 };
static int _type;
static int _index;

static void opl2lpt_reset(void);
static void writeReg(int r, int v);

#if 0
static void opl2lpt_done(void)
{
	if (_pport) {
//		opl2lpt_stop();
		opl2lpt_reset();
		ieee1284_close(_pport);
	}
}
#endif

static bool opl2lpt_init(void)
{
	struct parport_list parports = {};
	const char *parportName = config.opl2lpt_parport;

	if (!parportName)
		return false;
	// Look for available parallel ports
	if (ieee1284_find_ports(&parports, 0) != E1284_OK) {
		return false;
	}
	for (int i = 0; i < parports.portc; i++) {
		if (strcmp(parportName, "null") == 0 ||
		    strcmp(parportName, parports.portv[i]->name) == 0) {
			int caps = CAP1284_RAW;
			_pport = parports.portv[i];
			if (ieee1284_open(_pport, F1284_EXCL, &caps) != E1284_OK) {
				error("cannot open parallel port %s", _pport->name);
			}
			if (ieee1284_claim(_pport) != E1284_OK) {
				error("cannot claim parallel port %s", _pport->name);
				ieee1284_close(_pport);
				continue;
			}
			opl2lpt_reset();
			// Safe to free ports here, opened ports are refcounted.
			ieee1284_free_ports(&parports);
			return true;
		}
	}
	_pport = NULL;
	ieee1284_free_ports(&parports);
	return false;
}

static void opl2lpt_reset(void)
{
	for(int i = 0; i < 256; i ++) {
		writeReg(i, 0);
	}
	if (_type == Config_kOpl3) {
		for (int i = 0; i < 256; i++) {
			writeReg(i + 256, 0);
		}
	}
	_index = 0;
}

static void opl2lpt_write(void *impl, uint16_t port, uint8_t val)
{
	if (port & 1) {
		writeReg(_index, val);
	} else {
		switch (_type) {
		case Config_kOpl2:
			_index = val & 0xff;
			break;
		case Config_kOpl3:
			_index = (val & 0xff) | ((port << 7) & 0x100);
			break;
		default:
			error("OPL2LPT: unsupported OPL mode %d", _type);
			break;
		}
	}
}

static uint8_t opl2lpt_read(void *impl, uint16_t port)
{
	// No read support for the OPL2LPT
	return 0;
}

static void writeReg(int r, int v)
{
	if (_type == Config_kOpl3) {
		r &= 0x1ff;
	} else {
		r &= 0xff;
	}
	v &= 0xff;

	ieee1284_write_data(_pport, r & 0xff);
	if (r < 0x100) {
		ieee1284_write_control(_pport, OPL2LPTRegisterSelect[0]);
		ieee1284_write_control(_pport, OPL2LPTRegisterSelect[1]);
		ieee1284_write_control(_pport, OPL2LPTRegisterSelect[2]);
	} else {
		ieee1284_write_control(_pport, OPL3LPTRegisterSelect[0]);
		ieee1284_write_control(_pport, OPL3LPTRegisterSelect[1]);
		ieee1284_write_control(_pport, OPL3LPTRegisterSelect[2]);
	}
	usleep(4);		// 3.3 us

	ieee1284_write_data(_pport, v);
	ieee1284_write_control(_pport, OPL2LPTRegisterWrite[0]);
	ieee1284_write_control(_pport, OPL2LPTRegisterWrite[1]);
	ieee1284_write_control(_pport, OPL2LPTRegisterWrite[2]);
	usleep(23);
}

static void *dummy_create(int rate)
{
	return NULL;
}

static struct opl_ops opl2lpt_ops = {
    .PortRead = opl2lpt_read,
    .PortWrite = opl2lpt_write,
    .Create = dummy_create,
};

CONSTRUCTOR(static void create(void))
{
    bool ok = opl2lpt_init();
    if (ok)
	opl_register_ops(&opl2lpt_ops);
}
