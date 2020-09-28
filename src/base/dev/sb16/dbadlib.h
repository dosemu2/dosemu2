/*
 *  Copyright (C) 2002-2011  The DOSBox Team
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


#ifndef DOSBOX_ADLIB_H
#define DOSBOX_ADLIB_H

#include <stdbool.h>
#include <stdint.h>

typedef struct _AdlibTimer AdlibTimer;

struct _AdlibTimer {
	long long start;
	long long delay;
	bool enabled, overflow, masked;
	Bit8u counter;
};

static inline void AdlibTimer__AdlibTimer(AdlibTimer *self) {
	self->masked = false;
	self->overflow = false;
	self->enabled = false;
	self->counter = 0;
	self->delay = 0;
}

//Call update before making any further changes
static inline void AdlibTimer__Update(AdlibTimer *self, long long time) {
	long long deltaStart;
	if ( !self->enabled ) 
		return;
	deltaStart = time - self->start;
	//Only set the overflow flag when not masked
	if ( deltaStart >= 0 && !self->masked ) {
		self->overflow = 1;
	}
}

//On a reset make sure the start is in sync with the next cycle
static inline void AdlibTimer__Reset(AdlibTimer *self, const long long time) {
	long long delta, rem;
	self->overflow = false;
	if ( !self->enabled )
		return;
	delta = time - self->start;
	if ( self->delay )
		rem = self->delay - delta % self->delay;
	else
		rem = 0;
	self->start = time + rem;
}

static inline void AdlibTimer__Stop(AdlibTimer *self) {
	self->enabled = false;
}

static inline void AdlibTimer__Start(AdlibTimer *self, const long long time, Bits scale) {
	//Don't enable again
	if ( self->enabled ) {
		return;
	}
	self->enabled = true;
	self->delay = (255 - self->counter ) * scale;
	self->start = time + self->delay;
}

extern struct opl_ops dbadlib_ops;

#endif
