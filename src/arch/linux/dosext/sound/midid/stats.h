/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Statistics */

void stats_reset(void);
void stats_addcommand(int command);
void stats_addprogram(int program);
void stats_print(void);

