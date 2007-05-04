/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/***********************************************************************
  Statistics
 ***********************************************************************/

#include "stats.h"
#include <stdio.h>

typedef struct Stats {
	int command[256];	/* # of calls to this command */
	int program[256];	/* # of references to this program */
} Stats;
Stats stats;			/* Statistics */

void stats_reset(void)
{
	int i;
	for (i = 0; i < 256; i++)
		stats.command[i] = 0;
	for (i = 0; i < 256; i++)
		stats.program[i] = 0;
}

void stats_addcommand(int command)
{
	if ((command >= 0) && (command < 256))
		stats.command[command]++;
}

void stats_addprogram(int program)
{
	if ((program >= 0) && (program < 256))
		stats.program[program]++;
}

void stats_print(void)
{
	int i, chn;
	printf("Command/channel times:\n");
	printf("      ");
	for (chn = 0; chn < 16; chn++)
		printf("%3i:", chn);
	printf("\n");
	for (i = 8; i < 16; i++) {
		printf("0x%2x: ", i << 4);
		for (chn = 0; chn < 16; chn++) {
			if (stats.command[(i << 4) + chn])
				printf("%4i", stats.command[(i << 4) + chn]);
			else
				printf("%4c", '.');
		}
		printf("\n");
	}
	printf("\nProgram:\n");
	for (i = 0; i < 256; i++) {
		if (stats.program[i])
			printf("  program %4i : %6i times\n", i, stats.program[i]);
	}
	printf("\n");
}

