/*
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdio.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "doshelpers.h"

void demo_plugin_init(void)
{
	fprintf(stderr, "PLUGIN: demo_plugin_init called\n");
}


void demo_plugin_close(void)
{
	fprintf(stderr, "PLUGIN: demo_plugin_close called\n");
}

int demo_plugin_inte6(void)
{
	fprintf(stderr, "PLUGIN: demo_plugin_inte6 called\n");
	LWORD(eax) = 0;
	return 1;
}
