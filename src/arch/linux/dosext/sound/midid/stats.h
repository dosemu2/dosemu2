/* 
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Statistics */

#include<stdio.h>

void stats_reset(void);
void stats_addcommand(int command);
void stats_addprogram(int program);
void stats_print(void);

