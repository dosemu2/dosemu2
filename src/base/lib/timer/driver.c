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

#include "evtimer.h"

static const struct evtimer_ops *evtimer;

int register_evtimer(const struct evtimer_ops *ops)
{
  if (evtimer)
    return 0;
  evtimer = ops;
  return 1;
}

void *evtimer_create(void (*cbk)(int ticks, void *), void *arg)
{
  return evtimer->create(cbk, arg);
}

void evtimer_delete(void *tmr)
{
  evtimer->delete(tmr);
}

void evtimer_set_rel(void *tmr, uint64_t ns, int periodic)
{
  evtimer->set_rel(tmr, ns, periodic);
}

uint64_t evtimer_gettime(void *tmr)
{
  return evtimer->gettime(tmr);
}

void evtimer_stop(void *tmr)
{
  evtimer->stop(tmr);
}

void evtimer_block(void *tmr)
{
  evtimer->block(tmr);
}

void evtimer_unblock(void *tmr)
{
  evtimer->unblock(tmr);
}
