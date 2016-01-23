/* This code is taken from libpcl library.
 * Rip-off done by stsp for dosemu2 project.
 * Original copyrights below. */

/*
 *  PCL by Davide Libenzi (Portable Coroutine Library)
 *  Copyright (C) 2003..2010  Davide Libenzi
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include "pcl_config.h"
#include "pcl.h"
#include "pcl_private.h"

static cothread_ctx *co_get_global_ctx(void)
{
	static cothread_ctx tctx;

	if (tctx.co_curr == NULL)
		tctx.co_curr = &tctx.co_main;

	return &tctx;
}

/*
 * Simple case, the single thread one ...
 */

int co_thread_init(void)
{
	return 0;
}

void co_thread_cleanup(void)
{

}

cothread_ctx *co_get_thread_ctx(void)
{
	return co_get_global_ctx();
}
