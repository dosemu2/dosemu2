/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef _STATS_H
#define _STATS_H

/* Statistics */

void stats_reset(void);
void stats_addcommand(int command);
void stats_addprogram(int program);
void stats_print(void);

#endif
